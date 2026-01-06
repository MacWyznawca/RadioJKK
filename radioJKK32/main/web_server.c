#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "web_server.h"
#include "jkk_radio.h"
#include "jkk_nvs.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(JKK_EVT_BASE);
// Keep event IDs consistent with radio_jkk.c
typedef enum {
    JKK_EVT_SAVE_WIFI = 1,
} jkk_evt_id_t;

#ifdef CONFIG_JKK_RADIO_USING_I2C_LCD
#include "display/jkk_lcd_port.h"
#endif

static const char *TAG = "JKK_WEB";

static bool webServerRunning = false;
static EXT_RAM_BSS_ATTR httpd_handle_t server = NULL;

static EXT_RAM_BSS_ATTR char station_list[(128 + 128 + 32) * JKK_RADIO_MAX_STATIONS] = "";
static EXT_RAM_BSS_ATTR char eq_list[18 * JKK_RADIO_MAX_EQ_PRESETS] = "";
static uint8_t volume = 10;
static int8_t station_id = -1;
static uint8_t eq_id = 0;
static int8_t is_rec = 0;
static char wifi_ssid[32] = "";
static char wifi_pass[64] = "";
static bool wifi_pending = false;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

void JkkRadioWwwSetStationId(int8_t id) {
    station_id = id;
}

void JkkRadioWwwUpdateStationList(const char *list) {
    strncpy(station_list, list, sizeof(station_list) - 1);
}

void JkkRadioWwwUpdateEqList(const char *list) {
    strncpy(eq_list, list, sizeof(eq_list) - 1);
}

void JkkRadioWwwSetEqId(uint8_t id) {
    eq_id = id;
}

void JkkRadioWwwUpdateVolume(uint8_t vol) {
    volume = vol;
}

void JkkRadioWwwUpdateRecording(int8_t rec) {
    is_rec = rec;
}

bool JkkWebGetPendingWifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    if (!wifi_pending || !ssid || !pass) {
        return false;
    }
    strlcpy(ssid, wifi_ssid, ssid_len);
    strlcpy(pass, wifi_pass, pass_len);
    wifi_pending = false;
    wifi_ssid[0] = '\0';
    wifi_pass[0] = '\0';
    return true;
}

static void url_decode(char *dst, const char *src, size_t len) {
    size_t o = 0;
    for (size_t i = 0; i < len && src[i]; ++i) {
        if (src[i] == '%' && i + 2 < len) {
            int hi = src[i + 1];
            int lo = src[i + 2];
            hi = (hi >= '0' && hi <= '9') ? hi - '0' : (hi >= 'A' && hi <= 'F') ? 10 + hi - 'A' : (hi >= 'a' && hi <= 'f') ? 10 + hi - 'a' : -1;
            lo = (lo >= '0' && lo <= '9') ? lo - '0' : (lo >= 'A' && lo <= 'F') ? 10 + lo - 'A' : (lo >= 'a' && lo <= 'f') ? 10 + lo - 'a' : -1;
            if (hi >= 0 && lo >= 0) {
                dst[o++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        } else if (src[i] == '+') {
            dst[o++] = ' ';
            continue;
        }
        dst[o++] = src[i];
    }
    dst[o] = '\0';
}

esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

esp_err_t info_get_handler(httpd_req_t *req) {
    char current_status[80] = {0};
    snprintf(current_status, sizeof(current_status), "%d;%d;%d;%d;%d", volume, station_id, eq_id, JkkRadioIsPlaying() ? 1 : 0, is_rec);
    httpd_resp_set_type(req, "text/plain");
        
    // Dodajemy status LCD jako szósty parametr (0=off, 1=on)
    snprintf(current_status, sizeof(current_status), "%d;%d;%d;%d;%d;%d", volume, station_id, eq_id, JkkRadioIsPlaying() ? 1 : 0, is_rec,
#ifdef CONFIG_JKK_RADIO_USING_I2C_LCD
    JkkLcdPortGetLcdState() ? 1 : 0
#else
    -1
#endif
    );
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, current_status);
    return ESP_OK;
}

esp_err_t lcd_toggle_post_handler(httpd_req_t *req) {
    char buf[4] = {0};
    if (httpd_req_recv(req, buf, sizeof(buf)-1) <= 0) return ESP_FAIL;
    int turn_on = atoi(buf);
    bool new_state = 0;
#ifdef CONFIG_JKK_RADIO_USING_I2C_LCD
    if(turn_on){
        JkkRadioLcdOn();
        new_state = JkkLcdPortGetLcdState();
    }
    else
        new_state = JkkLcdPortOnOffLcd(false);
#endif
    char resp[8];
    snprintf(resp, sizeof(resp), "%d", new_state ? 1 : 0);
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

esp_err_t station_list_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, station_list);
    return ESP_OK;
}

