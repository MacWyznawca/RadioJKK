/* RadioJKK32 - MQTT Integration for Home Assistant
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 *
 * MQTT client: mDNS broker discovery, HA device-based discovery,
 * state publishing, command handling.
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_app_desc.h"
#include "mqtt_client.h"
#include "mdns.h"
#include "cJSON.h"

#include "jkk_mqtt.h"
#include "jkk_radio.h"
#include "jkk_nvs.h"

#ifdef CONFIG_JKK_RADIO_USING_I2C_LCD
#include "display/jkk_lcd_port.h"
#endif

static const char *TAG = "JKK_MQTT";

/* ── Identifiers & topics ────────────────────────────────── */

#define MQTT_TOPIC_PREFIX    "rjkk"
#define MQTT_UID_PREFIX      "rjkk_"
#define MQTT_DEFAULT_PORT    1883

// NVS
#define MQTT_NVS_NAMESPACE   JKK_RADIO_NVS_NAMESPACE

// mDNS broker instance names, in priority order (first match wins)
static const char *mdns_broker_priority[] = {
    "LuBASE", "lupanel", "homeassistant"
};
#define MDNS_BROKER_PRIORITY_COUNT (sizeof(mdns_broker_priority) / sizeof(mdns_broker_priority[0]))

/* ── State ───────────────────────────────────────────────── */

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;
static bool s_mqtt_enabled = true;           // RAM cache (default: enabled)
static char s_broker_addr[80] = "";         // RAM cache for broker address
static char s_mqtt_user[33]  = "";          // RAM cache for MQTT username
static char s_mqtt_pass[65]  = "";          // RAM cache for MQTT password
static char s_uid[20]       = "";   // "rjkk_AABBCCDDEEFF"
static char s_mac_id[14]    = "";   // "AABBCCDDEEFF"
static char s_mac_str[18]   = "";   // "AA:BB:CC:DD:EE:FF"
static char s_topic_state[48] = ""; // "rjkk/AABBCCDDEEFF/state"
static char s_topic_set[48]   = ""; // "rjkk/AABBCCDDEEFF/set"
static char s_topic_avty[48]  = ""; // "rjkk/AABBCCDDEEFF/avty"
static char s_topic_media_cmd[48] = ""; // "rjkk/AABBCCDDEEFF/media_cmd"
static char s_topic_vol_cmd[48]   = ""; // "rjkk/AABBCCDDEEFF/vol_cmd"

/* ── Forward declarations ────────────────────────────────── */

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);
static void mqtt_publish_discovery(void);
static void mqtt_handle_command(const char *payload, int len);
static esp_err_t mqtt_start_client(void);

/* ── Helpers ─────────────────────────────────────────────── */

