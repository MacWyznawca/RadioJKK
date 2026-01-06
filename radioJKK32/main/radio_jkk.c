/*  RadioJKK32 - Multifunction Internet Radio Player 
 * 
    1. RadioJKK32 is a multifunctional internet radio player designed to provide a seamless listening experience based on ESP-ADF.
    2. It supports various audio formats and streaming protocols, allowing users to access a wide range of radio stations.
    4. It includes advanced audio processing capabilities, such as equalization and resampling, to enhance the sound experience.
    5. The device is built on the ESP32-A1S audio dev board, leveraging its powerful processing capabilities and connectivity options.
    6. The project is open-source and licensed under the MIT License, allowing for free use, modification, and distribution.

 *  Copyright (C) 2025 Jaromir Kopp (JKK)
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"

#include "audio_common.h"
#include "http_stream.h"
#include "fatfs_stream.h"
#include "raw_stream.h"
#include "i2s_stream.h"
#include "esp_decoder.h"
#include "filter_resample.h"
#include "equalizer.h"

#include "audio_mem.h"
#include "audio_sys.h"

#include "periph_wifi.h"
#include "board.h"

#include "aac_encoder.h"
#include "periph_spiffs.h"
#include "periph_sdcard.h"

#include "jkk_radio.h"

#include "audio_idf_version.h"

#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "periph_button.h"

#include "esp_random.h"
#include <esp_timer.h>

#include "esp_netif.h"

#include <wifi_provisioning/manager.h>

#ifdef CONFIG_JKK_PROV_TRANSPORT_SOFTAP
#include <wifi_provisioning/scheme_softap.h>
#endif /* CONFIG_JKK_PROV_TRANSPORT_SOFTAP */

#include "RawSplit/raw_split.h"
#include "jkk_audio_main.h"
#include "jkk_audio_sdwrite.h"

#include "jkk_nvs.h"
#include "nvs.h"
#include "jkk_settings.h"

// #include "metadata_parser/jkk_metadata.h" 

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
#include "lvgl.h"
#include "display/jkk_mono_lcd.h"
#include "vmeter/volume_meter.h"
#include "display/jkk_lcd_port.h"
#endif
#include "web_server.h"

static const char *TAG = "RADIO";

#define PROV_QR_VERSION         "v1"
#define PROV_TRANSPORT_SOFTAP   "softap"
#define PROV_TRANSPORT_BLE      "ble"
#define PROV_POP_TXT            "jkk"
#define PROV_DEV_NAME           "JKK"

typedef union {
    struct {
        uint64_t current_station:10, 
                 current_eq:6,
                 current_volume:6,
                 is_playing:1;
    };
    uint64_t all64;
} __attribute__((packed)) JkkRadioDataToSave_t; 


static TaskHandle_t eventsHandle = NULL;

const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

static EXT_RAM_BSS_ATTR JkkRadio_t jkkRadio = {0};
static bool fallback_ap_started = false;
static int wifi_disconnect_count = 0;
static QueueHandle_t save_wifi_cmd_queue = NULL;
static bool using_menuconfig_wifi = false; // true when SSID/pass come from Kconfig defaults

// Custom event base for robust control messages (bypasses ADF queues)
ESP_EVENT_DEFINE_BASE(JKK_EVT_BASE);
typedef enum {
    JKK_EVT_SAVE_WIFI = 1,
} jkk_evt_id_t;

static void JkkApplyPendingWifiAndRestart(void) {
    char ssid[32] = {0};
    char pass[64] = {0};
    if (JkkWebGetPendingWifi(ssid, sizeof(ssid), pass, sizeof(pass))) {
        if(strcmp(ssid, jkkRadio.wifiSSID)){
            JkkNvsBlobSet("wifi_ssid", JKK_RADIO_NVS_NAMESPACE, ssid, strlen(ssid) + 1);
        }
        if(strcmp(pass, jkkRadio.wifiPassword)){
            JkkNvsBlobSet("wifi_password", JKK_RADIO_NVS_NAMESPACE, pass, strlen(pass) + 1);
        }
        strncpy(jkkRadio.wifiSSID, ssid, sizeof(jkkRadio.wifiSSID) - 1);
        strncpy(jkkRadio.wifiPassword, pass, sizeof(jkkRadio.wifiPassword) - 1);
        ESP_LOGI(TAG, "WiFi saved via event: %s", ssid);
        JkkRadioSettingsWriteWifi(ssid, pass, jkkRadio.runWebServer);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
    } else {
        ESP_LOGW(TAG, "No pending WiFi creds to save (event)");
    }
}

static esp_err_t JkkRadioStartFallbackSoftap(void) {
    if (fallback_ap_started) {
        ESP_LOGW(TAG, "Fallback SoftAP already started");
        return ESP_OK;
    }

    wifi_config_t ap_config = {0};
    const char *ssid = "RadioJKK-Setup";
    const char *pass = "radiopass"; // >=8 chars

    strncpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(ssid);
    strncpy((char *)ap_config.ap.password, pass, sizeof(ap_config.ap.password) - 1);
    ap_config.ap.max_connection = 4;
    ap_config.ap.channel = 1;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen(pass) < 8) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
        ap_config.ap.password[0] = '\0';
    }

    // Ensure default AP netif exists once; avoid duplicate key assert
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) {
        ap_netif = esp_netif_create_default_wifi_ap();
        ESP_LOGI(TAG, "Created default Wi-Fi AP netif");
    } else {
        ESP_LOGI(TAG, "Default Wi-Fi AP netif already exists");
    }

    if (jkkRadio.wifi_handle) {
        ESP_LOGW(TAG, "Stopping periph_wifi to avoid reconnect spam");
        esp_periph_stop(jkkRadio.wifi_handle);
    }

    // Drain pending events to free space if event iface exists
    if (jkkRadio.evt) {
        audio_event_iface_msg_t drop = {0};
        int drained = 0;
        while (audio_event_iface_listen(jkkRadio.evt, &drop, 0) == ESP_OK && drained < 16) {
            drained++;
        }
        if (drained > 0) {
            ESP_LOGI(TAG, "Drained %d pending events before starting SoftAP", drained);
        }
    }

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGE(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    fallback_ap_started = true;
    wifi_disconnect_count = 0;
    jkkRadio.runWebServer = true;
    stop_web_server();
    start_web_server();
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    {
        // Show SSID, password and mDNS hint on LCD
        char lcdMsg[64];
        snprintf(lcdMsg, sizeof(lcdMsg), "AP:%s pass:%s RadioJKK.local", ssid, pass);
        JkkLcdStationTxt(lcdMsg);

        // Show AP IP address
        esp_netif_ip_info_t ipinfo;
        if (ap_netif && esp_netif_get_ip_info(ap_netif, &ipinfo) == ESP_OK) {
            char ipTxt[16];
            snprintf(ipTxt, sizeof(ipTxt), IPSTR, IP2STR(&ipinfo.ip));
            JkkLcdIpTxt(ipTxt);
        }
    }
#endif
    ESP_LOGW(TAG, "Fallback SoftAP started: SSID=%s", ssid);
    return ESP_OK;
}

static void initialize_sntp(void){
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "tempus1.gum.gov.pl");
    esp_sntp_setservername(1, "0.europe.pool.ntp.org");
    esp_sntp_setservername(2, "pool.ntp.org");
    esp_sntp_init();
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0", 1); 
	tzset();
}

static void SetTimeSync_cb(struct timeval *tv){
    time_t now = 0;
    time(&now);
    ESP_LOGW(TAG, "SetTimeSync_cb %lld", now);
}

static void get_device_service_name(char *service_name, size_t max){
    uint8_t eth_mac[6];
    const char *ssid_prefix = PROV_DEV_NAME;
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X",
             ssid_prefix, eth_mac[4]);
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials"
                         "\n\tSSID     : %s\n\tPassword : %s",
                         (const char *) wifi_sta_cfg->ssid,
                         (const char *) wifi_sta_cfg->password);
                

                if(strcmp((const char *)wifi_sta_cfg->ssid, jkkRadio.wifiSSID)){
                    JkkNvsBlobSet("wifi_ssid", JKK_RADIO_NVS_NAMESPACE, (const char *)wifi_sta_cfg->ssid, strlen((const char *)wifi_sta_cfg->ssid) + 1);
                }
                if(strcmp((const char *)wifi_sta_cfg->password, jkkRadio.wifiPassword)){
                    JkkNvsBlobSet("wifi_password", JKK_RADIO_NVS_NAMESPACE, (const char *)wifi_sta_cfg->password, strlen((const char *)wifi_sta_cfg->password) + 1);
                }
                strcpy(jkkRadio.wifiSSID, (const char *)wifi_sta_cfg->ssid);
                strcpy(jkkRadio.wifiPassword, (const char *)wifi_sta_cfg->password);
                ESP_LOGI(TAG, "Read WiFi settings: SSID: %s, Password: %s", (const char *)wifi_sta_cfg->ssid, (const char *)wifi_sta_cfg->password);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
                JkkLcdQRcode(NULL);
