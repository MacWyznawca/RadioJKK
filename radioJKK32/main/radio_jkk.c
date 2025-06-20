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
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"

#include "jkk_audio_main.h"
#include "jkk_audio_sdwrite.h"

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

#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "board.h"

#include "aac_encoder.h"
#include "periph_spiffs.h"
#include "periph_sdcard.h"

#include "RawSplit/raw_split.h"

#include "jkk_radio.h"

#include "audio_idf_version.h"

#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "periph_button.h"

#include "esp_random.h"
#include <esp_timer.h>

#include "esp_netif.h"

#include "jkk_nvs.h"
#include "nvs.h"

extern const char stations_start[] asm("_binary_stations_txt_start"); 

#define EPOCH_TIMESTAMP (978307200l)

#define SD_RECORDS_PATH "/sdcard/rec"

#define JKK_RADIO_MAX_STATIONS (20) // Maximum number of radio stations
#define JKK_RADIO_MAX_EBMEDDED_STATIONS (4) // Maximum number of radio stations

#define JKK_RADIO_PREDEV_EQ (4) // Maximum number of radio stations

static const char *TAG = "RADIO";

static const EXT_RAM_BSS_ATTR int eq_Gains[JKK_RADIO_PREDEV_EQ][10] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 4, 3, 1, 0, -1, 0, 1, 3, 6},
    {0, 6, 6, 4, 0, -1, -1, 0, 6, 10},
    {-6, -6, -2, 1, 2, 3, 2, 1, -2, -6}
};

typedef struct JkkRadio_s {
    JkkAudioMain_t *audioMain;
    JkkAudioSdWrite_t *audioSdWrite;
    esp_periph_set_handle_t set;
    esp_periph_handle_t wifi_handle;
    audio_event_iface_handle_t evt;
    audio_board_handle_t board_handle;
    display_service_handle_t disp_serv;
    int player_volume; // Volume level for the player
    int current_station; // Current station index
    int station_count; // Total number of stations
    int current_eq;
    bool is_playing; // Flag indicating if the radio is currently playing
    JkkRadioStations_t *jkkRadioStations; // Pointer to the array of radio stations
    char wifiSSID[32]; // WiFi SSID
    char wifiPassword[64]; // WiFi Password
} JkkRadio_t;

static EXT_RAM_BSS_ATTR JkkRadio_t jkkRadio = {0};

static void initialize_sntp(void){
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "tempus1.gum.gov.pl");
    esp_sntp_setservername(1, "0.europe.pool.ntp.org");
    esp_sntp_setservername(2, "pool.ntp.org");
    esp_sntp_init();
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0", 1); // "CET-1CEST,M3.5.0,M10.5.0"
	tzset();
}