esp_err_t eq_list_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, eq_list);
    return ESP_OK;
}

esp_err_t volume_post_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    char buf[10];
    if (httpd_req_recv(req, buf, MIN(total_len, sizeof(buf))) <= 0) return ESP_FAIL;
    uint8_t new_volume = atoi(buf);
    JkkRadioSetVolume(new_volume);
    volume = new_volume;
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t station_select_post_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    char buf[8];
    if (httpd_req_recv(req, buf, MIN(total_len, sizeof(buf))) <= 0) return ESP_FAIL;
    uint16_t station = atoi(buf);
    if(JkkAudioIsPlaying()){
        JkkRadioSetStation(station);
        httpd_resp_sendstr(req, "OK");
    }
    else {
        httpd_resp_sendstr(req, "Audio not playing");
    }
    return ESP_OK;
}

esp_err_t station_delete_post_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    char buf[8] = {0};
    if (httpd_req_recv(req, buf, MIN(total_len, sizeof(buf))) <= 0) return ESP_FAIL;
    uint16_t station = atoi(buf);
    JkkRadioDeleteStation(station);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t station_edit_post_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    char lineStr[128 + 128 + 32] = {0};
    if (httpd_req_recv(req, lineStr, MIN(total_len, sizeof(lineStr))) <= 0) return ESP_FAIL;
    ESP_LOGI(TAG, "station_edit_post_handler %s", lineStr);
    JkkRadioEditStation(lineStr);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t station_reorder_post_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    char buf[32] = {0};
    if (httpd_req_recv(req, buf, MIN(total_len, sizeof(buf))) <= 0) return ESP_FAIL;
    
    // Parse "oldIndex,newIndex"
    char *oldIndex_str = strtok(buf, ",");
    char *newIndex_str = strtok(NULL, ",");
    
    if (oldIndex_str && newIndex_str) {
        int oldIndex = atoi(oldIndex_str);
        int newIndex = atoi(newIndex_str);
        ESP_LOGI(TAG, "station_reorder_post_handler: moving station from %d to %d", oldIndex, newIndex);
        
        esp_err_t result = JkkRadioReorderStation(oldIndex, newIndex);
        if (result == ESP_OK) {
            httpd_resp_sendstr(req, "OK");
        } else {
            httpd_resp_sendstr(req, "ERROR");
        }
    } else {
        ESP_LOGE(TAG, "Invalid reorder parameters");
        httpd_resp_sendstr(req, "ERROR");
    }
    
    return ESP_OK;
}