#endif
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                         "\n\tPlease reset to factory and retry provisioning",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                JkkRadioSaveTimerStart(jkkRadio.whatToDo | JKK_RADIO_TO_SAVE_PROVISIONED); 
                break;
            case WIFI_PROV_END:
                ESP_LOGW(TAG, "Provisioning END");
                /* De-initialize manager once provisioning is finished */
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (fallback_ap_started) {
                    ESP_LOGI(TAG, "Disconnected but fallback AP running; skip STA reconnect");
                    break;
                }
                wifi_disconnect_count++;
                ESP_LOGI(TAG, "Disconnected (%d). Reconnecting...", wifi_disconnect_count);
                // If using default Kconfig credentials, fallback immediately on first failure
                if (using_menuconfig_wifi && wifi_disconnect_count >= 1) {
                    ESP_LOGW(TAG, "Default credentials detected; starting fallback SoftAP after first failure");
                    JkkRadioStartFallbackSoftap();
                    wifi_disconnect_count = 0;
                    break;
                }
                if (wifi_disconnect_count >= 10) {
                    ESP_LOGW(TAG, "Too many disconnects, starting fallback SoftAP");
                    JkkRadioStartFallbackSoftap();
                    wifi_disconnect_count = 0;
                    break;
                }
                esp_wifi_connect();
                break;
#ifdef CONFIG_JKK_PROV_TRANSPORT_SOFTAP
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "SoftAP transport: Connected!");
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
                if (fallback_ap_started) {
                    // Fallback AP mode: guide user to web setup
                    JkkLcdStationTxt("Open 192.168.4.1 or RadioJKK.local and set Wi-Fi");
                    // Keep IP visible as 192.168.4.1 for AP
                    JkkLcdIpTxt("192.168.4.1");
                } else {
                    // Provisioning SoftAP mode: show PoP hint/QR
                    JkkLcdStationTxt("If you don't scan QR type: "PROV_POP_TXT);
                    JkkLcdQRcode(NULL);
                }
#endif
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "SoftAP transport: Disconnected!");
                break;
#endif
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
        char ipTxt[16];
        snprintf(ipTxt, sizeof(ipTxt), IPSTR, IP2STR(&event->ip_info.ip));
        JkkLcdIpTxt(ipTxt);
        // Brief hint to access via mDNS
        JkkLcdStationTxt("Web: RadioJKK.local");
#endif
        /* Signal main application to continue execution */
        wifi_disconnect_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
        //JkkRadioSaveTimerStart(jkkRadio.whatToDo | JKK_RADIO_TO_SAVE_PROVISIONED); 
    } else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        switch (event_id) {
            case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
                ESP_LOGI(TAG, "Secured session established!");
                break;
            case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
                ESP_LOGE(TAG, "Received invalid security parameters for establishing secure session!");
                break;
            case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
                ESP_LOGE(TAG, "Received incorrect username and/or PoP for establishing secure session!");
                break;
            default:
                break;
        }
    } else if (event_base == JKK_EVT_BASE) {
        switch (event_id) {
            case JKK_EVT_SAVE_WIFI:
                ESP_LOGI(TAG, "JKK_EVT_SAVE_WIFI received");
                JkkApplyPendingWifiAndRestart();
                break;
            default:
                break;
        }
    }
}