static void SetTimeSync_cb(struct timeval *tv){
    time_t now = 0;
    time(&now);
    ESP_LOGW(TAG, "SetTimeSync_cb %lld", now);
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
 
static void JkkSdRecInfoWrite(time_t timeSet, const char *path, const char *filePath){
    FILE *fptr;
    char infoText[480] = {0};
    char infoPath[48] = {0};
    if(path == NULL) {
        ESP_LOGE(TAG, "Invalid path or filePath");
        return;
    }   
    strcpy(infoPath, path);
    fptr = fopen(strcat(infoPath, "/info.txt"), "a");
    if (fptr == NULL) {
        ESP_LOGE(TAG, "Error opening file: %s", path);
        return;
    }
    struct tm timeinfo = { 0 };
    localtime_r(&timeSet, &timeinfo); 

    sprintf(infoText, "%s;%s;%s;%04d-%02d-%02d;%02d.%02d.%02d\n",
                filePath,
                jkkRadio.jkkRadioStations[jkkRadio.current_station].nameShort,
                jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong,
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    fprintf(fptr, infoText);

    fclose(fptr);
}

static bool JkkIOFileInfo(const char *f_path, uint32_t *lenght, uint32_t *time){
    if(!f_path) return 0;
    struct stat info = {0};
    bool exist = stat(f_path, &info) == 0;

    if(time && info.st_mtime) *time = info.st_mtime - EPOCH_TIMESTAMP;
    if(lenght && info.st_size) *lenght = info.st_size;
    return exist;
}

static void JkkChangeEq(int eqN){
    if(eqN == 1){
        jkkRadio.current_eq++;
    }
    else if(eqN == 0){
        jkkRadio.current_eq = 0;
    }
    if(jkkRadio.current_eq >= JKK_RADIO_PREDEV_EQ){
        jkkRadio.current_eq = 0;
    }
    display_service_set_pattern(jkkRadio.disp_serv, JKK_DISPLAY_PATTERN_BR_PULSE + jkkRadio.current_eq + 1, 1);
    JkkAudioEqSetAll(eq_Gains[jkkRadio.current_eq]);
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

    jkkRadio.current_station = nextStation;
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);

    ESP_LOGI(TAG, "Station change - Name: %s, Url: %s", jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong, jkkRadio.jkkRadioStations[jkkRadio.current_station].uri);
    display_service_set_pattern(jkkRadio.disp_serv, JKK_DISPLAY_PATTERN_BR_PULSE + jkkRadio.current_station + 1, 1);
    JkkAudioSetUrl(jkkRadio.jkkRadioStations[jkkRadio.current_station].uri, false);

    audio_pipeline_reset_ringbuffer(pipeline);
    audio_pipeline_reset_elements(pipeline);
    audio_pipeline_run(pipeline);

    if(jkkRadio.audioSdWrite->is_recording){
        JkkAudioSdWriteStopStream();

        char folderPath[32] = {0};
        time_t now = 0;
        time(&now);
        JkkMakePath(now, folderPath, NULL);
        esp_err_t ret = ESP_OK;
        ret = mkdir(folderPath, 0777);
        if (ret != 0 && errno != EEXIST) {
            ESP_LOGE(TAG, "Mkdir directory: %s, failed with errno: %d/%s", folderPath, errno, strerror(errno));
        }
        else{
            char filePath[48] = {0};
            JkkMakePath(now, filePath, "aac");
            ret = JkkAudioSdWriteStartStream(filePath);
            if(ret == ESP_OK) JkkSdRecInfoWrite(now, folderPath, filePath);
        }
    }
}

static esp_err_t JkkRadioSettingsRead(void) {
    FILE *fptr;

    JkkNvsBlobGet("wifi_ssid", JKK_RADIO_NVS_NAMESPACE, jkkRadio.wifiSSID, NULL); // &(size_t){sizeof(jkkRadio.wifiSSID)}
    JkkNvsBlobGet("wifi_password", JKK_RADIO_NVS_NAMESPACE, jkkRadio.wifiPassword, NULL); // &(size_t){sizeof(jkkRadio.wifiPassword)}

    ESP_LOGI(TAG, "WiFi settings from NVS: SSID: %s, Password: %s", jkkRadio.wifiSSID, jkkRadio.wifiPassword);

    fptr = fopen("/sdcard/settings.txt", "r");
    if (fptr == NULL) {
        ESP_LOGE(TAG, "Error opening file: /sdcard/settings.txt");
        return ESP_ERR_NOT_FOUND;
    }
    char lineStr[256];
    fgets(lineStr, sizeof(lineStr), fptr);
    char *ssid = strtok(lineStr, ";\n");
    char *pass = strtok(NULL, ";\n");
    if (ssid && pass) {
        if(strcmp(ssid, jkkRadio.wifiSSID)){
            JkkNvsBlobSet("wifi_ssid", JKK_RADIO_NVS_NAMESPACE, ssid, strlen(ssid) + 1);
        }
        if(strcmp(pass, jkkRadio.wifiPassword)){
            JkkNvsBlobSet("wifi_password", JKK_RADIO_NVS_NAMESPACE, pass, strlen(pass) + 1);
        }
        strcpy(jkkRadio.wifiSSID, ssid);
        strcpy(jkkRadio.wifiPassword, pass);
        ESP_LOGI(TAG, "Read WiFi settings: SSID: %s, Password: %s", ssid, pass);
    }
    fclose(fptr);
    ESP_LOGI(TAG, "WiFi settings: SSID: %s, Password: %s", jkkRadio.wifiSSID, jkkRadio.wifiPassword);
    return ESP_OK;
}

static void JkkRadioStationEmbeddedRead(char const *stationsEmbedded) {
    if (stationsEmbedded == NULL || strlen(stationsEmbedded) == 0) {
        ESP_LOGW(TAG, "No stations provided to read");
        jkkRadio.jkkRadioStations = heap_caps_calloc(JKK_RADIO_MAX_EBMEDDED_STATIONS, sizeof(JkkRadioStations_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (jkkRadio.jkkRadioStations == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for radio stations");
        }
        strncpy(jkkRadio.jkkRadioStations[0].uri, "http://mp3.polskieradio.pl:8904/", sizeof(jkkRadio.jkkRadioStations[0].uri) - 1);
        strncpy(jkkRadio.jkkRadioStations[0].nameLong, "Polskie Radio Program Trzeci", sizeof(jkkRadio.jkkRadioStations[0].nameLong) - 1);
        strncpy(jkkRadio.jkkRadioStations[0].nameShort, "PR3", sizeof(jkkRadio.jkkRadioStations[0].nameShort) - 1);
        jkkRadio.jkkRadioStations[0].is_favorite = false; // Default to not favorite
        jkkRadio.jkkRadioStations[0].type = JKK_RADIO_MUSIC; // Default to unknown type
        jkkRadio.jkkRadioStations[0].audioDes[0] = '\0'; // Default to empty audio description
        ESP_LOGI(TAG, "No stations provided, initialized with default station: %s", jkkRadio.jkkRadioStations[0].nameLong);
        jkkRadio.station_count = 1; // Set station count to 1 for the default station
        return;
    }
    if(jkkRadio.jkkRadioStations) {
        free(jkkRadio.jkkRadioStations);
        jkkRadio.jkkRadioStations = NULL;
    }
    char *stations = heap_caps_calloc(1, strlen(stationsEmbedded) + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (stations == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for stations");
        return;
    }
    strcpy(stations, stationsEmbedded);

    jkkRadio.jkkRadioStations = heap_caps_calloc(JKK_RADIO_MAX_EBMEDDED_STATIONS, sizeof(JkkRadioStations_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (jkkRadio.jkkRadioStations == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for radio stations");
        free(stations);
        return;
    }

    int index = 0;
    char *lineStr[JKK_RADIO_MAX_EBMEDDED_STATIONS] = {NULL};

    lineStr[index] = strtok(stations, "\n");
    // loop through the string to extract all other tokens
    while( lineStr[index] != NULL ) {
        index++;
        if (index >= JKK_RADIO_MAX_EBMEDDED_STATIONS) {
            ESP_LOGW(TAG, "Reached maximum station count (%d), stopping reading from file", JKK_RADIO_MAX_EBMEDDED_STATIONS);
            break; // Stop reading if we reach the maximum count
        }
        lineStr[index] = strtok(NULL, "\n"); // Get the next line
    }

    for(int i = 0; i < JKK_RADIO_MAX_EBMEDDED_STATIONS && lineStr[i] != NULL; i++) {
        char *uri = strtok(lineStr[i], ";\n");
        char *nameShort = strtok(NULL, ";\n");
        char *nameLong = strtok(NULL, ";\n");
        char *is_favorite = strtok(NULL, ";\n");
        char *type = strtok(NULL, ";\n");
        char *audioDes = strtok(NULL, ";\n");       
        if (uri) {
            strncpy(jkkRadio.jkkRadioStations[i].uri, uri, sizeof(jkkRadio.jkkRadioStations[i].uri) - 1);
            if (nameShort) {
                strncpy(jkkRadio.jkkRadioStations[i].nameShort, nameShort, sizeof(jkkRadio.jkkRadioStations[i].nameShort) - 1);
            } else {
                jkkRadio.jkkRadioStations[i].nameShort[0] = '\0'; // Default to empty if not provided
            }
            if(nameLong) {
                strncpy(jkkRadio.jkkRadioStations[i].nameLong, nameLong, sizeof(jkkRadio.jkkRadioStations[i].nameLong) - 1);
            } else {
                jkkRadio.jkkRadioStations[i].nameLong[0] = '\0'; // Default to empty if not provided
            }
            if(is_favorite) {
                jkkRadio.jkkRadioStations[i].is_favorite = (strcmp(is_favorite, "1") == 0);
            } else {
                jkkRadio.jkkRadioStations[i].is_favorite = false; // Default to false if not provided
            }
            if(type) {
                jkkRadio.jkkRadioStations[i].type = atoi(type);
                
            } else {
                jkkRadio.jkkRadioStations[i].type = JKK_RADIO_UNKNOWN; // Default to unknown type
            }
            if(audioDes) {
                strncpy(jkkRadio.jkkRadioStations[i].audioDes, audioDes, sizeof(jkkRadio.jkkRadioStations[i].audioDes) - 1);
            } else {
                jkkRadio.jkkRadioStations[i].audioDes[0] = '\0'; // Default to empty if not provided
            }
        }
    }
    for (int i = 0; i < index; i++) {
        ESP_LOGI(TAG, "Station %d: URI=%s, NameShort=%s, NameLong=%s, Favorite=%s, Type=%d, Audio desc.=%s",
                 i, jkkRadio.jkkRadioStations[i].uri,
                 jkkRadio.jkkRadioStations[i].nameShort,
                 jkkRadio.jkkRadioStations[i].nameLong,
                 jkkRadio.jkkRadioStations[i].is_favorite ? "true" : "false",
                 jkkRadio.jkkRadioStations[i].type,
                 jkkRadio.jkkRadioStations[i].audioDes);
        
        char key[16] = {0};
        sprintf(key, JKK_RADIO_NVS_STATION_KEY, i);
        JkkNvsBlobSet(key, JKK_RADIO_NVS_NAMESPACE, &jkkRadio.jkkRadioStations[i], sizeof(JkkRadioStations_t)); // Save each station to NVS
    }
    free(stations); // Free the temporary string buffer
    jkkRadio.station_count = index;
}

static int JkkRadioStationNvsCount(void){
    nvs_handle_t nvsHandle;
    nvs_type_t type = NVS_TYPE_ANY;
    int stationCount = 0;

    esp_err_t ret = nvs_open(JKK_RADIO_NVS_NAMESPACE, NVS_READONLY, &nvsHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS nvsHandle!\n", esp_err_to_name(ret));
        return ESP_ERR_NVS_INVALID_HANDLE;
    }
    for(uint8_t i = 0; i < JKK_RADIO_MAX_STATIONS; i++) {
        char key[16] = {0};
        sprintf(key, JKK_RADIO_NVS_STATION_KEY, i);
        ret = nvs_find_key(nvsHandle, key, &type);
        if(ret == ESP_OK && type == NVS_TYPE_BLOB) {
            stationCount++;
        } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
            break; // No more stations found
        }
    }

    nvs_close(nvsHandle);
    ESP_LOGI(TAG, "Found %d stations in NVS", stationCount);
    return stationCount;
}

static esp_err_t JkkRadioStationSdRead(void) {
    FILE *fptr;
    int nvsStationCount = JkkRadioStationNvsCount();
    int sdStationCount = 0;

    if(nvsStationCount > 0) {
        if(jkkRadio.jkkRadioStations) free(jkkRadio.jkkRadioStations);
        jkkRadio.jkkRadioStations = heap_caps_calloc(nvsStationCount, sizeof(JkkRadioStations_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (jkkRadio.jkkRadioStations == NULL) {
            ESP_LOGE(TAG, "Memory allocation failed for jkkRadio.jkkRadioStations");
            return ESP_ERR_NO_MEM;
        }
        for(uint8_t i = 0; i < nvsStationCount; i++) {
            char key[16] = {0};
            sprintf(key, JKK_RADIO_NVS_STATION_KEY, i);
            JkkNvsBlobGet(key, JKK_RADIO_NVS_NAMESPACE, &jkkRadio.jkkRadioStations[i], NULL); 
            ESP_LOGI(TAG, "Station from NVS %d: URI=%s, NameShort=%s, NameLong=%s, Favorite=%s, Type=%d, Audio desc.=%s",
                     i, jkkRadio.jkkRadioStations[i].uri,
                     jkkRadio.jkkRadioStations[i].nameShort,
                     jkkRadio.jkkRadioStations[i].nameLong,
                     jkkRadio.jkkRadioStations[i].is_favorite ? "true" : "false",
                     jkkRadio.jkkRadioStations[i].type,
                     jkkRadio.jkkRadioStations[i].audioDes);
        }
        jkkRadio.station_count = nvsStationCount;
    }

    fptr = fopen("/sdcard/stations.txt", "r");
    if (fptr == NULL) {
        ESP_LOGE(TAG, "Error opening file: /sdcard/stations.txt and no stations in NVS");
        if(nvsStationCount == 0) {
            ESP_LOGW(TAG, "No stations found in NVS and /sdcard/stations.txt does not exist, using embedded stations");
            JkkRadioStationEmbeddedRead(stations_start);
        }
        return ESP_ERR_NOT_FOUND;
    }

    char lineStr[512];

    while(fgets(lineStr, sizeof(lineStr), fptr)) {
        if (lineStr[0] == '#' || lineStr[0] == '\n') continue; // Skip comments and empty lines
        sdStationCount++;
    }
    ESP_LOGI(TAG, "Found %d stations in /sdcard/stations.txt", sdStationCount);
    rewind(fptr); // Reset file pointer to the beginning
    if(sdStationCount == 0) {
        ESP_LOGW(TAG, "No stations found in /sdcard/stations.txt");
        fclose(fptr);
        JkkRadioStationEmbeddedRead(stations_start);
        return ESP_ERR_NOT_FOUND;
    }
    else if (sdStationCount > JKK_RADIO_MAX_STATIONS) {
        ESP_LOGW(TAG, "Too many stations (%d) in /sdcard/stations.txt, limiting to JKK_RADIO_MAX_STATIONS", sdStationCount);
        sdStationCount = JKK_RADIO_MAX_STATIONS; // Limit to JKK_RADIO_MAX_STATIONS stations
    }
    if( nvsStationCount < sdStationCount) {
        
        jkkRadio.jkkRadioStations = heap_caps_realloc(jkkRadio.jkkRadioStations, sdStationCount * sizeof(JkkRadioStations_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (jkkRadio.jkkRadioStations == NULL) {
            ESP_LOGE(TAG, "Memory reallocation failed for jkkRadio.jkkRadioStations");
            fclose(fptr);
            return ESP_ERR_NO_MEM;
        }
    }
    
    int index = 0;
    while(fgets(lineStr, sizeof(lineStr), fptr)) {
        if (lineStr[0] == '#' || lineStr[0] == '\n') continue; // Skip comments and empty lines
        char *uri = strtok(lineStr, ";\n");
        char *nameShort = strtok(NULL, ";\n");
        char *nameLong = strtok(NULL, ";\n");
        char *is_favorite = strtok(NULL, ";\n");
        char *type = strtok(NULL, ";\n");
        char *audioDes = strtok(NULL, ";\n");
        
        if (uri) {
            char key[16] = {0};
            sprintf(key, JKK_RADIO_NVS_STATION_KEY, index);
            if(strcmp(uri, jkkRadio.jkkRadioStations[index].uri) || strcmp(nameShort, jkkRadio.jkkRadioStations[index].nameShort) || strcmp(nameLong, jkkRadio.jkkRadioStations[index].nameLong) || jkkRadio.jkkRadioStations[index].is_favorite != (strcmp(is_favorite, "1") == 0)) {
                // If the station is different from the one in NVS, update it
                strncpy(jkkRadio.jkkRadioStations[index].uri, uri, sizeof(jkkRadio.jkkRadioStations[index].uri) - 1);

                if (nameShort) {
                    strncpy(jkkRadio.jkkRadioStations[index].nameShort, nameShort, sizeof(jkkRadio.jkkRadioStations[index].nameShort) - 1);
                } else {
                    jkkRadio.jkkRadioStations[index].nameShort[0] = '\0'; // Default to empty if not provided
                }
                if(nameLong) {
                    strncpy(jkkRadio.jkkRadioStations[index].nameLong, nameLong, sizeof(jkkRadio.jkkRadioStations[index].nameLong) - 1);
                } else {
                    jkkRadio.jkkRadioStations[index].nameLong[0] = '\0'; // Default to empty if not provided
                }
                if(is_favorite) {
                    jkkRadio.jkkRadioStations[index].is_favorite = (strcmp(is_favorite, "1") == 0);
                } else {
                    jkkRadio.jkkRadioStations[index].is_favorite = false; // Default to false if not provided
                }
                if(type) {
                    jkkRadio.jkkRadioStations[index].type = atoi(type);
                    
                } else {
                    jkkRadio.jkkRadioStations[index].type = JKK_RADIO_UNKNOWN; // Default to unknown type
                }
                if(audioDes) {
                    strncpy(jkkRadio.jkkRadioStations[index].audioDes, audioDes, sizeof(jkkRadio.jkkRadioStations[index].audioDes) - 1);
                } else {
                    jkkRadio.jkkRadioStations[index].audioDes[0] = '\0'; // Default to empty if not provided
                }
                JkkNvsBlobSet(key, JKK_RADIO_NVS_NAMESPACE, &jkkRadio.jkkRadioStations[index], sizeof(JkkRadioStations_t)); // Save each station to NVS
                ESP_LOGI(TAG, "Updated station %d from file: URI=%s, NameShort=%s, NameLong=%s, Favorite=%s, Type=%d, Audio desc.=%s",
                         index,
                         jkkRadio.jkkRadioStations[index].uri,
                         jkkRadio.jkkRadioStations[index].nameShort,
                         jkkRadio.jkkRadioStations[index].nameLong,
                         jkkRadio.jkkRadioStations[index].is_favorite ? "true" : "false",
                         jkkRadio.jkkRadioStations[index].type,
                         jkkRadio.jkkRadioStations[index].audioDes);
            }
            index++;
            if (index >= sdStationCount) {
                ESP_LOGI(TAG, "Reached end of station list(%d), stopping reading from file", sdStationCount);
                break; // Stop reading if we reach the maximum count
            }
        }
    }
    if(index < nvsStationCount) {
        // If there are more stations in NVS than in the file, remove the extra ones
        for(int i = index; i < nvsStationCount; i++) {
            char key[16] = {0};
            sprintf(key, JKK_RADIO_NVS_STATION_KEY, i);
            ESP_LOGI(TAG, "Removing station %d from NVS: %s", i, key);
            JkkNvsErase(key, JKK_RADIO_NVS_NAMESPACE);
        }
    }
    if(index < nvsStationCount) {
        // If there are more stations in the file than in NVS, fill the rest with empty stations
        for(int i = index; i < sdStationCount; i++) {
            jkkRadio.jkkRadioStations[i].uri[0] = '\0';
            jkkRadio.jkkRadioStations[i].nameShort[0] = '\0';
            jkkRadio.jkkRadioStations[i].nameLong[0] = '\0';
            jkkRadio.jkkRadioStations[i].is_favorite = false;
            jkkRadio.jkkRadioStations[i].type = JKK_RADIO_UNKNOWN;
            jkkRadio.jkkRadioStations[i].audioDes[0] = '\0';
        }
    }
    fclose(fptr);
    ESP_LOGI(TAG, "Loaded %d stations from /sdcard/stations.txt", index);

    ESP_LOGI(TAG, "Loaded stations:");
    for (int i = 0; i < index; i++) {
        ESP_LOGI(TAG, "Station %d: URI=%s, NameShort=%s, NameLong=%s, Favorite=%s, Type=%d, Audio desc.=%s",
                 i,
                 jkkRadio.jkkRadioStations[i].uri,
                 jkkRadio.jkkRadioStations[i].nameShort,
                 jkkRadio.jkkRadioStations[i].nameLong,
                 jkkRadio.jkkRadioStations[i].is_favorite ? "true" : "false",
                 jkkRadio.jkkRadioStations[i].type,
                 jkkRadio.jkkRadioStations[i].audioDes);     
    }
    jkkRadio.station_count = index;
    return ESP_OK;
}

void app_main(void){
    jkkRadio.player_volume = 15;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());

    jkkRadio.disp_serv = audio_board_led_init();

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("ledc", ESP_LOG_ERROR);

    jkkRadio.current_station = 0;
    jkkRadio.current_eq = 0;

    ESP_LOGI(TAG, "Start audio codec chip");
    jkkRadio.board_handle = audio_board_init();
    audio_hal_ctrl_codec(jkkRadio.board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);
    audio_hal_set_volume(jkkRadio.board_handle->audio_hal, 10);

    jkkRadio.audioMain = JkkAudioMain_init(3, 1, 1, 1); // in/out type: 3 - HTTP, 1 - I2S; processing type: 1 - EQUALIZER, 1 - RAW_SPLIT; split nr

    jkkRadio.audioSdWrite = JkkAudioSdWrite_init(1, 22050, 2); // 1 - AAC, sample_rate, channels

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    jkkRadio.set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "Start and wait for SDCARD to mount");
    audio_board_sdcard_init(jkkRadio.set, SD_MODE_1_LINE);

    JkkRadioSettingsRead();
    JkkRadioStationSdRead();

    ESP_LOGI(TAG, "Start and wait for Wi-Fi network");

    periph_wifi_cfg_t wifi_cfg = {
        .wifi_config.sta.ssid = CONFIG_WIFI_SSID, 
        .wifi_config.sta.password = CONFIG_WIFI_PASSWORD,
    };

    if(jkkRadio.wifiSSID[0] != '\0'){
        strncpy((char *)wifi_cfg.wifi_config.sta.ssid, jkkRadio.wifiSSID, sizeof(wifi_cfg.wifi_config.sta.ssid) - 1);
    } 
    if(jkkRadio.wifiPassword[0] != '\0'){
        strncpy((char *)wifi_cfg.wifi_config.sta.password, jkkRadio.wifiPassword, sizeof(wifi_cfg.wifi_config.sta.password) - 1);
    }
    jkkRadio.wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(jkkRadio.set, jkkRadio.wifi_handle);
    periph_wifi_wait_for_connected(jkkRadio.wifi_handle, portMAX_DELAY);

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
    JkkAudioSetUrl(jkkRadio.jkkRadioStations[0].uri, false);
    display_service_set_pattern(jkkRadio.disp_serv, JKK_DISPLAY_PATTERN_BR_PULSE + 1, 1);

    ESP_LOGI(TAG, "Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    jkkRadio.evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "Listening event from all elements of jkkRadio.audioMain->pipeline");
    audio_pipeline_set_listener(jkkRadio.audioMain->pipeline, jkkRadio.evt);
    audio_pipeline_set_listener(jkkRadio.audioSdWrite->pipeline, jkkRadio.evt);

    ESP_LOGI(TAG, "Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(jkkRadio.set), jkkRadio.evt);

    ESP_LOGI(TAG, "Start audio_pipeline");
    audio_pipeline_run(jkkRadio.audioMain->pipeline);

    JkkAudioEqSetAll(eq_Gains[jkkRadio.current_eq]);

    while (1) {
        audio_event_iface_msg_t msg = {0};
        esp_err_t ret = audio_event_iface_listen(jkkRadio.evt, &msg, pdMS_TO_TICKS(3000)); // portMAX_DELAY
        if (ret != ESP_OK) {
            audio_element_state_t sdState = audio_element_get_state(jkkRadio.audioSdWrite->fatfs_wr);
           // audio_element_state_t inState = audio_element_get_state(jkkRadio.audioMain->input);
          //  ESP_LOGW(TAG, "[ Uncnow ] fatfs_wr state: %d, inState: %d", sdState, inState);
            jkkRadio.audioSdWrite->is_recording = (sdState == AEL_STATE_RUNNING);
            if(jkkRadio.audioSdWrite->is_recording){
                display_service_set_pattern(jkkRadio.disp_serv, DISPLAY_PATTERN_RECORDING_START, 1);
            }

            if(msg.source_type == 0 && msg.cmd == 0){
                continue;
            }
        }
      //  ESP_LOGI(TAG, "[ any ] msg.source_type=%d, msg.cmd=%d, Pointer: %p", msg.source_type, msg.cmd, msg.source);

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *)jkkRadio.audioSdWrite->fatfs_wr
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && msg.data_len == 4
            && (int)msg.data == AEL_STATUS_ERROR_OUTPUT) {
            
            ESP_LOGW(TAG, "Stop write stream"); 
            display_service_set_pattern(jkkRadio.disp_serv, DISPLAY_PATTERN_RECORDING_STOP, 1);
            JkkAudioSdWriteStopStream();
            continue;
        }

        if (msg.source_type == PERIPH_ID_SDCARD && msg.cmd == AEL_MSG_CMD_FINISH) {  
            JkkRadioSettingsRead();
            JkkRadioStationSdRead();
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *)jkkRadio.audioMain->decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            static audio_element_info_t prev_music_info = {0};
            audio_element_info_t music_info = {0};
            audio_element_getinfo(jkkRadio.audioMain->decoder, &music_info);

            ESP_LOGI(TAG, "Receive music info from dec decoder, sample_rates=%d, bits=%d, ch=%d", music_info.sample_rates, music_info.bits, music_info.channels);

            if ((prev_music_info.bits != music_info.bits) || (prev_music_info.sample_rates != music_info.sample_rates)
                || (prev_music_info.channels != music_info.channels)) {
                ESP_LOGI(TAG, "Change sample_rates=%d, bits=%d, ch=%d",  music_info.sample_rates, music_info.bits, music_info.channels);
                JkkAudioI2sSetClk(music_info.sample_rates, music_info.bits, music_info.channels, true);
                JkkAudioEqSetInfo(music_info.sample_rates, music_info.channels);
                JkkAudioSdWriteResChange(music_info.sample_rates, music_info.channels, music_info.bits);
                memcpy(&prev_music_info, &music_info, sizeof(audio_element_info_t));
            }
           // JkkAudioSdWriteRestartStream(false);

            audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, true);
            continue;
        }

        /* restart stream when the first jkkRadio.audioMain->pipeline element (http_stream_reader in this case) receives stop event (caused by reading errors) */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) jkkRadio.audioMain->input
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (int)msg.data == AEL_STATUS_ERROR_OPEN) {
            ESP_LOGW(TAG, "Restart stream");
            JkkAudioRestartStream();
            continue;
        }

        if (msg.source_type == PERIPH_ID_BUTTON && (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_PRESSED)) {
            if ((int)msg.data == get_input_play_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);
                    JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_PREV);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);
                    JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_FAV);
                }
            } else if ((int)msg.data == get_input_set_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);
                    JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_NEXT);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);
                    JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_FIRST);
                }
            } else if ((int)msg.data == get_input_mode_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    JkkChangeEq(1);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    JkkChangeEq(0);
                }
            } else if ((int)msg.data == get_input_rec_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    audio_element_state_t sdState = audio_element_get_state(jkkRadio.audioSdWrite->fatfs_wr);
                    jkkRadio.audioSdWrite->is_recording = (sdState == AEL_STATE_RUNNING);
                    if(jkkRadio.audioSdWrite->is_recording){
                        display_service_set_pattern(jkkRadio.disp_serv, DISPLAY_PATTERN_RECORDING_START, 1);
                        continue;
                    }

                    char folderPath[32];
                    time_t now = 0;
                    time(&now);
                    JkkMakePath(now, folderPath, NULL);
                    esp_err_t ret = ESP_OK;
                    ret = mkdir(folderPath, 0777);
                    if (ret != 0 && errno != EEXIST) {
                        ESP_LOGE(TAG, "Mkdir directory: %s, failed with errno: %d/%s", folderPath, errno, strerror(errno));
                    }
                    else{
                        char filePath[48] = {0};
                        JkkMakePath(now, filePath, "aac");
                        ret = JkkAudioSdWriteStartStream(filePath);
                        if(ret == ESP_OK) {
                            JkkSdRecInfoWrite(now, folderPath, filePath);
                            display_service_set_pattern(jkkRadio.disp_serv, DISPLAY_PATTERN_RECORDING_START, 1);
                        }
                    }
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    display_service_set_pattern(jkkRadio.disp_serv, DISPLAY_PATTERN_RECORDING_STOP, 1);
                    JkkAudioSdWriteStopStream();
                }
             }else if ((int)msg.data == get_input_volup_id() && (msg.cmd == PERIPH_BUTTON_RELEASE)) {
                ESP_LOGI(TAG, "[Vol+] touch tap event");
                jkkRadio.player_volume += jkkRadio.player_volume < 40 ? 5 : 10;
                if (jkkRadio.player_volume > 100) {
                    jkkRadio.player_volume = 100;
                }
                audio_hal_set_volume(jkkRadio.board_handle->audio_hal, jkkRadio.player_volume);
                ESP_LOGI(TAG, "Volume jkkRadio.set to %d%%", jkkRadio.player_volume);

              //  cli_get_mp3_id3_info(audio_decoder);
            } else if ((int)msg.data == get_input_voldown_id()) {
                ESP_LOGI(TAG, "[Vol-] touch tap event");
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    jkkRadio.player_volume -= jkkRadio.player_volume < 40 ? 5 : 10;
                    if (jkkRadio.player_volume < 0) {
                        jkkRadio.player_volume = 0;
                    }
                    audio_hal_set_volume(jkkRadio.board_handle->audio_hal, jkkRadio.player_volume);
                    ESP_LOGI(TAG, "Volume jkkRadio.set to %d%%", jkkRadio.player_volume);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    jkkRadio.player_volume = 0;
                    audio_hal_set_volume(jkkRadio.board_handle->audio_hal, 0);
                    ESP_LOGI(TAG, "Volume jkkRadio.set MUTED");
                }
            }
        }
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