esp_err_t eq_select_post_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    char buf[8];
    if (httpd_req_recv(req, buf, MIN(total_len, sizeof(buf))) <= 0) return ESP_FAIL;
    uint8_t eq_index = atoi(buf);
    ESP_LOGI(TAG, "eq_select_post_handler: selecting equalizer %d", eq_index);
    JkkRadioSetEqualizer(eq_index);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t stations_backup_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "stations_backup_get_handler called");
    
    // Set headers for file download
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"radiojkk_stations_file.csv\"");
    
    // Create temporary export
    esp_err_t ret = JkkRadioExportStations("stat_tmp.txt");
    if (ret != ESP_OK) {
        httpd_resp_sendstr(req, "Error creating backup");
        return ESP_FAIL;
    }
    
    // Read and send file
    FILE *fptr = fopen("/sdcard/stat_tmp.txt", "r");
    if (fptr == NULL) {
        httpd_resp_sendstr(req, "Error reading backup file");
        return ESP_FAIL;
    }
    
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), fptr)) {
        httpd_resp_sendstr_chunk(req, buffer);
    }
    
    fclose(fptr);
    remove("/sdcard/stat_tmp.txt"); // Clean up
    
    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    
    ESP_LOGI(TAG, "Stations backup sent successfully");
    return ESP_OK;
}

esp_err_t stop_post_handler(httpd_req_t *req) {
    //JkkRadioStop();
    JkkRadioSendMessageToMain(0, JKK_RADIO_CMD_STOP);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t toggle_play_pause_post_handler(httpd_req_t *req) {
    JkkRadioSendMessageToMain(0, JKK_RADIO_CMD_TOGGLE_PLAY_PAUSE);
    //JkkRadioTogglePlayPause();
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t toggle_record_post_handler(httpd_req_t *req) {
    esp_err_t ret = JkkRadioToggleRecording();
    if (ret != ESP_OK) {
        httpd_resp_sendstr(req, "Error toggling recording");
    }
    else {
        httpd_resp_sendstr(req, "OK");
    }  
    return ESP_OK;
}

static esp_err_t wifi_save_post_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 160) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid length");
        return ESP_FAIL;
    }

    char buf[192] = {0};
    if (httpd_req_recv(req, buf, MIN(sizeof(buf) - 1, total_len)) <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
        return ESP_FAIL;
    }

    char ssid_raw[64] = {0};
    char pass_raw[96] = {0};

    char *ssid_ptr = strstr(buf, "ssid=");
    char *pass_ptr = strstr(buf, "pass=");
    if (!ssid_ptr || !pass_ptr) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing fields");
        return ESP_FAIL;
    }

    ssid_ptr += 5;
    char *ssid_end = strchr(ssid_ptr, '&');
    size_t ssid_len = ssid_end ? (size_t)(ssid_end - ssid_ptr) : strlen(ssid_ptr);
    size_t pass_len = strlen(pass_ptr + 5);
    url_decode(ssid_raw, ssid_ptr, MIN(ssid_len, sizeof(ssid_raw) - 1));
    url_decode(pass_raw, pass_ptr + 5, MIN(pass_len, sizeof(pass_raw) - 1));

    if (strlen(ssid_raw) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty SSID");
        return ESP_FAIL;
    }

    strncpy(wifi_ssid, ssid_raw, sizeof(wifi_ssid) - 1);
    strncpy(wifi_pass, pass_raw, sizeof(wifi_pass) - 1);
    wifi_pending = true;

    // Prefer robust esp_event path (internal RAM task) to avoid PSRAM task NVS writes and ADF queue congestion
    esp_err_t post_ret = esp_event_post(JKK_EVT_BASE, JKK_EVT_SAVE_WIFI, NULL, 0, 0);
    if (post_ret != ESP_OK) {
        // Fallback to legacy message path
        JkkRadioSendMessageToMain(JKK_RADIO_CMD_SAVE_WIFI, JKK_RADIO_CMD_SAVE_WIFI);
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Saving Wi-Fi... device will reboot");
    return ESP_OK;
}

httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
httpd_uri_t uri_station_name = { .uri = "/status", .method = HTTP_GET, .handler = info_get_handler };
httpd_uri_t uri_volume = { .uri = "/volume", .method = HTTP_POST, .handler = volume_post_handler };
httpd_uri_t uri_station_list = { .uri = "/station_list", .method = HTTP_GET, .handler = station_list_get_handler };
httpd_uri_t uri_eq_list = { .uri = "/eq_list", .method = HTTP_GET, .handler = eq_list_get_handler };
httpd_uri_t uri_station_select = { .uri = "/station_select", .method = HTTP_POST, .handler = station_select_post_handler };
httpd_uri_t uri_station_delete = { .uri = "/station_delete", .method = HTTP_POST, .handler = station_delete_post_handler };
httpd_uri_t uri_station_edit = { .uri = "/station_edit", .method = HTTP_POST, .handler = station_edit_post_handler };
httpd_uri_t uri_station_reorder = { .uri = "/station_reorder", .method = HTTP_POST, .handler = station_reorder_post_handler };
httpd_uri_t uri_eq_select = { .uri = "/eq_select", .method = HTTP_POST, .handler = eq_select_post_handler };
httpd_uri_t uri_stations_backup = { .uri = "/backup_stations", .method = HTTP_GET, .handler = stations_backup_get_handler };
httpd_uri_t uri_stop = { .uri = "/stop", .method = HTTP_POST, .handler = stop_post_handler };
httpd_uri_t uri_toggle = { .uri = "/toggle", .method = HTTP_POST, .handler = toggle_play_pause_post_handler };
httpd_uri_t uri_rec_toggle = { .uri = "/rec_toggle", .method = HTTP_POST, .handler = toggle_record_post_handler };
httpd_uri_t uri_wifi_save = { .uri = "/wifi_save", .method = HTTP_POST, .handler = wifi_save_post_handler };

httpd_uri_t uri_lcd_toggle = { .uri = "/lcd_toggle", .method = HTTP_POST, .handler = lcd_toggle_post_handler };

#define MDNS_INSTANCE "radio jkk web server"
#define MDNS_HOST_NAME "RadioJKK"
    
static void initialise_mdns(void)
{
    mdns_init();
    mdns_hostname_set(MDNS_HOST_NAME);
    mdns_instance_name_set(MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "Ai Thinker ESP32 A1S"},
        {"path", "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add("RadioJKK", "_http", "_tcp", 80, serviceTxtData, sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
    netbiosns_init();
    netbiosns_set_name(MDNS_HOST_NAME);
}

void start_web_server(void) {
    if(webServerRunning) {
        ESP_LOGW(TAG, "Web server is already running");
        return;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 4096;
    config.server_port = 80;
    config.core_id = 1; 
    config.max_open_sockets = 16;
    config.max_uri_handlers = 16;
    config.task_priority = tskIDLE_PRIORITY + 1;
    config.task_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT; // MALLOC_CAP_SPIRAM // MALLOC_CAP_INTERNAL

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_root);
        httpd_register_uri_handler(server, &uri_station_name);
        httpd_register_uri_handler(server, &uri_volume);
        httpd_register_uri_handler(server, &uri_station_list);
        httpd_register_uri_handler(server, &uri_eq_list);
        httpd_register_uri_handler(server, &uri_station_select);
        httpd_register_uri_handler(server, &uri_station_delete);
        httpd_register_uri_handler(server, &uri_station_edit);
        httpd_register_uri_handler(server, &uri_station_reorder);
        httpd_register_uri_handler(server, &uri_eq_select);
        httpd_register_uri_handler(server, &uri_stations_backup);
        httpd_register_uri_handler(server, &uri_stop);
        httpd_register_uri_handler(server, &uri_toggle);
        httpd_register_uri_handler(server, &uri_rec_toggle);
        httpd_register_uri_handler(server, &uri_wifi_save);
        httpd_register_uri_handler(server, &uri_lcd_toggle);
        ESP_LOGI(TAG, "Serwer WWW uruchomiony");

        initialise_mdns();
        webServerRunning = true;
    }
}

void stop_web_server(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "Serwer WWW zatrzymany");
        webServerRunning = false;
    }
}