esp_err_t JkkRadioExportStations(const char *filename) {
    FILE *fptr;
    char filepath[64];
    
    if (!jkkRadio.jkkRadioStations || jkkRadio.station_count == 0) {
        ESP_LOGW(TAG, "No stations to export");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Prepare file path
    if (filename == NULL) {
        strcpy(filepath, "/sdcard/stations_export.txt");
    } else {
        snprintf(filepath, sizeof(filepath), "/sdcard/%s", filename);
    }
    
    ESP_LOGI(TAG, "Exporting stations to %s", filepath);
    
    fptr = fopen(filepath, "w");
    if (fptr == NULL) {
        ESP_LOGE(TAG, "Error opening %s for writing", filepath);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Write header with timestamp
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    fprintf(fptr, "# RadioJKK32 Station Export\n");
    fprintf(fptr, "# Generated: %04d-%02d-%02d %02d:%02d:%02d\n",
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    fprintf(fptr, "# Total stations: %d\n", jkkRadio.station_count);
    fprintf(fptr, "#\n");
    fprintf(fptr, "# URL;ShortName;LongName;Favorite;Type;Comment\n");
    
    int exported_count = 0;
    
    // Write stations
    for (int i = 0; i < jkkRadio.station_count; i++) {
        const JkkRadioStations_t *station = &jkkRadio.jkkRadioStations[i];
        
        // Skip invalid stations
        if (strlen(station->uri) == 0) {
            continue;
        }
        
        fprintf(fptr, "%s;%s;%s;%d;%d",
                station->uri,
                station->nameShort,
                station->nameLong,
                station->is_favorite ? 1 : 0,
                station->type);
        
        if (strlen(station->audioDes) > 0) {
            fprintf(fptr, ";%s", station->audioDes);
        }
        
        fprintf(fptr, "\n");
        exported_count++;
    } 
    fclose(fptr);
    
    ESP_LOGI(TAG, "Exported %d stations to %s", exported_count, filepath);
    return ESP_OK;
}

void JkkRadioEqListForWWW(void){
    char *eqRollerOptions = NULL;

    eqRollerOptions = heap_caps_calloc(jkkRadio.eq_count, 18, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    for (int i = 0; i < jkkRadio.eq_count; i++) {
        char eqName[28] = {0};
        snprintf(eqName, sizeof(eqName), "%d;%s", i, jkkRadio.eqPresets[i].name);
        strncat(eqRollerOptions, eqName, 18);
        if(i < jkkRadio.eq_count - 1) strcat(eqRollerOptions, "\n");
    }
    JkkRadioWwwUpdateEqList(eqRollerOptions);
    free(eqRollerOptions);
}

void JkkRadioSetEqualizer(uint8_t eq) {
    if(eq >= jkkRadio.eq_count) {
        ESP_LOGE(TAG, "Invalid equalizer index: %d", eq);
        return;
    }
    
    int oldEq = jkkRadio.current_eq;
    jkkRadio.current_eq = eq;
    
    ESP_LOGI(TAG, "Setting equalizer to: %s (index %d)", jkkRadio.eqPresets[eq].name, eq);
    
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    if(JkkAudioMainProcessState()) JkkLcdEqTxt(jkkRadio.eqPresets[jkkRadio.current_eq].name);
    else JkkLcdEqTxt("");
#endif
    
    if(oldEq != jkkRadio.current_eq) {
        JkkAudioEqSetAll(jkkRadio.eqPresets[jkkRadio.current_eq].gain);
        JkkRadioSaveTimerStart(JKK_RADIO_TO_SAVE_EQ); // Save eq after delay to limit NVS writes
    }
    
    JkkRadioWwwSetEqId(jkkRadio.current_eq);
}

static void JkkRadioAllStationsSave(void) {
    for (int i = 0; i < jkkRadio.station_count; i++) {
        char key[16] = {0};
        sprintf(key, JKK_RADIO_NVS_STATION_KEY, i);
        JkkNvsBlobSet(key, JKK_RADIO_NVS_NAMESPACE, &jkkRadio.jkkRadioStations[i], sizeof(JkkRadioStations_t));
    }
}

esp_err_t JkkRadioReorderStation(int oldIndex, int newIndex) {
    ESP_LOGW(TAG, "Reordering station: from %d to %d", oldIndex, newIndex);
    
    if (oldIndex < 0 || oldIndex >= jkkRadio.station_count || 
        newIndex < 0 || newIndex >= jkkRadio.station_count) {
        ESP_LOGE(TAG, "Invalid indices for reordering: old=%d, new=%d, count=%d", 
                 oldIndex, newIndex, jkkRadio.station_count);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (oldIndex == newIndex) {
        ESP_LOGI(TAG, "No reordering needed, indices are the same");
        return ESP_OK;
    }
    
    JkkRadioStations_t tempStation;
    memcpy(&tempStation, &jkkRadio.jkkRadioStations[oldIndex], sizeof(JkkRadioStations_t));

    if (oldIndex < newIndex) {
        for (int i = oldIndex; i < newIndex; i++) {
            memcpy(&jkkRadio.jkkRadioStations[i], &jkkRadio.jkkRadioStations[i + 1], sizeof(JkkRadioStations_t));
        }
    } else {
        for (int i = oldIndex; i > newIndex; i--) {
            memcpy(&jkkRadio.jkkRadioStations[i], &jkkRadio.jkkRadioStations[i - 1], sizeof(JkkRadioStations_t));
        }
    }
    
    memcpy(&jkkRadio.jkkRadioStations[newIndex], &tempStation, sizeof(JkkRadioStations_t));
    
    if (jkkRadio.current_station == oldIndex) {
        jkkRadio.current_station = newIndex;
        jkkRadio.whatToDo |= JKK_RADIO_TO_SAVE_CURRENT_STATION;
    } else if (oldIndex < newIndex && 
               jkkRadio.current_station > oldIndex && 
               jkkRadio.current_station <= newIndex) {
        jkkRadio.current_station--;
        jkkRadio.whatToDo |= JKK_RADIO_TO_SAVE_CURRENT_STATION;
    } else if (oldIndex > newIndex && 
               jkkRadio.current_station >= newIndex && 
               jkkRadio.current_station < oldIndex) {
        jkkRadio.current_station++;
        jkkRadio.whatToDo |= JKK_RADIO_TO_SAVE_CURRENT_STATION;
    }
    
    if (jkkRadio.prev_station == oldIndex) {
        jkkRadio.prev_station = newIndex;
    } else if (oldIndex < newIndex && 
               jkkRadio.prev_station > oldIndex && 
               jkkRadio.prev_station <= newIndex) {
        jkkRadio.prev_station--;
    } else if (oldIndex > newIndex && 
               jkkRadio.prev_station >= newIndex && 
               jkkRadio.prev_station < oldIndex) {
        jkkRadio.prev_station++;
    }
    
    jkkRadio.whatToDo |= JKK_RADIO_TO_SAVE_ALL_STATIONS;
    
    JkkRadioListForWWW();
    JkkRadioWwwSetStationId(jkkRadio.current_station);
    
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    if (JkkLcdRollerMode() == JKK_ROLLER_MODE_STATION_LIST) {
        char *lcdRollerOptions = heap_caps_calloc(jkkRadio.station_count, 10, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (lcdRollerOptions) {
            for (int i = 0; i < jkkRadio.station_count; i++) {
                strncat(lcdRollerOptions, jkkRadio.jkkRadioStations[i].nameShort, 8);
                if (i < jkkRadio.station_count - 1) strcat(lcdRollerOptions, "\n");
            }
            JkkLcdSetRollerOptions(lcdRollerOptions, jkkRadio.current_station);
            free(lcdRollerOptions);
        }
    }
#endif
    
    ESP_LOGI(TAG, "Station successfully moved from position %d to %d", oldIndex, newIndex);
    JkkRadioSaveTimerStart(jkkRadio.whatToDo | JKK_RADIO_TO_SAVE_STATION_LIST);
    return ESP_OK;
}

static void JkkRadioOneStationErase(int id) {
    if (id < 0 || id >= JKK_RADIO_MAX_STATIONS) {
        ESP_LOGE(TAG, "JkkRadioOneStationErase Invalid station ID: %d", id);
        return;
    }
    char key[16] = {0};
    sprintf(key, JKK_RADIO_NVS_STATION_KEY, id);
    JkkNvsErase(key, JKK_RADIO_NVS_NAMESPACE);
}

void JkkRadioDeleteStation(uint16_t station){
    ESP_LOGW(TAG, "Deleting station: %d", station);
    if(station >= jkkRadio.station_count) {
        ESP_LOGE(TAG, "Invalid station index: %d", station);
        return;
    }
    int index = station;
    if(jkkRadio.current_station == index) {
        jkkRadio.current_station = (jkkRadio.prev_station < jkkRadio.station_count && jkkRadio.prev_station != index) ? jkkRadio.prev_station : 0; // Switch to previous station if deleting current
        JkkRadioSetStation(jkkRadio.current_station);
    } 
    ESP_LOGI(TAG, "Removing station %d: %s", index, jkkRadio.jkkRadioStations[index].nameShort);
    // Shift remaining stations down
    for(int i = index; i < jkkRadio.station_count - 1; i++) {
        jkkRadio.jkkRadioStations[i] = jkkRadio.jkkRadioStations[i + 1];
    }

    jkkRadio.station_count--;
    jkkRadio.jkkRadioStations = realloc(jkkRadio.jkkRadioStations, jkkRadio.station_count * sizeof(JkkRadioStations_t));
    if(!jkkRadio.jkkRadioStations) {
        ESP_LOGE(TAG, "Failed to reallocate memory for stations");
        jkkRadio.station_count = 0; 
        return;
    } else {
        ESP_LOGI(TAG, "Station %d deleted successfully. New station count: %d", index, jkkRadio.station_count);
    }
    JkkRadioListForWWW(); 
    JkkRadioSendMessageToMain(index, JKK_RADIO_CMD_ERASE_FROM_NVS_STATION);
    JkkRadioSaveTimerStart(JKK_RADIO_TO_SAVE_STATION_LIST);
}

static void JkkRadioOneStationSave(int id) {
    if (id < 0 || id >= jkkRadio.station_count) {
        ESP_LOGE(TAG, "JkkRadioOneStationSave Invalid station ID: %d", id);
        return;
    }
    char key[16] = {0};
    sprintf(key, JKK_RADIO_NVS_STATION_KEY, id);
    JkkNvsBlobSet(key, JKK_RADIO_NVS_NAMESPACE, &jkkRadio.jkkRadioStations[id], sizeof(JkkRadioStations_t));
}

void JkkRadioEditStation(char *csvTxt){
    ESP_LOGW(TAG, "Edit station: %s", csvTxt);
    char *idtx = strtok(csvTxt, ";\n");
    char *nameShort = strtok(NULL, ";\n");
    char *nameLong = strtok(NULL, ";\n");
    char *uri = strtok(NULL, ";\n");
    char *is_favorite = strtok(NULL, ";\n");
    if(idtx) {
        int id = atoi(idtx);
        if(id >= jkkRadio.station_count) {
            ESP_LOGE(TAG, "JkkRadioEditStation Invalid station ID: %d", id);
            return;
        }
        if(id == -1) {
            id = jkkRadio.station_count; 
            jkkRadio.jkkRadioStations = realloc(jkkRadio.jkkRadioStations, (jkkRadio.station_count + 1) * sizeof(JkkRadioStations_t));
            if(!jkkRadio.jkkRadioStations) {
                ESP_LOGE(TAG, "Failed to allocate memory for new station");
                return;
            }
            jkkRadio.station_count++;
        }
        if(nameShort) {
            strncpy(jkkRadio.jkkRadioStations[id].nameShort, nameShort, sizeof(jkkRadio.jkkRadioStations[id].nameShort) - 1);
        } else {
            jkkRadio.jkkRadioStations[id].nameShort[0] = '\0'; 
        }
        if(nameLong) {
            strncpy(jkkRadio.jkkRadioStations[id].nameLong, nameLong, sizeof(jkkRadio.jkkRadioStations[id].nameLong) - 1);
        } else {
            jkkRadio.jkkRadioStations[id].nameLong[0] = '\0'; 
        }
        if(uri) {
            strncpy(jkkRadio.jkkRadioStations[id].uri, uri, sizeof(jkkRadio.jkkRadioStations[id].uri) - 1);
        } else {
            jkkRadio.jkkRadioStations[id].uri[0] = '\0'; 
        }
        if(is_favorite) {
            jkkRadio.jkkRadioStations[id].is_favorite = (strcmp(is_favorite, "1") == 0);
        } else {
            jkkRadio.jkkRadioStations[id].is_favorite = false; 
        }
        jkkRadio.jkkRadioStations[id].type = JKK_RADIO_UNKNOWN; 
        jkkRadio.jkkRadioStations[id].addFrom = JKK_RADIO_ADD_FROM_WEB; 
        jkkRadio.jkkRadioStations[id].audioDes[0] = '\0'; 
        
        ESP_LOGI(TAG, "Updated station %d: URI=%s, NameShort=%s, NameLong=%s",
                 id,
                 jkkRadio.jkkRadioStations[id].uri, 
                 jkkRadio.jkkRadioStations[id].nameShort,
                 jkkRadio.jkkRadioStations[id].nameLong);
        JkkRadioListForWWW(); // Update the list for web server
        JkkRadioSendMessageToMain(id, JKK_RADIO_CMD_SAVE_TO_NVS_STATION);
        JkkRadioSaveTimerStart(JKK_RADIO_TO_SAVE_STATION_LIST);

    } else {
        ESP_LOGE(TAG, "Invalid station ID");
    }
}

static void JkkRadioUpdateVolume(void){
    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, true);
    if (jkkRadio.player_volume > 0 && JkkRadioIsPlaying()) {   
        audio_hal_set_volume(jkkRadio.board_handle->audio_hal, jkkRadio.player_volume); 
    }
    if (jkkRadio.player_volume == 0) {
        audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);
    }
    
    JkkRadioSaveTimerStart(JKK_RADIO_TO_SAVE_VOLUME);
    JkkRadioWwwUpdateVolume(jkkRadio.player_volume);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    JkkLcdVolumeInt(jkkRadio.player_volume);
#endif
}

void JkkRadioSetVolume(uint8_t vol){
    if(vol > 100) vol = 100;
    jkkRadio.player_volume = vol;
    ESP_LOGI(TAG, "Set volume: %d", jkkRadio.player_volume);
    JkkRadioUpdateVolume();
}

void JkkRadioListForWWW(void){
    char *webOptions = NULL;

    webOptions = heap_caps_calloc(jkkRadio.station_count, 128 + 256 + 32, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    for (int i = 0; i < jkkRadio.station_count; i++) {
        char stationName[128 + 256 + 44] = {0};
        snprintf(stationName, sizeof(stationName), "%d;%s;%s;%s", i, jkkRadio.jkkRadioStations[i].nameShort, jkkRadio.jkkRadioStations[i].nameLong, jkkRadio.jkkRadioStations[i].uri);
        strncat(webOptions, stationName, 128 + 256 + 44);
        if(i < jkkRadio.station_count - 1) strcat(webOptions, "\n");
    }
    JkkRadioWwwUpdateStationList(webOptions);
    free(webOptions);
}

esp_err_t JkkRadioSendMessageToMain(int mess, int command){
    audio_event_iface_msg_t msg = {0};

    msg.source_type = AUDIO_ELEMENT_TYPE_UNKNOW;
    msg.cmd = command; 
    msg.data = (void*)(intptr_t)mess;
    msg.data_len = sizeof(int);

    // Fast path for SAVE_WIFI: use dedicated queue to avoid clogged audio iface
    if (command == JKK_RADIO_CMD_SAVE_WIFI && save_wifi_cmd_queue) {
        int v = mess;
        if (xQueueSend(save_wifi_cmd_queue, &v, 0) != pdPASS) {
            int dropped;
            xQueueReceive(save_wifi_cmd_queue, &dropped, 0); // drop oldest
            vTaskDelay(pdMS_TO_TICKS(5));
            if (xQueueSend(save_wifi_cmd_queue, &v, 0) != pdPASS) {
                ESP_LOGW(TAG, "SAVE_WIFI queue full, dropping cmd");
                return ESP_FAIL;
            }
        }
        ESP_LOGW(TAG, "SAVE_WIFI enqueued via dedicated queue");
        return ESP_OK;
    }

    // Prefer sending via periph event iface (source) so it reaches the listener (jkkRadio.evt)
    audio_event_iface_handle_t target_iface = NULL;
    if (jkkRadio.set) {
        target_iface = esp_periph_set_get_event_iface(jkkRadio.set);
    } else if (jkkRadio.evt) {
        // Last resort: send to listener (may be ignored if no upstream), better than dropping
        target_iface = jkkRadio.evt;
    }

    if (!target_iface) {
        ESP_LOGW(TAG, "No event iface available, dropping cmd=%d", command);
        return ESP_FAIL;
    }

    esp_err_t ret = audio_event_iface_sendout(target_iface, &msg);
    if (ret != ESP_OK) {
        // Likely listener queue (jkkRadio.evt) is full — drain it if available
        if (jkkRadio.evt) {
            audio_event_iface_msg_t drop = {0};
            int drained = 0;
            while (audio_event_iface_listen(jkkRadio.evt, &drop, 0) == ESP_OK && drained < 16) {
                drained++;
            }
            if (drained > 0) {
                ESP_LOGI(TAG, "Drained %d events from main iface before retry", drained);
            }
        } else {
            // As a fallback, drain from target itself (may be periph iface)
            audio_event_iface_msg_t drop = {0};
            int drained = 0;
            while (audio_event_iface_listen(target_iface, &drop, 0) == ESP_OK && drained < 8) {
                drained++;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = audio_event_iface_sendout(target_iface, &msg);
    }
    ESP_LOGW(TAG, "JkkRadioSendMessageToMain ret: %d, mess: %d", ret, (int)(intptr_t)msg.data);
    return ret;
}

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
static void JkkTimeDisp(void){
    time_t now = 0;
    struct tm timeinfo = { 0 };
    time(&now);
    localtime_r(&now, &timeinfo);

    char timeStr[12] = {0};
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    JkkLcdTimeTxt(timeStr);
}
#endif

int LedIndicatorPattern(void *handle, int disp_pattern, int value){
    int ret = ESP_OK;
    if(handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
#if (CONFIG_JKK_RADIO_USING_I2C_LCD != 1)
    ret = display_service_set_pattern(handle, disp_pattern, value);
#endif
    return ret;
}

static esp_err_t JkkMakePath(time_t timeSet, char *path, char *ext){
    if(path == NULL) return ESP_ERR_INVALID_ARG;

    if(timeSet < EPOCH_TIMESTAMP){
        if(ext) {
            sprintf(path, SD_RECORDS_PATH"/no_time/%08lX.%3s", esp_random(), ext);
            ESP_LOGI(TAG, "Mkdir path (no time set): %s", path);
        }
        else {
            sprintf(path, SD_RECORDS_PATH"/no_time");
            ESP_LOGI(TAG, "Mkdir directory (no time set): %s", path);
        }
        return ESP_OK;
    }

    struct tm timeinfo = { 0 };
    localtime_r(&timeSet, &timeinfo); 

    if(ext) {
        sprintf(path, SD_RECORDS_PATH"/%02d-%02d-%02d/%02d%02d%02d_%1d.%3s", timeinfo.tm_year - 100, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, (uint8_t)((esp_timer_get_time() / 100000) % 10), ext);
        ESP_LOGI(TAG, "Mkdir path: %s", path);
    }
    else {
        sprintf(path, SD_RECORDS_PATH"/%02d-%02d-%02d", timeinfo.tm_year - 100, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        ESP_LOGI(TAG, "Mkdir directory: %s", path);
    }
    return ESP_OK;
}

static bool JkkIOFileInfo(const char *f_path, uint32_t *lenght, uint32_t *time){
    if(!f_path) return 0;
    struct stat info = {0};
    bool exist = stat(f_path, &info) == 0;

    if(time && info.st_mtime) *time = info.st_mtime - EPOCH_TIMESTAMP;
    if(lenght && info.st_size) *lenght = info.st_size;
    return exist;
}
 
static void JkkSdRecInfoWrite(time_t timeSet, const char *path, const char *filePath, bool end){
    FILE *fptr;
    char infoText[480] = {0};
    char infoPath[48] = {0};
    if(path == NULL) {
        ESP_LOGE(TAG, "Invalid path or filePath");
        return;
    }   
    snprintf(infoPath, sizeof(infoPath), "%s/info.txt", path);
    bool isExist = JkkIOFileInfo(infoPath, NULL, NULL);
    fptr = fopen(infoPath, "a");
    if (fptr == NULL) {
        ESP_LOGE(TAG, "Error opening file: %s", path);
        return;
    }
    struct tm timeinfo = { 0 };
    localtime_r(&timeSet, &timeinfo); 
    
    if(!end){
        snprintf(infoText, sizeof(infoText), "%s%s;%s;%s;%04d-%02d-%02d;%02d.%02d.%02d",
                isExist ? "\n" : "# File path;Short name;Description;Start date;Start time;End date;End time\n#\n",
                filePath,
                jkkRadio.jkkRadioStations[jkkRadio.current_station].nameShort,
                jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong,
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    else {
        snprintf(infoText, sizeof(infoText), ";%04d-%02d-%02d;%02d.%02d.%02d",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    fprintf(fptr, infoText);
    fclose(fptr);
}

static void JkkChangeEq(int eqN){
    int oldEq = jkkRadio.current_eq;

    if(eqN == JKK_RADIO_CMD_SET_UNKNOW){
        jkkRadio.current_eq++;
    }
    else if(eqN == 0){
        jkkRadio.current_eq = 0;
    }
    else {
        jkkRadio.current_eq = eqN;
    }
    if(jkkRadio.current_eq >= jkkRadio.eq_count){
        jkkRadio.current_eq = 0;
    }
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    if(JkkAudioMainProcessState()) JkkLcdEqTxt(jkkRadio.eqPresets[jkkRadio.current_eq].name);
    else JkkLcdEqTxt("");
#endif
    JkkRadioWwwSetEqId(jkkRadio.current_eq);
    
    if(oldEq != jkkRadio.current_eq){
        JkkAudioEqSetAll(jkkRadio.eqPresets[jkkRadio.current_eq].gain);
        JkkRadioSaveTimerStart(JKK_RADIO_TO_SAVE_EQ);
    }
}

static void JkkChangeStation(audio_pipeline_handle_t pipeline, changeStation_e urbNr){
    if(jkkRadio.jkkRadioStations == NULL) {
        ESP_LOGW(TAG, "Invalid state");
        return;
    }
    int nextStation = 0;

    if(urbNr == JKK_RADIO_STATION_PREV) {
        nextStation = jkkRadio.current_station - 1;
    } else if(urbNr == JKK_RADIO_STATION_NEXT) {
        nextStation = jkkRadio.current_station + 1;
    } else if(urbNr == JKK_RADIO_STATION_FAV){
        for (size_t i = 0; i < jkkRadio.station_count; i++){
            if(jkkRadio.jkkRadioStations[i].is_favorite) {
                nextStation = i;
                break;
            }
        }
    }
        
    if(nextStation < 0) nextStation = jkkRadio.station_count - 1;
    if(nextStation >= jkkRadio.station_count) nextStation = 0;

    JkkRadioSetStation(nextStation);
}

void JkkRadioSetStation(uint16_t station){
   
    if(station > jkkRadio.station_count - 1) {
        ESP_LOGI(TAG, "No change in station, current station: %d", jkkRadio.current_station);
        return;
    }

    if(station == jkkRadio.current_station) {
        ESP_LOGI(TAG, "No change in station, current station: %d", jkkRadio.current_station);
        return;
    }

    if(jkkRadio.statusStation == JKK_RADIO_STATUS_CHANGING_STATION) {
        ESP_LOGW(TAG, "Changing station in progress");
        return;
    }

    if(jkkRadio.audioSdWrite->is_recording) {
        if(!JkkAudioMainProcessState()){
            ESP_LOGW(TAG, "Stop rec first!");
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
     //       JkkLcdStationTxt("Stop rec first!");
#endif
     //       JkkRadioWwwSetStationId(-2);
     //       vTaskDelay(pdMS_TO_TICKS(2000)); 
     //       JkkRadioWwwSetStationId(jkkRadio.current_station);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
     //       JkkLcdStationTxt(jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong);
#endif
            return;
        }
        JkkRadioStopRecording();
    }

    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);

    jkkRadio.statusStation = JKK_RADIO_STATUS_CHANGING_STATION;

    esp_err_t ret = ESP_OK;

    ret |= audio_pipeline_stop(jkkRadio.audioMain->pipeline);
    ret |= audio_pipeline_wait_for_stop(jkkRadio.audioMain->pipeline);
    ret |= audio_pipeline_change_state(jkkRadio.audioMain->pipeline, AEL_STATE_INIT);
    ret |= audio_pipeline_reset_items_state(jkkRadio.audioMain->pipeline);
    ret |= audio_pipeline_reset_elements(jkkRadio.audioMain->pipeline);
    ret |= audio_pipeline_reset_ringbuffer(jkkRadio.audioMain->pipeline);
    ret |= audio_pipeline_terminate(jkkRadio.audioMain->pipeline);

    if(ret != ESP_OK){
        ESP_LOGI(TAG, "audio_pipeline_reset_ringbuffer Error: %d", ret);
        jkkRadio.statusStation = JKK_RADIO_STATUS_NORMAL;
    }

    ret = ESP_OK;

    ESP_LOGI(TAG, "Station change - Name: %s, Url: %s", jkkRadio.jkkRadioStations[station].nameLong, jkkRadio.jkkRadioStations[station].uri);
    ret |= JkkAudioSetUrl(jkkRadio.jkkRadioStations[station].uri, false);

   // ret |= audio_pipeline_reset_ringbuffer(jkkRadio.audioMain->pipeline);
  //  ret |= audio_pipeline_reset_elements(jkkRadio.audioMain->pipeline);
    // ret |= audio_pipeline_resume(jkkRadio.audioMain->pipeline);
    ret |= audio_pipeline_run(jkkRadio.audioMain->pipeline);

    if(ret != ESP_OK){
        ESP_LOGI(TAG, "audio_pipeline_resume Error: %d", ret);
        jkkRadio.statusStation = JKK_RADIO_STATUS_NORMAL;
    }
    else {
        if(station != jkkRadio.current_station){
            jkkRadio.prev_station = jkkRadio.current_station;
            jkkRadio.current_station = station;
            JkkRadioSaveTimerStart(JKK_RADIO_TO_SAVE_CURRENT_STATION);
            JkkRadioWwwSetStationId(jkkRadio.current_station);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
            JkkLcdStationTxt(">tuning<");
            if(JkkLcdRollerMode() == JKK_ROLLER_MODE_STATION_LIST) {
                JkkLcdShowRoller(true, jkkRadio.current_station, JKK_ROLLER_MODE_STATION_LIST);
            }
#endif
        }
    }
}

static void SaveTimerHandle(TimerHandle_t xTimer){
    if(xTimer == NULL) return;
    ESP_LOGI(TAG, "SaveTimerHandle");

    if((jkkRadio.whatToDo & JKK_RADIO_TO_SAVE_CURRENT_STATION) > 0) {
        jkkRadio.statusStation = JKK_RADIO_STATUS_NORMAL;
    }

    if(jkkRadio.whatToDo & (JKK_RADIO_TO_SAVE_CURRENT_STATION | JKK_RADIO_TO_SAVE_EQ | JKK_RADIO_TO_SAVE_VOLUME | JKK_RADIO_TO_SAVE_PLAY)) {
        bool isPlaying = JkkAudioIsPlaying();
        JkkRadioDataToSave_t toSave = {
            .current_eq = jkkRadio.current_eq,
            .current_station = jkkRadio.current_station,
            .current_volume = jkkRadio.player_volume,
            .is_playing = isPlaying,
        };
        JkkNvs64_set("stateStEq", JKK_RADIO_NVS_NAMESPACE, toSave.all64);
        jkkRadio.whatToDo &= ~(JKK_RADIO_TO_SAVE_CURRENT_STATION | JKK_RADIO_TO_SAVE_EQ | JKK_RADIO_TO_SAVE_VOLUME | JKK_RADIO_TO_SAVE_PLAY);
    }
    if(jkkRadio.whatToDo & JKK_RADIO_TO_SAVE_STATION_LIST){
        JkkRadioExportStations("stations.txt");
        jkkRadio.whatToDo &= ~JKK_RADIO_TO_SAVE_STATION_LIST;
    }
    if(jkkRadio.whatToDo & JKK_RADIO_TO_SAVE_ALL_STATIONS){
        JkkRadioAllStationsSave();
        jkkRadio.whatToDo &= ~JKK_RADIO_TO_SAVE_ALL_STATIONS;
    }
    if(jkkRadio.whatToDo & JKK_RADIO_TO_SAVE_PROVISIONED){
        wifi_prov_mgr_deinit();
        stop_web_server();
        start_web_server();
        ESP_LOGI(TAG, "Provisioning ended");
        jkkRadio.whatToDo &= ~JKK_RADIO_TO_SAVE_PROVISIONED;
    }
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    if(jkkRadio.whatToDo & JKK_RADIO_TO_DO_LCD_OFF) {
        JkkLcdPortOnOffLcd(false);
        jkkRadio.whatToDo &= ~JKK_RADIO_TO_DO_LCD_OFF;
    }
    JkkLcdIpTxt("");
#endif
} 

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
void JkkRadioLcdOn(void) {
    if(JkkLcdPortOnOffLcd(true)) {
        jkkRadio.whatToDo &= ~JKK_RADIO_TO_DO_LCD_OFF;
    }
}
#endif

esp_err_t JkkRadioSaveTimerStart(toSave_e toSave) {
    if(jkkRadio.waitTimer_h == NULL) {
        ESP_LOGE(TAG, "Failed to create save timer");
        return ESP_ERR_INVALID_STATE;
    }
    if(toSave == JKK_RADIO_TO_SAVE_NOTHING) {
        ESP_LOGW(TAG, "Nothing to save");
        return ESP_OK;
    }
    if(toSave > JKK_RADIO_TO_SAVE_ALL){
        jkkRadio.whatToDo = 0;
        return ESP_OK;
    }
    jkkRadio.whatToDo |= toSave;
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    JkkLcdIpTxt(".");
#endif
    return pdPASS == xTimerStart(jkkRadio.waitTimer_h, portMAX_DELAY);
}


void JkkRadioPlay(void) {
    ESP_LOGI(TAG, "Play command received");
    static bool isPlayingTemp;
    isPlayingTemp = JkkRadioIsPlaying();

    esp_err_t ret = JkkAudioPlay();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start playbook: %s", esp_err_to_name(ret));
        return;
    }
    
    if (jkkRadio.player_volume > 0) {
        vTaskDelay(pdMS_TO_TICKS(100)); 
        audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, true);
        ESP_LOGI(TAG, "PA amplifier enabled");
    }
    if(isPlayingTemp != JkkRadioIsPlaying()) {
        JkkRadioSaveTimerStart(JKK_RADIO_TO_SAVE_PLAY);
    }
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    if (JkkRadioIsPlaying()) {
        JkkRadioLcdOn();
        JkkLcdStationTxt(jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong);
    }
#endif
}

void JkkRadioPause(void) {
    ESP_LOGI(TAG, "Pause command received");
    static bool isPlayingTemp;
    isPlayingTemp = JkkRadioIsPlaying();
    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);
    ESP_LOGI(TAG, "PA amplifier disabled");
    JkkRadioStopRecording();
    esp_err_t ret = JkkAudioPause();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to pause playback: %s", esp_err_to_name(ret));
    }
    if(isPlayingTemp != JkkRadioIsPlaying()) {
        JkkRadioSaveTimerStart(JKK_RADIO_TO_SAVE_PLAY);
    }
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    if (!JkkRadioIsPlaying()) {
        JkkLcdStationTxt("| PAUSED | press long vol+ to start");
        JkkLcdVolumeIndicatorCallback(0,0);
        JkkRadioSaveTimerStart(JKK_RADIO_TO_DO_LCD_OFF);
    }
#endif
}

void JkkRadioStop(void) {
    ESP_LOGI(TAG, "Stop command received");
    static bool isPlayingTemp;
    isPlayingTemp = JkkRadioIsPlaying();
    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);
    ESP_LOGI(TAG, "PA amplifier disabled");
    vTaskDelay(pdMS_TO_TICKS(100));
    JkkRadioStopRecording();
    esp_err_t ret = JkkAudioStop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop playback: %s", esp_err_to_name(ret));
    }
    if(isPlayingTemp != JkkRadioIsPlaying()) {
        JkkRadioSaveTimerStart(JKK_RADIO_TO_SAVE_PLAY);
    }
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    JkkLcdStationTxt("- STOPPED - press long vol+ to start");
    JkkLcdVolumeIndicatorCallback(0,0);
    JkkRadioSaveTimerStart(JKK_RADIO_TO_DO_LCD_OFF);
#endif
}

void JkkRadioTogglePlayPause(void) {
    ESP_LOGI(TAG, "Toggle play/pause command received");
    
    if (JkkAudioIsPlaying()) {
        JkkRadioStop();
    } else {
        JkkRadioPlay();
    }
}

bool JkkRadioIsPlaying(void) {
    return jkkRadio.audioMain->audio_state == JKK_AUDIO_STATE_PLAYING;
}

esp_err_t JkkRadioStartRecording(void){
    char folderPath[32];
    time_t now = 0;
    time(&now);
    JkkMakePath(now, folderPath, NULL);
    esp_err_t ret = ESP_OK;
    ret = mkdir(folderPath, 0777);
    if (ret != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "Mkdir directory: %s, failed with errno: %d/%s", folderPath, errno, strerror(errno));
    }
    else {
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
        if(jkkRadio.audioMain->sample_rate > 25000){
            JkkAudioMainOnOffProcessing(false, jkkRadio.evt);
            JkkLcdEqTxt("");
            JkkLcdVolumeIndicatorCallback(0,0);
            JkkRadioWwwSetEqId(0);
        }
#endif
        char filePath[48] = {0};
        JkkMakePath(now, filePath, "aac");
        ret = JkkAudioSdWriteStartStream(filePath);
        if(ret == ESP_OK) {
            JkkSdRecInfoWrite(now, folderPath, filePath, false);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
            JkkLcdIpTxt("");
            JkkLcdRec(true);
#endif
        }
    }
    return ret;
}

void JkkRadioStopRecording(void) {
    ESP_LOGI(TAG, "Stop recording command received");
    char folderPath[32];
    char filePath[48] = {0};
    time_t now = 0;
    time(&now);
    JkkMakePath(now, folderPath, NULL);
    JkkMakePath(now, filePath, "aac");
    JkkSdRecInfoWrite(now, folderPath, filePath, true);
    JkkAudioSdWriteStopStream();
    JkkRadioWwwUpdateRecording(0);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    JkkRadioWwwSetEqId(jkkRadio.current_eq);
    JkkLcdRec(false);
    JkkAudioMainOnOffProcessing(true, jkkRadio.evt);
    JkkLcdEqTxt(jkkRadio.eqPresets[jkkRadio.current_eq].name);
#endif
}

esp_err_t JkkRadioToggleRecording(void) {
    ESP_LOGI(TAG, "Toggle recording command received");
    JkkRadioSendMessageToMain(JKK_RADIO_CMD_TOGGLE_RECORD, JKK_RADIO_CMD_TOGGLE_RECORD);
    return ESP_OK;   
}

void JkkToggleRecording(int command) {
    ESP_LOGI(TAG, "Toggle recording command received");
    if(command != JKK_RADIO_CMD_TOGGLE_RECORD) {
        return;
    }
    if (jkkRadio.audioSdWrite->is_recording) {
        JkkRadioStopRecording();
        JkkRadioWwwUpdateRecording(0);
    } else if(JkkRadioIsPlaying()) {
        esp_err_t ret = JkkRadioStartRecording();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start recording: %s", esp_err_to_name(ret));
            JkkRadioWwwUpdateRecording(-1);
            return ;
        }
        JkkRadioWwwUpdateRecording(1);
    } 
}

static void MainAppTask(void *arg){

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("ledc", ESP_LOG_ERROR);

    jkkRadio.current_station = 0;
    jkkRadio.current_eq = 0;

    ESP_LOGI(TAG, "Start audio codec chip");
    jkkRadio.board_handle = audio_board_init();
    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);
    audio_hal_ctrl_codec(jkkRadio.board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    wifi_event_group = xEventGroupCreate();

    jkkRadio.waitTimer_h = xTimerCreate("saveTimer", (JKK_RADIO_WAIT_TO_SAVE_TIME / portTICK_PERIOD_MS), pdFALSE, NULL, SaveTimerHandle);
    if (!save_wifi_cmd_queue) {
        save_wifi_cmd_queue = xQueueCreate(4, sizeof(int));
    }

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    ESP_LOGI(TAG, "Initialize I2C LCD display");
    JkkLcdUiInit(&jkkRadio);
#endif

    jkkRadio.audioMain = JkkAudioMain_init(3, 1, 1, 1); // in/out type: 3 - HTTP, 1 - I2S; processing type: 1 - EQUALIZER, 1 - RAW_SPLIT; split nr

    jkkRadio.audioSdWrite = JkkAudioSdWrite_init(1, 22050, 2); // 1 - AAC, sample_rate, channels

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    jkkRadio.set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "Start and wait for SDCARD to mount");
    audio_board_sdcard_init(jkkRadio.set, SD_MODE_1_LINE);

    JkkRadioSettingsRead(&jkkRadio);
    JkkRadioStationSdRead(&jkkRadio);
    JkkRadioEqSdRead(&jkkRadio);

    JkkRadioEqListForWWW();
    JkkRadioListForWWW();

    JkkRadioDataToSave_t toRead = {0};

    bool playAnything = false;
    uint64_t _saved_state_tmp = 0; // avoid taking address of packed member
    if(JkkNvs64_get("stateStEq", JKK_RADIO_NVS_NAMESPACE, &_saved_state_tmp) == ESP_OK){
        toRead.all64 = _saved_state_tmp;
        jkkRadio.current_eq = toRead.current_eq < jkkRadio.eq_count ? toRead.current_eq : 0;
        jkkRadio.prev_station = jkkRadio.current_station = toRead.current_station < jkkRadio.station_count ? toRead.current_station : 0;
        jkkRadio.player_volume = toRead.current_volume <= 100 ? toRead.current_volume : 10; // Volume should be in range 0-100
        jkkRadio.is_playing = toRead.is_playing;
    }
    else {
        ESP_LOGW(TAG, "No saved state found, using defaults");
        jkkRadio.player_volume = 10;
        playAnything = jkkRadio.is_playing = true;
    }

    audio_hal_set_volume(jkkRadio.board_handle->audio_hal, jkkRadio.player_volume);
    JkkRadioWwwUpdateVolume(jkkRadio.player_volume);

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    JkkLcdStationTxt(">>>>>>WiFi");
    JkkLcdVolumeInt(jkkRadio.player_volume);
#endif

    ESP_LOGI(TAG, "Start and wait for Wi-Fi network");

    periph_wifi_cfg_t wifi_cfg = {
        .wifi_config.sta.ssid = CONFIG_WIFI_SSID, 
        .wifi_config.sta.password = CONFIG_WIFI_PASSWORD,
    };

    bool wifiFromFlash = true;
    if(jkkRadio.wifiSSID[0] != '\0'){
        strncpy((char *)wifi_cfg.wifi_config.sta.ssid, jkkRadio.wifiSSID, sizeof(wifi_cfg.wifi_config.sta.ssid) - 1);
    } 
    else {
        wifiFromFlash = false;
    }
    if(jkkRadio.wifiPassword[0] != '\0'){
        strncpy((char *)wifi_cfg.wifi_config.sta.password, jkkRadio.wifiPassword, sizeof(wifi_cfg.wifi_config.sta.password) - 1);
    }
    else {
        wifiFromFlash = false;
    }
    // Track whether we are using menuconfig defaults (no NVS/SD creds)
    using_menuconfig_wifi = !wifiFromFlash;
    jkkRadio.wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(jkkRadio.set, jkkRadio.wifi_handle);

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(JKK_EVT_BASE, JKK_EVT_SAVE_WIFI, &event_handler, NULL));
#ifdef CONFIG_JKK_PROV_TRANSPORT_SOFTAP
    // Create default AP netif only if not present (provisioning softAP)
    if (!esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")) {
        esp_netif_create_default_wifi_ap();
    }
#endif /* CONFIG_JKK_PROV_TRANSPORT_SOFTAP */
    #ifdef CONFIG_JKK_PROV_TRANSPORT_SOFTAP
        wifi_prov_mgr_config_t config = {
    #ifdef CONFIG_JKK_PROV_TRANSPORT_SOFTAP
        .scheme = wifi_prov_scheme_softap,
    #endif /* CONFIG_JKK_PROV_TRANSPORT_SOFTAP */
    #ifdef CONFIG_JKK_PROV_TRANSPORT_SOFTAP
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    #endif /* CONFIG_JKK_PROV_TRANSPORT_SOFTAP */
        };

        if(!wifiFromFlash) wifi_prov_mgr_reset_provisioning();

        ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
        ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&jkkRadio.isProvisioned));

        if (!jkkRadio.isProvisioned) {
        ESP_LOGI(TAG, "Starting provisioning");
        char service_name[16];
        get_device_service_name(service_name, sizeof(service_name));

        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

        const char *pop = PROV_POP_TXT;

        wifi_prov_security1_params_t *sec_params = pop;

        const char *service_key = NULL;

        wifi_prov_mgr_endpoint_create("RadioJKK");
        wifi_prov_mgr_disable_auto_stop(1000);

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
        char payload[256] = {0};
#ifdef CONFIG_JKK_PROV_TRANSPORT_SOFTAP
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\"" ",\"pop\":\"%s\",\"transport\":\"%s\"}",
                    PROV_QR_VERSION, service_name, pop, PROV_TRANSPORT_SOFTAP);
#endif 
        JkkLcdStationTxt("Open ESP SodfAP app and scan QR or select SOFTAP and search for JKK...");
        
        JkkLcdQRcode(payload);
#endif

        /* Start provisioning service */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *) sec_params, service_name, service_key));
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
        wifi_prov_mgr_deinit();
    }
