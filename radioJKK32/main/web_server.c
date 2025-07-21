#include <esp_http_server.h>
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "web_server.h"
#include "jkk_radio.h"

static const char *TAG = "JKK_WEB";

static bool webServerRunning = false;
static EXT_RAM_BSS_ATTR httpd_handle_t server = NULL;

static EXT_RAM_BSS_ATTR char station_list[(128 + 128 + 32) * JKK_RADIO_MAX_STATIONS] = "";
static uint8_t volume = 10;
static int8_t station_id = -1;
static uint8_t eq_id = -1;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

void JkkRadioWwwSetStationId(int8_t id) {
    station_id = id;
}

void JkkRadioWwwUpdateStationList(const char *list) {
    strncpy(station_list, list, sizeof(station_list) - 1);
}

void JkkRadioWwwUpdateVolume(uint8_t vol) {
    volume = vol;
    ESP_LOGI(TAG, "Volume WWW server set to %d%%", volume);
}

esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

esp_err_t info_get_handler(httpd_req_t *req) {
    char current_status[128 + 128 + 32] = {0};
    snprintf(current_status, sizeof(current_status), "%d;%d;%d", volume, station_id, eq_id);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, current_status);
    return ESP_OK;
}

esp_err_t station_list_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, station_list);
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
    JkkRadioSetStation(station);
    httpd_resp_sendstr(req, "OK");
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

httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
httpd_uri_t uri_station_name = { .uri = "/status", .method = HTTP_GET, .handler = info_get_handler };
httpd_uri_t uri_volume = { .uri = "/volume", .method = HTTP_POST, .handler = volume_post_handler };
httpd_uri_t uri_station_list = { .uri = "/station_list", .method = HTTP_GET, .handler = station_list_get_handler };
httpd_uri_t uri_station_select = { .uri = "/station_select", .method = HTTP_POST, .handler = station_select_post_handler };
httpd_uri_t uri_station_delete = { .uri = "/station_delete", .method = HTTP_POST, .handler = station_delete_post_handler };
httpd_uri_t uri_station_edit = { .uri = "/station_edit", .method = HTTP_POST, .handler = station_edit_post_handler };

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

    ESP_ERROR_CHECK(mdns_service_add("RadioJKK", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
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
    config.max_open_sockets = 10;
    config.task_priority = tskIDLE_PRIORITY + 1;
    config.task_caps = MALLOC_CAP_INTERNAL| MALLOC_CAP_8BIT; // MALLOC_CAP_SPIRAM

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_root);
        httpd_register_uri_handler(server, &uri_station_name);
        httpd_register_uri_handler(server, &uri_volume);
        httpd_register_uri_handler(server, &uri_station_list);
        httpd_register_uri_handler(server, &uri_station_select);
        httpd_register_uri_handler(server, &uri_station_delete);
        httpd_register_uri_handler(server, &uri_station_edit);
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