static void build_identifiers(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    snprintf(s_mac_id, sizeof(s_mac_id), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    snprintf(s_mac_str, sizeof(s_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    snprintf(s_uid, sizeof(s_uid), "%s%s", MQTT_UID_PREFIX, s_mac_id);

    snprintf(s_topic_state, sizeof(s_topic_state), "%s/%s/state", MQTT_TOPIC_PREFIX, s_mac_id);
    snprintf(s_topic_set,   sizeof(s_topic_set),   "%s/%s/set",   MQTT_TOPIC_PREFIX, s_mac_id);
    snprintf(s_topic_avty,  sizeof(s_topic_avty),  "%s/%s/avty",  MQTT_TOPIC_PREFIX, s_mac_id);
    snprintf(s_topic_media_cmd, sizeof(s_topic_media_cmd), "%s/%s/media_cmd", MQTT_TOPIC_PREFIX, s_mac_id);
    snprintf(s_topic_vol_cmd,   sizeof(s_topic_vol_cmd),   "%s/%s/vol_cmd",   MQTT_TOPIC_PREFIX, s_mac_id);
}

/* ── mDNS broker discovery ───────────────────────────────── */

/**
 * Search for _mqtt._tcp mDNS service.
 * Returns true if broker found, fills host/port.
 */
/**
 * Extract IPv4 address from mDNS result into host buffer.
 * Returns true on success.
 */
static bool mdns_result_to_host(mdns_result_t *r, char *host, size_t host_len, uint16_t *port)
{
    /* Prefer IPv4 address */
    for (mdns_ip_addr_t *a = r->addr; a != NULL; a = a->next) {
        if (a->addr.type == ESP_IPADDR_TYPE_V4) {
            snprintf(host, host_len, IPSTR, IP2STR(&a->addr.u_addr.ip4));
            *port = r->port ? r->port : MQTT_DEFAULT_PORT;
            return true;
        }
    }
    /* Fallback to hostname */
    if (r->hostname && strlen(r->hostname) > 0) {
        strlcpy(host, r->hostname, host_len);
        *port = r->port ? r->port : MQTT_DEFAULT_PORT;
        return true;
    }
    return false;
}

static bool mdns_discover_broker(char *host, size_t host_len, uint16_t *port)
{
    /* Ensure mDNS is initialized (idempotent — returns error if already inited) */
    mdns_init();

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr("_mqtt", "_tcp", 4000, 20, &results);
    if (err != ESP_OK || results == NULL) {
        ESP_LOGI(TAG, "mDNS: brak wyników _mqtt._tcp");
        return false;
    }

    /* Pass 1: search by priority names (LuBASE > lupanel > homeassistant) */
    for (int pri = 0; pri < (int)MDNS_BROKER_PRIORITY_COUNT; pri++) {
        for (mdns_result_t *r = results; r != NULL; r = r->next) {
            if (!r->instance_name) continue;
            if (strcasecmp(r->instance_name, mdns_broker_priority[pri]) == 0) {
                if (mdns_result_to_host(r, host, host_len, port)) {
                    ESP_LOGI(TAG, "mDNS: broker '%s' (pri %d) -> %s:%d",
                             r->instance_name, pri, host, *port);
                    mdns_query_results_free(results);
                    return true;
                }
            }
        }
    }

    /* Pass 2: fallback — accept any _mqtt._tcp service */
    for (mdns_result_t *r = results; r != NULL; r = r->next) {
        if (mdns_result_to_host(r, host, host_len, port)) {
            ESP_LOGI(TAG, "mDNS: broker (dowolny) '%s' -> %s:%d",
                     r->instance_name ? r->instance_name : "?", host, *port);
            mdns_query_results_free(results);
            return true;
        }
    }

    mdns_query_results_free(results);
    ESP_LOGI(TAG, "mDNS: znaleziono wyniki ale żaden nie ma adresu");
    return false;
}

/* ── NVS broker address ──────────────────────────────────── */

esp_err_t JkkMqttSetBrokerAddress(const char *address)
{
    /* Update RAM cache */
    if (address && strlen(address) > 0) {
        strlcpy(s_broker_addr, address, sizeof(s_broker_addr));
    } else {
        s_broker_addr[0] = '\0';
    }
    /* Persist to NVS — call only from internal-RAM task (event handler) */
    if (!address || strlen(address) == 0) {
        return JkkNvsErase(JKK_MQTT_NVS_KEY_BROKER, MQTT_NVS_NAMESPACE);
    }
    return JkkNvsBlobSet(JKK_MQTT_NVS_KEY_BROKER, MQTT_NVS_NAMESPACE,
                         address, strlen(address) + 1);
}

esp_err_t JkkMqttGetBrokerAddress(char *address, size_t len)
{
    /* Return cached value — safe to call from PSRAM httpd task */
    if (s_broker_addr[0] != '\0') {
        strlcpy(address, s_broker_addr, len);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

/* ── NVS MQTT credentials ────────────────────────────────── */

esp_err_t JkkMqttSetCredentials(const char *user, const char *pass)
{
    /* Update RAM cache */
    if (user && strlen(user) > 0)
        strlcpy(s_mqtt_user, user, sizeof(s_mqtt_user));
    else
        s_mqtt_user[0] = '\0';

    if (pass && strlen(pass) > 0)
        strlcpy(s_mqtt_pass, pass, sizeof(s_mqtt_pass));
    else
        s_mqtt_pass[0] = '\0';

    /* Persist to NVS */
    if (s_mqtt_user[0])
        JkkNvsBlobSet(JKK_MQTT_NVS_KEY_USER, MQTT_NVS_NAMESPACE, s_mqtt_user, strlen(s_mqtt_user) + 1);
    else
        JkkNvsErase(JKK_MQTT_NVS_KEY_USER, MQTT_NVS_NAMESPACE);

    if (s_mqtt_pass[0])
        JkkNvsBlobSet(JKK_MQTT_NVS_KEY_PASS, MQTT_NVS_NAMESPACE, s_mqtt_pass, strlen(s_mqtt_pass) + 1);
    else
        JkkNvsErase(JKK_MQTT_NVS_KEY_PASS, MQTT_NVS_NAMESPACE);

    return ESP_OK;
}

esp_err_t JkkMqttGetUsername(char *buf, size_t len)
{
    if (s_mqtt_user[0] != '\0') {
        strlcpy(buf, s_mqtt_user, len);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t JkkMqttGetPassword(char *buf, size_t len)
{
    if (s_mqtt_pass[0] != '\0') {
        strlcpy(buf, s_mqtt_pass, len);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

/* ── Discovery JSON (cJSON) ──────────────────────────────── */

static char *build_discovery_json(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    const char *fw = app->version;

    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    /* ── device ── */
    cJSON *dev = cJSON_AddObjectToObject(root, "dev");
    cJSON *ids = cJSON_AddArrayToObject(dev, "ids");
    cJSON_AddItemToArray(ids, cJSON_CreateString(s_uid));
    char dev_name[32];
    snprintf(dev_name, sizeof(dev_name), "RadioJKK %s", &s_mac_id[8]); // last 4 hex chars
    cJSON_AddStringToObject(dev, "name", dev_name);
    cJSON_AddStringToObject(dev, "mf", "JKK");
    cJSON_AddStringToObject(dev, "mdl", "ESP32-A1S Radio");
    cJSON_AddStringToObject(dev, "sw", fw);
    cJSON_AddStringToObject(dev, "sn", s_mac_str);

    /* ── origin ── */
    cJSON *origin = cJSON_AddObjectToObject(root, "o");
    cJSON_AddStringToObject(origin, "name", "RadioJKK");
    cJSON_AddStringToObject(origin, "sw", fw);

    /* ── availability ── */
    cJSON_AddStringToObject(root, "avty_t", s_topic_avty);

    /* ── components ── */
    cJSON *cmps = cJSON_AddObjectToObject(root, "cmps");

    /* ---- select: station (dropdown with names) ---- */
    {
        char key[28]; snprintf(key, sizeof(key), "O%sst", s_uid);
        cJSON *st = cJSON_AddObjectToObject(cmps, key);
        cJSON_AddStringToObject(st, "p", "select");
        cJSON_AddStringToObject(st, "name", "Station");
        char uid[32]; snprintf(uid, sizeof(uid), "%s_st", s_uid);
        cJSON_AddStringToObject(st, "unique_id", uid);
        cJSON_AddStringToObject(st, "stat_t", s_topic_state);
        cJSON_AddStringToObject(st, "cmd_t", s_topic_set);
        cJSON_AddStringToObject(st, "val_tpl", "{{ value_json.station_name }}");
        cJSON_AddStringToObject(st, "cmd_tpl", "{\"station_name\":{{ value | tojson }}}");
        cJSON *st_opts = cJSON_AddArrayToObject(st, "options");
        int st_count = JkkRadioGetStationCount();
        for (int i = 0; i < st_count; i++) {
            const char *sname = JkkRadioGetStationName(i);
            if (sname && strlen(sname) > 0) {
                cJSON_AddItemToArray(st_opts, cJSON_CreateString(sname));
            }
        }
        cJSON_AddStringToObject(st, "ic", "mdi:radio");
    }

    /* ---- select: equalizer (dropdown with names) ---- */
    {
        char key[28]; snprintf(key, sizeof(key), "O%seq", s_uid);
        cJSON *eq = cJSON_AddObjectToObject(cmps, key);
        cJSON_AddStringToObject(eq, "p", "select");
        cJSON_AddStringToObject(eq, "name", "Equalizer");
        char uid[32]; snprintf(uid, sizeof(uid), "%s_eq", s_uid);
        cJSON_AddStringToObject(eq, "unique_id", uid);
        cJSON_AddStringToObject(eq, "stat_t", s_topic_state);
        cJSON_AddStringToObject(eq, "cmd_t", s_topic_set);
        cJSON_AddStringToObject(eq, "val_tpl", "{{ value_json.eq_name }}");
        cJSON_AddStringToObject(eq, "cmd_tpl", "{\"eq_name\":{{ value | tojson }}}");
        cJSON *eq_opts = cJSON_AddArrayToObject(eq, "options");
        int eq_count = JkkRadioGetEqCount();
        for (int i = 0; i < eq_count; i++) {
            const char *ename = JkkRadioGetEqName(i);
            if (ename && strlen(ename) > 0) {
                cJSON_AddItemToArray(eq_opts, cJSON_CreateString(ename));
            }
        }
        cJSON_AddStringToObject(eq, "ic", "mdi:equalizer");
    }

    /* ---- number: volume ---- */
    {
        char key[28]; snprintf(key, sizeof(key), "O%svol", s_uid);
        cJSON *vol = cJSON_AddObjectToObject(cmps, key);
        cJSON_AddStringToObject(vol, "p", "number");
        cJSON_AddStringToObject(vol, "name", "Volume");
        char uid[32]; snprintf(uid, sizeof(uid), "%s_vol", s_uid);
        cJSON_AddStringToObject(vol, "unique_id", uid);
        cJSON_AddStringToObject(vol, "stat_t", s_topic_state);
        cJSON_AddStringToObject(vol, "cmd_t", s_topic_set);
        cJSON_AddStringToObject(vol, "val_tpl", "{{ value_json.vol }}");
        cJSON_AddStringToObject(vol, "cmd_tpl", "{\"vol\":{{ value | int }}}");
        cJSON_AddNumberToObject(vol, "min", 0);
        cJSON_AddNumberToObject(vol, "max", 100);
        cJSON_AddNumberToObject(vol, "step", 1);
        cJSON_AddStringToObject(vol, "ic", "mdi:volume-high");
    }

    /* ---- switch: recording ---- */
    {
        char key[28]; snprintf(key, sizeof(key), "O%srec", s_uid);
        cJSON *rec = cJSON_AddObjectToObject(cmps, key);
        cJSON_AddStringToObject(rec, "p", "switch");
        cJSON_AddStringToObject(rec, "name", "Nagrywanie");
        char uid[32]; snprintf(uid, sizeof(uid), "%s_rec", s_uid);
        cJSON_AddStringToObject(rec, "unique_id", uid);
        cJSON_AddStringToObject(rec, "stat_t", s_topic_state);
        cJSON_AddStringToObject(rec, "cmd_t", s_topic_set);
        cJSON_AddStringToObject(rec, "val_tpl",
                                "{{ 'ON' if value_json.rec else 'OFF' }}");
        cJSON_AddStringToObject(rec, "stat_on", "ON");
        cJSON_AddStringToObject(rec, "stat_off", "OFF");
        cJSON_AddStringToObject(rec, "pl_on",  "{\"rec\":1}");
        cJSON_AddStringToObject(rec, "pl_off", "{\"rec\":0}");
        cJSON_AddStringToObject(rec, "ic", "mdi:record-rec");
    }

    /* ---- switch: LCD ---- */
    {
        char key[28]; snprintf(key, sizeof(key), "O%slcd", s_uid);
        cJSON *lcd = cJSON_AddObjectToObject(cmps, key);
        cJSON_AddStringToObject(lcd, "p", "switch");
        cJSON_AddStringToObject(lcd, "name", "LCD");
        char uid[32]; snprintf(uid, sizeof(uid), "%s_lcd", s_uid);
        cJSON_AddStringToObject(lcd, "unique_id", uid);
        cJSON_AddStringToObject(lcd, "stat_t", s_topic_state);
        cJSON_AddStringToObject(lcd, "cmd_t", s_topic_set);
        cJSON_AddStringToObject(lcd, "val_tpl",
                                "{{ 'ON' if value_json.lcd else 'OFF' }}");
        cJSON_AddStringToObject(lcd, "stat_on", "ON");
        cJSON_AddStringToObject(lcd, "stat_off", "OFF");
        cJSON_AddStringToObject(lcd, "pl_on",  "{\"lcd\":1}");
        cJSON_AddStringToObject(lcd, "pl_off", "{\"lcd\":0}");
        cJSON_AddStringToObject(lcd, "ic", "mdi:monitor");
    }

    /* ---- switch: Odtwarzanie (Play/Stop) ---- */
    {
        char key[28]; snprintf(key, sizeof(key), "O%splay", s_uid);
        cJSON *sw = cJSON_AddObjectToObject(cmps, key);
        cJSON_AddStringToObject(sw, "p", "switch");
        cJSON_AddStringToObject(sw, "name", "Odtwarzanie");
        char uid[32]; snprintf(uid, sizeof(uid), "%s_play", s_uid);
        cJSON_AddStringToObject(sw, "unique_id", uid);
        cJSON_AddStringToObject(sw, "stat_t", s_topic_state);
        cJSON_AddStringToObject(sw, "cmd_t", s_topic_set);
        cJSON_AddStringToObject(sw, "val_tpl",
                                "{{ 'ON' if value_json.state == 'playing' else 'OFF' }}");
        cJSON_AddStringToObject(sw, "stat_on", "ON");
        cJSON_AddStringToObject(sw, "stat_off", "OFF");
        cJSON_AddStringToObject(sw, "pl_on",  "{\"state\":\"play\"}");
        cJSON_AddStringToObject(sw, "pl_off", "{\"state\":\"stop\"}");
        cJSON_AddStringToObject(sw, "ic", "mdi:play-pause");
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str; // caller must free()
}

/* ── State JSON ──────────────────────────────────────────── */

static char *build_state_json(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    /* map audio state to HA media_player state strings */
    const char *state_str;
    jkk_audio_state_t astate = JkkAudioGetState();
    switch (astate) {
        case JKK_AUDIO_STATE_PLAYING:  state_str = "playing"; break;
        case JKK_AUDIO_STATE_PAUSED:   state_str = "paused";  break;
        default:                        state_str = "idle";    break;
    }
    cJSON_AddStringToObject(root, "state", state_str);
    cJSON_AddNumberToObject(root, "vol", JkkRadioGetVolume());
    cJSON_AddNumberToObject(root, "station", JkkRadioGetStation());

    const char *sname = JkkRadioGetStationName(JkkRadioGetStation());
    cJSON_AddStringToObject(root, "station_name", sname ? sname : "");

    cJSON_AddNumberToObject(root, "eq", JkkRadioGetEq());

    const char *ename = JkkRadioGetEqName(JkkRadioGetEq());
    cJSON_AddStringToObject(root, "eq_name", ename ? ename : "");

    cJSON_AddBoolToObject(root, "rec", JkkRadioIsRecording());

    bool lcd_on = false;
#ifdef CONFIG_JKK_RADIO_USING_I2C_LCD
    lcd_on = JkkLcdPortGetLcdState();
#endif
    cJSON_AddBoolToObject(root, "lcd", lcd_on);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

/* ── Command handler ─────────────────────────────────────── */

static void mqtt_handle_command(const char *payload, int len)
{
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (!root) {
        ESP_LOGW(TAG, "Nieudany parse komendy MQTT");
        return;
    }

    /* state: play / pause / stop */
    cJSON *j_state = cJSON_GetObjectItem(root, "state");
    if (j_state && cJSON_IsString(j_state)) {
        const char *cmd = j_state->valuestring;
        if (strcmp(cmd, "play") == 0) {
            JkkRadioSendMessageToMain(0, JKK_RADIO_CMD_PLAY);
        } else if (strcmp(cmd, "pause") == 0) {
            JkkRadioSendMessageToMain(0, JKK_RADIO_CMD_PAUSE);
        } else if (strcmp(cmd, "stop") == 0) {
            JkkRadioSendMessageToMain(0, JKK_RADIO_CMD_STOP);
        } else if (strcmp(cmd, "next") == 0) {
            int next = JkkRadioGetStation() + 1;
            if (next >= JkkRadioGetStationCount()) next = 0;
            JkkRadioSendMessageToMain(next, JKK_RADIO_CMD_SET_STATION);
        } else if (strcmp(cmd, "prev") == 0) {
            int prev = JkkRadioGetStation() - 1;
            if (prev < 0) prev = JkkRadioGetStationCount() - 1;
            JkkRadioSendMessageToMain(prev, JKK_RADIO_CMD_SET_STATION);
        }
    }

    /* vol: 0-100 */
    cJSON *j_vol = cJSON_GetObjectItem(root, "vol");
    if (j_vol && cJSON_IsNumber(j_vol)) {
        int vol = j_vol->valueint;
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;
        JkkRadioSetVolume((uint8_t)vol);
    }

    /* station: index */
    cJSON *j_station = cJSON_GetObjectItem(root, "station");
    if (j_station && cJSON_IsNumber(j_station)) {
        int st = j_station->valueint;
        if (st >= 0 && st < JkkRadioGetStationCount()) {
            JkkRadioSendMessageToMain(st, JKK_RADIO_CMD_SET_STATION);
        }
    }

    /* station_name: from select entity (lookup name → index) */
    cJSON *j_stn = cJSON_GetObjectItem(root, "station_name");
    if (j_stn && cJSON_IsString(j_stn)) {
        const char *name = j_stn->valuestring;
        int count = JkkRadioGetStationCount();
        for (int i = 0; i < count; i++) {
            const char *sname = JkkRadioGetStationName(i);
            if (sname && strcmp(sname, name) == 0) {
                JkkRadioSendMessageToMain(i, JKK_RADIO_CMD_SET_STATION);
                break;
            }
        }
    }

    /* eq: index */
    cJSON *j_eq = cJSON_GetObjectItem(root, "eq");
    if (j_eq && cJSON_IsNumber(j_eq)) {
        int eq = j_eq->valueint;
        if (eq >= 0 && eq < JkkRadioGetEqCount()) {
            JkkRadioSendMessageToMain(eq, JKK_RADIO_CMD_SET_EQUALIZER);
        }
    }

    /* eq_name: from select entity (lookup name → index) */
    cJSON *j_eqn = cJSON_GetObjectItem(root, "eq_name");
    if (j_eqn && cJSON_IsString(j_eqn)) {
        const char *name = j_eqn->valuestring;
        int count = JkkRadioGetEqCount();
        for (int i = 0; i < count; i++) {
            const char *ename = JkkRadioGetEqName(i);
            if (ename && strcmp(ename, name) == 0) {
                JkkRadioSendMessageToMain(i, JKK_RADIO_CMD_SET_EQUALIZER);
                break;
            }
        }
    }

    /* rec: 0/1 */
    cJSON *j_rec = cJSON_GetObjectItem(root, "rec");
    if (j_rec && cJSON_IsNumber(j_rec)) {
        bool want_rec = (j_rec->valueint != 0);
        bool is_rec = JkkRadioIsRecording();
        if (want_rec != is_rec) {
            JkkRadioSendMessageToMain(JKK_RADIO_CMD_TOGGLE_RECORD, JKK_RADIO_CMD_TOGGLE_RECORD);
        }
    }

    /* lcd: 0/1 */
    cJSON *j_lcd = cJSON_GetObjectItem(root, "lcd");
    if (j_lcd && cJSON_IsNumber(j_lcd)) {
#ifdef CONFIG_JKK_RADIO_USING_I2C_LCD
        bool want_on = (j_lcd->valueint != 0);
        bool is_on = JkkLcdPortGetLcdState();
        if (want_on && !is_on) {
            JkkRadioLcdOn();
        } else if (!want_on && is_on) {
            JkkLcdPortOnOffLcd(false);
        }
#endif
    }

    cJSON_Delete(root);

    /* Publish updated state after processing commands */
    vTaskDelay(pdMS_TO_TICKS(200));
    JkkMqttPublishState();
}

/* ── Discovery publish ───────────────────────────────────── */

static void mqtt_publish_discovery(void)
{
    char topic[80];
    int msg_id;

    /* Device-based discovery (select, number, switch, button) */
    char *json = build_discovery_json();
    if (json) {
        snprintf(topic, sizeof(topic), "homeassistant/device/%s/config", s_uid);
        msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, json, 0, 1, 1);
        ESP_LOGI(TAG, "Device discovery -> %s (msg_id=%d, len=%d)", topic, msg_id, (int)strlen(json));
        free(json);
    } else {
        ESP_LOGE(TAG, "Błąd budowy device discovery JSON");
    }

    /* Clear old media_player discovery (was never valid, cleanup retained) */
    snprintf(topic, sizeof(topic), "homeassistant/media_player/%s/config", s_uid);
    esp_mqtt_client_publish(s_mqtt_client, topic, "", 0, 1, 1);
}

/* ── MQTT event handler ──────────────────────────────────── */

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)data;

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT połączony z brokerem");
        s_mqtt_connected = true;

        /* publish availability: online (retained) */
        esp_mqtt_client_publish(s_mqtt_client, s_topic_avty, "online", 0, 1, 1);

        /* publish HA discovery */
        mqtt_publish_discovery();

        /* subscribe to command topic */
        esp_mqtt_client_subscribe(s_mqtt_client, s_topic_set, 1);

        /* subscribe to media_player command topics */
        esp_mqtt_client_subscribe(s_mqtt_client, s_topic_media_cmd, 1);
        esp_mqtt_client_subscribe(s_mqtt_client, s_topic_vol_cmd, 1);

        /* subscribe to HA status topic (for re-discovery on HA restart) */
        esp_mqtt_client_subscribe(s_mqtt_client, "homeassistant/status", 0);

        /* publish current state */
        vTaskDelay(pdMS_TO_TICKS(500));
        JkkMqttPublishState();
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT rozłączony");
        s_mqtt_connected = false;
        break;

    case MQTT_EVENT_DATA:
        if (event->topic_len > 0 && event->data_len > 0) {
            /* Check if this is our set topic (JSON commands) */
            if (event->topic_len == (int)strlen(s_topic_set) &&
                strncmp(event->topic, s_topic_set, event->topic_len) == 0) {
                mqtt_handle_command(event->data, event->data_len);
            }
            /* media_player command topic (plain strings from HA) */
            else if (event->topic_len == (int)strlen(s_topic_media_cmd) &&
                     strncmp(event->topic, s_topic_media_cmd, event->topic_len) == 0) {
                char cmd[32] = "";
                int clen = event->data_len < 31 ? event->data_len : 31;
                memcpy(cmd, event->data, clen);
                cmd[clen] = '\0';
                ESP_LOGI(TAG, "media_cmd: %s", cmd);
                /* HA MQTT media_player sends uppercase by default:
                 * PLAY, PAUSE, STOP, NEXT, PREVIOUS
                 * Also accept legacy lowercase variants. */
                if (strcasecmp(cmd, "PLAY") == 0 ||
                    strcmp(cmd, "media_play") == 0) {
                    JkkRadioSendMessageToMain(0, JKK_RADIO_CMD_PLAY);
                } else if (strcasecmp(cmd, "PAUSE") == 0 ||
                           strcmp(cmd, "media_pause") == 0) {
                    JkkRadioSendMessageToMain(0, JKK_RADIO_CMD_PAUSE);
                } else if (strcasecmp(cmd, "STOP") == 0 ||
                           strcmp(cmd, "media_stop") == 0) {
                    JkkRadioSendMessageToMain(0, JKK_RADIO_CMD_STOP);
                } else if (strcasecmp(cmd, "NEXT") == 0 ||
                           strcmp(cmd, "media_next_track") == 0) {
                    int next = JkkRadioGetStation() + 1;
                    if (next >= JkkRadioGetStationCount()) next = 0;
                    JkkRadioSendMessageToMain(next, JKK_RADIO_CMD_SET_STATION);
                } else if (strcasecmp(cmd, "PREVIOUS") == 0 ||
                           strcmp(cmd, "media_previous_track") == 0) {
                    int prev = JkkRadioGetStation() - 1;
                    if (prev < 0) prev = JkkRadioGetStationCount() - 1;
                    JkkRadioSendMessageToMain(prev, JKK_RADIO_CMD_SET_STATION);
                } else {
                    ESP_LOGW(TAG, "Nieznana komenda media_cmd: %s", cmd);
                }
                vTaskDelay(pdMS_TO_TICKS(200));
                JkkMqttPublishState();
            }
            /* vol_cmd topic (float 0.0-1.0 from HA) */
            else if (event->topic_len == (int)strlen(s_topic_vol_cmd) &&
                     strncmp(event->topic, s_topic_vol_cmd, event->topic_len) == 0) {
                char vbuf[16] = "";
                int vlen = event->data_len < 15 ? event->data_len : 15;
                memcpy(vbuf, event->data, vlen);
                vbuf[vlen] = '\0';
                float fvol = strtof(vbuf, NULL);
                int vol = (int)(fvol * 100.0f + 0.5f);
                if (vol < 0) vol = 0;
                if (vol > 100) vol = 100;
                ESP_LOGI(TAG, "vol_cmd: %s -> %d", vbuf, vol);
                JkkRadioSetVolume((uint8_t)vol);
            }
            /* Check for HA birth message (re-publish discovery) */
            else if (strncmp(event->topic, "homeassistant/status", event->topic_len) == 0) {
                if (event->data_len >= 6 && strncmp(event->data, "online", 6) == 0) {
                    ESP_LOGI(TAG, "HA restart -> re-publish discovery");
                    mqtt_publish_discovery();
                    vTaskDelay(pdMS_TO_TICKS(300));
                    JkkMqttPublishState();
                }
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT błąd");
        break;

    default:
        break;
    }
}

/* ── Public API ──────────────────────────────────────────── */

bool JkkMqttIsConnected(void)
{
    return s_mqtt_connected;
}

void JkkMqttPublishState(void)
{
    if (!s_mqtt_connected || !s_mqtt_client) return;

    char *json = build_state_json();
    if (!json) return;

    esp_mqtt_client_publish(s_mqtt_client, s_topic_state, json, 0, 1, 1); // QoS 1, retain
    free(json);
}

void JkkMqttPublishDiscovery(void)
{
    if (!s_mqtt_connected || !s_mqtt_client) return;
    mqtt_publish_discovery();
    ESP_LOGI(TAG, "Re-published HA discovery (station list changed)");
}

/* ── Enable/Disable ──────────────────────────────────────── */

esp_err_t JkkMqttSetEnabled(bool enabled)
{
    s_mqtt_enabled = enabled;  /* Update RAM cache */
    uint64_t val = enabled ? 1 : 0;
    /* Persist to NVS — call only from internal-RAM task (event handler) */
    return JkkNvs64_set(JKK_MQTT_NVS_KEY_ENABLED, MQTT_NVS_NAMESPACE, val);
}

bool JkkMqttIsEnabled(void)
{
    /* Return cached value — safe to call from PSRAM httpd task */
    return s_mqtt_enabled;
}

esp_err_t JkkMqttInit(void)
{
    /* Load NVS values into RAM cache (safe here — internal-RAM task stack) */
    {
        uint64_t en = 1;
        JkkNvs64_get(JKK_MQTT_NVS_KEY_ENABLED, MQTT_NVS_NAMESPACE, &en);
        s_mqtt_enabled = (en != 0);

        size_t rlen = sizeof(s_broker_addr);
        if (JkkNvsBlobGet(JKK_MQTT_NVS_KEY_BROKER, MQTT_NVS_NAMESPACE,
                          s_broker_addr, &rlen) == ESP_OK && rlen > 0) {
            s_broker_addr[rlen - 1] = '\0';
        } else {
            s_broker_addr[0] = '\0';
        }

        /* Load credentials */
        size_t ulen = sizeof(s_mqtt_user);
        if (JkkNvsBlobGet(JKK_MQTT_NVS_KEY_USER, MQTT_NVS_NAMESPACE,
                          s_mqtt_user, &ulen) == ESP_OK && ulen > 0) {
            s_mqtt_user[ulen - 1] = '\0';
        } else {
            s_mqtt_user[0] = '\0';
        }
        size_t plen = sizeof(s_mqtt_pass);
        if (JkkNvsBlobGet(JKK_MQTT_NVS_KEY_PASS, MQTT_NVS_NAMESPACE,
                          s_mqtt_pass, &plen) == ESP_OK && plen > 0) {
            s_mqtt_pass[plen - 1] = '\0';
        } else {
            s_mqtt_pass[0] = '\0';
        }
    }

    if (!s_mqtt_enabled) {
        ESP_LOGI(TAG, "MQTT wyłączony w konfiguracji");
        return ESP_OK;
    }

    return mqtt_start_client();
}

/**
 * Internal: create and start MQTT client using current RAM-cached config.
 * Separated from JkkMqttInit() so JkkMqttReconnect() can reuse it
 * without reloading NVS.
 */
static esp_err_t mqtt_start_client(void)
{
    build_identifiers();
    ESP_LOGI(TAG, "MQTT start: uid=%s", s_uid);

    char broker_host[64] = "";
    uint16_t broker_port = MQTT_DEFAULT_PORT;
    bool broker_found = false;

    /* 1. Try mDNS discovery */
    broker_found = mdns_discover_broker(broker_host, sizeof(broker_host), &broker_port);

    /* 2. Fallback to NVS */
    if (!broker_found) {
        char nvs_addr[80] = "";
        if (JkkMqttGetBrokerAddress(nvs_addr, sizeof(nvs_addr)) == ESP_OK && strlen(nvs_addr) > 0) {
            /* Parse host:port */
            char *colon = strchr(nvs_addr, ':');
            if (colon) {
                *colon = '\0';
                broker_port = (uint16_t)atoi(colon + 1);
                if (broker_port == 0) broker_port = MQTT_DEFAULT_PORT;
            }
            strlcpy(broker_host, nvs_addr, sizeof(broker_host));
            broker_found = true;
            ESP_LOGI(TAG, "NVS: broker %s:%d", broker_host, broker_port);
        }
    }

    if (!broker_found) {
        ESP_LOGW(TAG, "Broker MQTT nie znaleziony (mDNS ani NVS)");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Łączenie z brokerem MQTT %s:%d", broker_host, broker_port);

    /* Build broker URI — same approach as LuBASE JkkMqttStartToHost() */
    static char s_broker_uri[96];
    snprintf(s_broker_uri, sizeof(s_broker_uri), "mqtt://%s:%d", broker_host, broker_port);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = s_broker_uri,
        .broker.address.port = broker_port,
        .credentials.username = s_mqtt_user[0] ? s_mqtt_user : NULL,
        .credentials.authentication.password = s_mqtt_pass[0] ? s_mqtt_pass : NULL,
        .credentials.client_id = s_uid,
        .network.disable_auto_reconnect = false,
        .session.last_will = {
            .topic = s_topic_avty,
            .msg = "offline",
            .msg_len = 7,
            .qos = 1,
            .retain = 1,
        },
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) {
        ESP_LOGE(TAG, "Nie udało się zainicjować klienta MQTT");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_err_t ret = esp_mqtt_client_start(s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Nie udało się uruchomić klienta MQTT: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MQTT klient uruchomiony -> %s:%d", broker_host, broker_port);
    return ESP_OK;
}

/* ── Reconnect (stop + re-init with current RAM cache) ──── */

void JkkMqttReconnect(void)
{
    ESP_LOGI(TAG, "MQTT reconnect requested");

    /* Stop and destroy existing client */
    if (s_mqtt_client) {
        /* Publish offline before disconnect */
        if (s_mqtt_connected) {
            esp_mqtt_client_publish(s_mqtt_client, s_topic_avty, "offline", 7, 1, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        esp_mqtt_client_stop(s_mqtt_client);
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        s_mqtt_connected = false;
    }

    if (!s_mqtt_enabled) {
        ESP_LOGI(TAG, "MQTT wyłączony — klient zatrzymany");
        return;
    }

    /* Re-start with current RAM-cached settings (NVS already saved) */
    mqtt_start_client();
}