#endif /* CONFIG_JKK_PROV_TRANSPORT_SOFTAP */

    esp_err_t wifiRet = periph_wifi_wait_for_connected(jkkRadio.wifi_handle, pdMS_TO_TICKS(10 * 60 * 1000));

    if(wifiRet == ESP_OK){
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
        JkkLcdStationTxt(jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong);
#endif
        JkkRadioWwwSetStationId(jkkRadio.current_station);
    }
    else {
    #if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
        JkkLcdStationTxt(" WiFi cfg AP");
    #endif
        ESP_LOGW(TAG, "WiFi connect failed, starting fallback SoftAP");
    #ifdef CONFIG_JKK_PROV_TRANSPORT_SOFTAP
        wifi_prov_mgr_reset_provisioning();
        wifi_prov_mgr_deinit();
    #endif
        JkkRadioStartFallbackSoftap();
    }  

    ESP_LOGI(TAG, "Initialize keys on board");
    audio_board_key_init(jkkRadio.set);

    sntp_set_time_sync_notification_cb(SetTimeSync_cb);
    initialize_sntp();

    ESP_LOGI(TAG, "Connect input ringbuffer of pipeline_save to http stream multi output");
    ringbuf_handle_t rb = audio_element_get_output_ringbuf(jkkRadio.audioSdWrite->raw_read);
    audio_element_set_multi_output_ringbuf(jkkRadio.audioMain->split, rb, 0);

    esp_err_t ret = mkdir(SD_RECORDS_PATH, 0777);
    if (ret != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "Mkdir directory: %s, failed with errno: %d/%s", SD_RECORDS_PATH, errno, strerror(errno));
    }
    
    ESP_LOGI(TAG, "Set up  uri (http as http_stream, dec as decoder, and default output is i2s)");
    JkkAudioSetUrl(jkkRadio.jkkRadioStations[jkkRadio.current_station].uri, false);
    
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    JkkLcdStationTxt(jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong);
#endif
    JkkRadioWwwSetStationId(jkkRadio.current_station);
    JkkRadioWwwSetEqId(jkkRadio.current_eq); 

    ESP_LOGI(TAG, "Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    jkkRadio.evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "Listening event from all elements of jkkRadio.audioMain->pipeline");
    audio_pipeline_set_listener(jkkRadio.audioMain->pipeline, jkkRadio.evt);
    audio_pipeline_set_listener(jkkRadio.audioSdWrite->pipeline, jkkRadio.evt);

    ESP_LOGI(TAG, "Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(jkkRadio.set), jkkRadio.evt);

    ESP_LOGI(TAG, "Start audio_pipeline");

    if(jkkRadio.is_playing || playAnything){
        JkkAudioPlay();
    }
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    else{
        JkkLcdStationTxt("- STOPPED - press long vol+ to start");
        JkkLcdVolumeIndicatorCallback(0,0);
    }
#endif

    JkkAudioEqSetAll(jkkRadio.eqPresets[jkkRadio.current_eq].gain);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    if(JkkAudioMainProcessState()) JkkLcdEqTxt(jkkRadio.eqPresets[jkkRadio.current_eq].name);
#endif

    if(jkkRadio.runWebServer){
        ESP_LOGI(TAG, "Start web server");
        start_web_server();
    }
    
    while (1) {
        // Handle SAVE_WIFI commands from dedicated queue first
        if (save_wifi_cmd_queue) {
            int qcmd;
            while (xQueueReceive(save_wifi_cmd_queue, &qcmd, 0) == pdTRUE) {
                if (qcmd == JKK_RADIO_CMD_SAVE_WIFI) {
                    char ssid[32] = {0};
                    char pass[64] = {0};
                    if (JkkWebGetPendingWifi(ssid, sizeof(ssid), pass, sizeof(pass))) {
                        if(strcmp(ssid, jkkRadio.wifiSSID)){
                            JkkNvsBlobSet("wifi_ssid", JKK_RADIO_NVS_NAMESPACE, ssid, strlen(ssid) + 1);
                        }
                        if(strcmp(pass, jkkRadio.wifiPassword)){
                            JkkNvsBlobSet("wifi_password", JKK_RADIO_NVS_NAMESPACE, pass, strlen(pass) + 1);
                        }
                        strncpy(jkkRadio.wifiSSID, ssid, sizeof(jkkRadio.wifiSSID) - 1);
                        strncpy(jkkRadio.wifiPassword, pass, sizeof(jkkRadio.wifiPassword) - 1);
                        ESP_LOGI(TAG, "WiFi saved via web (queue): %s", ssid);
                        vTaskDelay(pdMS_TO_TICKS(200));
                        esp_restart();
                    } else {
                        ESP_LOGW(TAG, "No pending WiFi creds to save (queue)");
                    }
                }
            }
        }
        audio_event_iface_msg_t msg = {0};
        esp_err_t ret = audio_event_iface_listen(jkkRadio.evt, &msg, pdMS_TO_TICKS(3000)); // portMAX_DELAY
        if (ret != ESP_OK) {
            audio_element_state_t inState = audio_element_get_state(jkkRadio.audioMain->input);
            audio_element_state_t outState = audio_element_get_state(jkkRadio.audioMain->output);
            audio_element_state_t decState = audio_element_get_state(jkkRadio.audioMain->decoder);
            audio_element_state_t vmState = audio_element_get_state(jkkRadio.audioMain->vmeter);
         //   ESP_LOGW(TAG, "[ Uncnow ] fatfs_wr state: %d, inState: %d", sdState, inState);
            jkkRadio.audioSdWrite->is_recording = JkkAudioSdWriteIsRecording();
            jkkRadio.is_playing = (inState == AEL_STATE_RUNNING && outState == AEL_STATE_RUNNING && decState == AEL_STATE_RUNNING && vmState == AEL_STATE_RUNNING);
            if(jkkRadio.is_playing && (jkkRadio.statusStation == JKK_RADIO_STATUS_ERROR || jkkRadio.statusStation == JKK_RADIO_STATUS_CHANGING_STATION)){
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
                if(jkkRadio.current_station >= 0 && jkkRadio.current_station < jkkRadio.station_count){
                    JkkLcdStationTxt(jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong);
                }
#endif
                jkkRadio.statusStation = JKK_RADIO_STATUS_NORMAL;
                JkkRadioWwwSetStationId(jkkRadio.current_station);
            }
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
            JkkTimeDisp();
#endif
            if(jkkRadio.audioSdWrite->is_recording){
                JkkRadioWwwUpdateRecording(1);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
                JkkLcdRec(true);
#endif             
            }
            else {
                JkkRadioWwwUpdateRecording(0);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
                JkkLcdRec(false);
#endif
            }        
            if(msg.source_type == 0 && msg.cmd == 0){
                continue;
            }
        } 
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && msg.data_len == 4 && msg.data) {
            if((int)(intptr_t)msg.data >= AEL_STATUS_ERROR_OPEN && (int)(intptr_t)msg.data <= AEL_STATUS_ERROR_UNKNOWN){
                if(jkkRadio.statusStation == JKK_RADIO_STATUS_CHANGING_STATION){
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
                    JkkLcdStationTxt(" error! ");
#endif
                    JkkRadioWwwSetStationId(-1);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    jkkRadio.statusStation = JKK_RADIO_STATUS_ERROR;
                    JkkRadioSetStation(jkkRadio.prev_station);
                }
            }
        }
        if(msg.source_type == AUDIO_ELEMENT_TYPE_UNKNOW){
            if(msg.cmd == JKK_RADIO_CMD_SET_STATION){
                int received_value = (int)(intptr_t)msg.data;
                ESP_LOGW(TAG, "JKK_RADIO_CMD_SET_STATION: %d", received_value); 
                JkkRadioSetStation(received_value);
            }
            else if(msg.cmd == JKK_RADIO_CMD_SET_EQUALIZER){
                int received_value = (int)(intptr_t)msg.data;
                ESP_LOGW(TAG, "JKK_RADIO_CMD_SET_STATION: %d", received_value); 
                JkkChangeEq(received_value);
            }
            else if(msg.cmd == JKK_RADIO_CMD_TOGGLE_RECORD){
                int received_value = (int)(intptr_t)msg.data;
                JkkToggleRecording(received_value);
            }
            else if(msg.cmd == JKK_RADIO_CMD_SAVE_TO_NVS_STATION){
                int received_value = (int)(intptr_t)msg.data;
                ESP_LOGW(TAG, "JKK_RADIO_CMD_SAVE_TO_NVS_STATION: %d", received_value);
                JkkRadioOneStationSave(received_value);
            }
            else if(msg.cmd == JKK_RADIO_CMD_ERASE_FROM_NVS_STATION){
                int received_value = (int)(intptr_t)msg.data;
                ESP_LOGW(TAG, "JKK_RADIO_CMD_ERASE_FROM_NVS_STATION: %d", received_value);
                JkkRadioOneStationErase(received_value);
            }
            else if(msg.cmd == JKK_RADIO_CMD_TOGGLE_PLAY_PAUSE){
                ESP_LOGW(TAG, "JKK_RADIO_CMD_TOGGLE_PLAY_PAUSE"); 
                JkkRadioTogglePlayPause();
            }
            else if(msg.cmd == JKK_RADIO_CMD_STOP){
                ESP_LOGW(TAG, "JKK_RADIO_CMD_STOP"); 
                JkkRadioStop();
            }
            else if(msg.cmd == JKK_RADIO_CMD_SAVE_WIFI){
                char ssid[32] = {0};
                char pass[64] = {0};
                if (JkkWebGetPendingWifi(ssid, sizeof(ssid), pass, sizeof(pass))) {
                    if(strcmp(ssid, jkkRadio.wifiSSID)){
                        JkkNvsBlobSet("wifi_ssid", JKK_RADIO_NVS_NAMESPACE, ssid, strlen(ssid) + 1);
                    }
                    if(strcmp(pass, jkkRadio.wifiPassword)){
                        JkkNvsBlobSet("wifi_password", JKK_RADIO_NVS_NAMESPACE, pass, strlen(pass) + 1);
                    }
                    strncpy(jkkRadio.wifiSSID, ssid, sizeof(jkkRadio.wifiSSID) - 1);
                    strncpy(jkkRadio.wifiPassword, pass, sizeof(jkkRadio.wifiPassword) - 1);
                    ESP_LOGI(TAG, "WiFi saved via web: %s", ssid);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    esp_restart();
                } else {
                    ESP_LOGW(TAG, "No pending WiFi creds to save");
                }
            }
        } 
      //  int data = msg.data != NULL ? (int)(intptr_t)msg.data : -1;
      //  ESP_LOGI(TAG, "[ any ] msg.source_type=%d, msg.cmd=%d, Pointer: %p, data: %d", msg.source_type, msg.cmd, msg.source, data);
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *)jkkRadio.audioSdWrite->fatfs_wr
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && msg.data_len == 4
            && (int)msg.data == AEL_STATUS_ERROR_OUTPUT) {
            
            ESP_LOGW(TAG, "Stop write stream"); 
            JkkRadioStopRecording();
            continue;
        }

        if (msg.source_type == PERIPH_ID_SDCARD && msg.cmd == AEL_MSG_CMD_FINISH) {  
            JkkRadioSettingsRead(&jkkRadio);
            JkkRadioStationSdRead(&jkkRadio);
            JkkRadioEqSdRead(&jkkRadio);
            JkkRadioEqListForWWW();
            JkkRadioListForWWW();
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *)jkkRadio.audioMain->decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {

            static audio_element_info_t prev_music_info = {0};
            audio_element_info_t music_info = {0};
            audio_element_getinfo(jkkRadio.audioMain->decoder, &music_info);

            ESP_LOGI(TAG, "Receive music info from dec decoder, sample_rates=%d, bits=%d, ch=%d", 
                     music_info.sample_rates, music_info.bits, music_info.channels);

            if ((prev_music_info.bits != music_info.bits) || 
                (prev_music_info.sample_rates != music_info.sample_rates) || 
                (prev_music_info.channels != music_info.channels)) {
                
                ESP_LOGI(TAG, "Change sample_rates=%d, bits=%d, ch=%d",  
                         music_info.sample_rates, music_info.bits, music_info.channels);
                
                JkkAudioI2sSetClk(music_info.sample_rates, music_info.bits, music_info.channels, true);
                JkkAudioEqSetInfo(music_info.sample_rates, music_info.channels);
                JkkAudioSdWriteResChange(music_info.sample_rates, music_info.channels, music_info.bits);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
                volume_meter_update_format(jkkRadio.audioMain->vmeter, music_info.sample_rates, 
                                         music_info.channels, music_info.bits);
#endif
                memcpy(&prev_music_info, &music_info, sizeof(audio_element_info_t));
            }
            
            if (jkkRadio.player_volume > 0) {
                vTaskDelay(pdMS_TO_TICKS(100));
                if (JkkAudioIsPlaying()) {
                    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, true);
                    ESP_LOGI(TAG, "PA amplifier enabled after music info");
                }
            }
            
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
            if(jkkRadio.statusStation == JKK_RADIO_STATUS_CHANGING_STATION){
                JkkLcdStationTxt(jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong);
            }
#endif
            JkkRadioWwwSetStationId(jkkRadio.current_station);
            jkkRadio.statusStation = JKK_RADIO_STATUS_NORMAL;
            continue;
        }

        /* restart stream when the first jkkRadio.audioMain->pipeline element (http_stream_reader in this case) receives stop event (caused by reading errors) */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) jkkRadio.audioMain->input
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (int)msg.data == AEL_STATUS_ERROR_OPEN) {
            ESP_LOGW(TAG, "Restart stream");
            JkkAudioRestartStream();
            continue;
        }
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
        if (msg.source_type == PERIPH_ID_BUTTON && (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_PRESSED || msg.cmd == PERIPH_BUTTON_PRESSED)) {
            if(JkkLcdPortGetLcdState() == false){
                ESP_LOGI(TAG, "LCD backlight is off, turn it on");
                JkkRadioLcdOn();
                continue;
            }
            if ((int)msg.data == get_input_set_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    // jkkRollerMode_t rollerMode = JkkLcdRollerMode(); // unused
                    if(msg.cmd == PERIPH_BUTTON_RELEASE){
                        char *lcdRollerOptions = heap_caps_calloc(jkkRadio.eq_count, 10, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                        for (int i = 0; i < jkkRadio.eq_count; i++) {
                            strncat(lcdRollerOptions, jkkRadio.eqPresets[i].name, 8);
                            if(i < jkkRadio.eq_count - 1) strcat(lcdRollerOptions, "\n");
                        }
                        JkkLcdSetRollerOptions(lcdRollerOptions, jkkRadio.current_eq);
                        free(lcdRollerOptions);
                        JkkLcdShowRoller(true, jkkRadio.current_eq, JKK_ROLLER_MODE_EQUALIZER_LIST);
                    }
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    jkkRadio.audioSdWrite->is_recording = JkkAudioSdWriteIsRecording();
                    if(jkkRadio.audioSdWrite->is_recording){
                        JkkRadioStopRecording();
                    }
                    else{
                        JkkRadioStartRecording();
                    }
                }
            } else if ((int)msg.data == get_input_mode_id()) {
                jkkRollerMode_t rollerMode = JkkLcdRollerMode();
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    if(rollerMode == JKK_ROLLER_MODE_STATION_LIST || rollerMode == JKK_ROLLER_MODE_EQUALIZER_LIST) {
                        JkkLcdButtonSet(LV_KEY_ENTER, 1);
                    }
                    else {
                        char *lcdRollerOptions = heap_caps_calloc(jkkRadio.station_count, 10, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                        for (int i = 0; i < jkkRadio.station_count; i++) {
                            strncat(lcdRollerOptions, jkkRadio.jkkRadioStations[i].nameShort, 8);
                            if(i < jkkRadio.station_count - 1) strcat(lcdRollerOptions, "\n");
                        }
                        JkkLcdSetRollerOptions(lcdRollerOptions, jkkRadio.current_station);
                        free(lcdRollerOptions);
                        JkkLcdShowRoller(true, jkkRadio.current_station, JKK_ROLLER_MODE_STATION_LIST);
                    }
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED && (rollerMode == JKK_ROLLER_MODE_STATION_LIST || rollerMode == JKK_ROLLER_MODE_EQUALIZER_LIST)){
                    JkkLcdButtonSet(LV_KEY_ESC, 1);
                    JkkLcdShowRoller(false, UINT8_MAX, JKK_ROLLER_MODE_HIDE);
                }

                ESP_LOGI(TAG, "[] get_input_mode_id");
            } else if ((int)msg.data == get_input_volup_id()) {
                jkkRollerMode_t rollerMode = JkkLcdRollerMode();
                if(rollerMode > JKK_ROLLER_MODE_HIDE && rollerMode < JKK_ROLLER_MODE_UNKNOWN){
                    if(msg.cmd == PERIPH_BUTTON_RELEASE){
                        JkkLcdButtonSet(LV_KEY_RIGHT, 1);
                    }
                    else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                        JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_FIRST);
                    }
                }
                else {
                    ESP_LOGI(TAG, "[Vol+] touch tap event");
                    if(msg.cmd == PERIPH_BUTTON_RELEASE){
                        jkkRadio.player_volume += jkkRadio.player_volume < 40 ? (jkkRadio.player_volume < 5 ? 1 : 5) : 10;
                    }
                    else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                        JkkRadioPlay();
                    }
                    if (jkkRadio.player_volume > 100) {
                        jkkRadio.player_volume = 100;
                    }
                    JkkRadioUpdateVolume();
                    ESP_LOGI(TAG, "Volume jkkRadio.set to %d%%", jkkRadio.player_volume);
                }
            } else if ((int)msg.data == get_input_voldown_id()) {
                jkkRollerMode_t rollerMode = JkkLcdRollerMode();
                if(rollerMode > JKK_ROLLER_MODE_HIDE && rollerMode < JKK_ROLLER_MODE_UNKNOWN){
                    ESP_LOGI(TAG, "[Left] tap event");
                    if(msg.cmd == PERIPH_BUTTON_RELEASE){
                        JkkLcdButtonSet(LV_KEY_LEFT, 1);
                    }
                    else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                        JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_FAV);
                    }
                }
                else {
                    ESP_LOGI(TAG, "[Vol-] tap event");
                    if(msg.cmd == PERIPH_BUTTON_RELEASE){
                        jkkRadio.player_volume -= jkkRadio.player_volume < 40 ? (jkkRadio.player_volume < 6 ? 1 : 5) : 10;
                        if (jkkRadio.player_volume < 0) {
                            jkkRadio.player_volume = 0;
                        }
                        JkkRadioUpdateVolume();
                        ESP_LOGI(TAG, "Volume jkkRadio.set to %d%%", jkkRadio.player_volume);
                    }
                    else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                        JkkRadioStop();
                        ESP_LOGI(TAG, "Stoped");
                    }
                }
            }
        }
#else
        if (msg.source_type == PERIPH_ID_BUTTON && (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_PRESSED)) {
            if ((int)msg.data == get_input_play_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_PREV);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_FAV);
                }
            } else if ((int)msg.data == get_input_set_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_NEXT);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_FIRST);
                }
            } else if ((int)msg.data == get_input_mode_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    JkkChangeEq(JKK_RADIO_CMD_SET_UNKNOW);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    JkkChangeEq(0);
                }
            } else if ((int)msg.data == get_input_rec_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    jkkRadio.audioSdWrite->is_recording = JkkAudioSdWriteIsRecording();
                    if(jkkRadio.audioSdWrite->is_recording){
                        continue;
                    }
                    JkkRadioStartRecording();
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    JkkRadioStopRecording();
                }
             } else if ((int)msg.data == get_input_volup_id()) {
            
                 if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    ESP_LOGI(TAG, "[Vol+] touch tap event");
                    jkkRadio.player_volume += jkkRadio.player_volume < 40 ? 5 : 10;
                    if (jkkRadio.player_volume > 100) {
                        jkkRadio.player_volume = 100;
                    }
                    JkkRadioUpdateVolume();
                    ESP_LOGI(TAG, "Volume jkkRadio.set to %d%%", jkkRadio.player_volume);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    JkkRadioPlay();
                    ESP_LOGI(TAG, "Playing");
                }
            } else if ((int)msg.data == get_input_voldown_id()) {
                ESP_LOGI(TAG, "[Vol-] touch tap event");
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    jkkRadio.player_volume -= jkkRadio.player_volume < 40 ? 5 : 10;
                    if (jkkRadio.player_volume < 0) {
                        jkkRadio.player_volume = 0;
                    }
                    JkkRadioUpdateVolume();
                    ESP_LOGI(TAG, "Volume jkkRadio.set to %d%%", jkkRadio.player_volume);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    JkkRadioStop();
                    ESP_LOGI(TAG, "Stoped");
                }
            }
        }
#endif
    }

    esp_periph_set_stop_all(jkkRadio.set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(jkkRadio.set), jkkRadio.evt);
    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(jkkRadio.evt);
    /* Release all resources */
    esp_periph_set_destroy(jkkRadio.set);
    
    ESP_LOGI(TAG, "Stop audio_pipeline");
    JkkAudioMain_deinit();
    JkkAudioSdWrite_deinit();
    audio_board_deinit(jkkRadio.board_handle);
    free(jkkRadio.jkkRadioStations);

}

void app_main(){
    xTaskCreatePinnedToCoreWithCaps(MainAppTask, "LVGL", 4 * 1024 + 512, NULL, 4, &eventsHandle, 1, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}