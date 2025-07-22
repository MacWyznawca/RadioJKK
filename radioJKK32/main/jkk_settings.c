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
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "jkk_radio.h"
#include "jkk_nvs.h"

#include "display/jkk_mono_lcd.h"

#include "jkk_settings.h"

#define JKK_RADIO_WEB_SERVER_OFF "wwwoff"

static const char *TAG = "JKK_SETTI";

extern const char stations_start[] asm("_binary_stations_txt_start"); 

static const JkkRadioEqualizer_t eq_embedded[JKK_RADIO_MAX_EBMEDDED_EQ_PRESETS] = {
    {
        .name = "flat",
        .gain = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    }, 
    {
        .name = "music",
        .gain = {2,3,1,0,-1,-2,0,1,2,0},
    }, 
    {
        .name = "rock",
        .gain = {4,5,3,1,-1,-3,-1,3,4,0},
    }, 
    {
        .name = "speak",
        .gain = {-3,-2,1,3,3,1,0,-1,-2,0},
    }, 
};

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
static void JkkLcdReloadRoller(JkkRadio_t *jkkRadio){
    char *lcdRollerOptions = NULL;

    if(JkkLcdRollerMode() == JKK_ROLLER_MODE_STATION_LIST){
        ESP_LOGI(TAG, "Loaded stations:");
        lcdRollerOptions = heap_caps_calloc(jkkRadio->station_count, 10, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        for (int i = 0; i < jkkRadio->station_count; i++) {
            strncat(lcdRollerOptions, jkkRadio->jkkRadioStations[i].nameShort, 8);
            if(i < jkkRadio->station_count - 1) strcat(lcdRollerOptions, "\n");
        }
        JkkLcdSetRollerOptions(lcdRollerOptions, jkkRadio->current_station);
    }
    else if(JkkLcdRollerMode() == JKK_ROLLER_MODE_EQUALIZER_LIST){
        ESP_LOGI(TAG, "Loaded equs:");
        lcdRollerOptions = heap_caps_calloc(jkkRadio->eq_count, 10, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        for (int i = 0; i < jkkRadio->eq_count; i++) {
            strncat(lcdRollerOptions, jkkRadio->eqPresets[i].name, 8);
            if(i < jkkRadio->eq_count - 1) strcat(lcdRollerOptions, "\n");
        }
        JkkLcdSetRollerOptions(lcdRollerOptions, jkkRadio->current_eq);
    }
    if(lcdRollerOptions) free(lcdRollerOptions);
}
#endif

esp_err_t JkkRadioSettingsRead(JkkRadio_t *jkkRadio) {
    if (jkkRadio == NULL) {
        ESP_LOGE(TAG, "Invalid arguments: wifiSSID or wifiPassword is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    FILE *fptr;

    JkkNvsBlobGet("wifi_ssid", JKK_RADIO_NVS_NAMESPACE, jkkRadio->wifiSSID, NULL); // &(size_t){sizeof(wifiSSID)}
    JkkNvsBlobGet("wifi_password", JKK_RADIO_NVS_NAMESPACE, jkkRadio->wifiPassword, NULL); // &(size_t){sizeof(wifiPassword)}

    ESP_LOGI(TAG, "WiFi settings from NVS: SSID: %s, Password: %s", jkkRadio->wifiSSID, jkkRadio->wifiPassword);

    jkkRadio->runWebServer = true;

    fptr = fopen("/sdcard/settings.txt", "r");
    if (fptr == NULL) {
        ESP_LOGE(TAG, "Error opening file: /sdcard/settings.txt");
        return ESP_ERR_NOT_FOUND;
    }
    char lineStr[256];
    fgets(lineStr, sizeof(lineStr), fptr);
    char *ssid = strtok(lineStr, ";\n");
    char *pass = strtok(NULL, ";\n");
    char *webServer = strtok(NULL, ";\n");
    if (ssid && pass) {
        if(strcmp(ssid, jkkRadio->wifiSSID)){
            JkkNvsBlobSet("wifi_ssid", JKK_RADIO_NVS_NAMESPACE, ssid, strlen(ssid) + 1);
        }
        if(strcmp(pass, jkkRadio->wifiPassword)){
            JkkNvsBlobSet("wifi_password", JKK_RADIO_NVS_NAMESPACE, pass, strlen(pass) + 1);
        }
        strcpy(jkkRadio->wifiSSID, ssid);
        strcpy(jkkRadio->wifiPassword, pass);
        ESP_LOGI(TAG, "Read WiFi settings: SSID: %s, Password: %s", ssid, pass);
    }
    if(webServer) {
        if(strcmp(webServer, JKK_RADIO_WEB_SERVER_OFF) == 0){
            jkkRadio->runWebServer = false;
        }
        ESP_LOGI(TAG, "Read Web Server: %s", webServer);
    }
    fclose(fptr);
    ESP_LOGI(TAG, "WiFi settings: SSID: %s, Password: %s", jkkRadio->wifiSSID, jkkRadio->wifiPassword);
    return ESP_OK;
}

static void JkkRadioStationEmbeddedRead(JkkRadio_t *jkkRadio, char const *stationsEmbedded) {
    if (stationsEmbedded == NULL || strlen(stationsEmbedded) == 0) {
        ESP_LOGW(TAG, "No stations provided to read");
        jkkRadio->jkkRadioStations = heap_caps_calloc(JKK_RADIO_MAX_EBMEDDED_STATIONS, sizeof(JkkRadioStations_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (jkkRadio->jkkRadioStations == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for radio stations");
        }
        strncpy(jkkRadio->jkkRadioStations[0].uri, "http://mp3.polskieradio.pl:8904/", sizeof(jkkRadio->jkkRadioStations[0].uri) - 1);
        strncpy(jkkRadio->jkkRadioStations[0].nameLong, "Polskie Radio Program Trzeci", sizeof(jkkRadio->jkkRadioStations[0].nameLong) - 1);
        strncpy(jkkRadio->jkkRadioStations[0].nameShort, "PR3", sizeof(jkkRadio->jkkRadioStations[0].nameShort) - 1);
        jkkRadio->jkkRadioStations[0].is_favorite = false; // Default to not favorite
        jkkRadio->jkkRadioStations[0].type = JKK_RADIO_MUSIC; // Default to unknown type
        jkkRadio->jkkRadioStations[0].audioDes[0] = '\0'; // Default to empty audio description
        ESP_LOGI(TAG, "No stations provided, initialized with default station: %s", jkkRadio->jkkRadioStations[0].nameLong);
        jkkRadio->station_count = 1; // Set station count to 1 for the default station
        JkkRadioListForWWW();
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
        JkkLcdReloadRoller(jkkRadio);
#endif
        return;
    }
    if(jkkRadio->jkkRadioStations) {
        free(jkkRadio->jkkRadioStations);
        jkkRadio->jkkRadioStations = NULL;
    }
    char *stations = heap_caps_calloc(1, strlen(stationsEmbedded) + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (stations == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for stations");
        return;
    }
    strcpy(stations, stationsEmbedded);

    jkkRadio->jkkRadioStations = heap_caps_calloc(JKK_RADIO_MAX_EBMEDDED_STATIONS, sizeof(JkkRadioStations_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (jkkRadio->jkkRadioStations == NULL) {
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
            strncpy(jkkRadio->jkkRadioStations[i].uri, uri, sizeof(jkkRadio->jkkRadioStations[i].uri) - 1);
            if (nameShort) {
                strncpy(jkkRadio->jkkRadioStations[i].nameShort, nameShort, sizeof(jkkRadio->jkkRadioStations[i].nameShort) - 1);
            } else {
                jkkRadio->jkkRadioStations[i].nameShort[0] = '\0'; // Default to empty if not provided
            }
            if(nameLong) {
                strncpy(jkkRadio->jkkRadioStations[i].nameLong, nameLong, sizeof(jkkRadio->jkkRadioStations[i].nameLong) - 1);
            } else {
                jkkRadio->jkkRadioStations[i].nameLong[0] = '\0'; // Default to empty if not provided
            }
            if(is_favorite) {
                jkkRadio->jkkRadioStations[i].is_favorite = (strcmp(is_favorite, "1") == 0);
            } else {
                jkkRadio->jkkRadioStations[i].is_favorite = false; // Default to false if not provided
            }
            if(type) {
                jkkRadio->jkkRadioStations[i].type = atoi(type);
                
            } else {
                jkkRadio->jkkRadioStations[i].type = JKK_RADIO_UNKNOWN; // Default to unknown type
            }
            if(audioDes) {
                strncpy(jkkRadio->jkkRadioStations[i].audioDes, audioDes, sizeof(jkkRadio->jkkRadioStations[i].audioDes) - 1);
            } else {
                jkkRadio->jkkRadioStations[i].audioDes[0] = '\0'; // Default to empty if not provided
            }
        }
    }
    for (int i = 0; i < index; i++) {
        ESP_LOGI(TAG, "Station %d: URI=%s, NameShort=%s, NameLong=%s, Favorite=%s, Type=%d, Audio desc.=%s",
                 i, jkkRadio->jkkRadioStations[i].uri,
                 jkkRadio->jkkRadioStations[i].nameShort,
                 jkkRadio->jkkRadioStations[i].nameLong,
                 jkkRadio->jkkRadioStations[i].is_favorite ? "true" : "false",
                 jkkRadio->jkkRadioStations[i].type,
                 jkkRadio->jkkRadioStations[i].audioDes);
        
        char key[16] = {0};
        sprintf(key, JKK_RADIO_NVS_STATION_KEY, i);
        JkkNvsBlobSet(key, JKK_RADIO_NVS_NAMESPACE, &jkkRadio->jkkRadioStations[i], sizeof(JkkRadioStations_t)); // Save each station to NVS
    }
    free(stations); // Free the temporary string buffer
    jkkRadio->station_count = index;
    JkkRadioListForWWW();
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    JkkLcdReloadRoller(jkkRadio);
#endif
}

static int JkkRadioStationNvsCount(void){
    nvs_handle_t nvsHandle;
    nvs_type_t type = NVS_TYPE_ANY;
    int stationCount = 0;

    esp_err_t ret = nvs_open(JKK_RADIO_NVS_NAMESPACE, NVS_READONLY, &nvsHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS nvsHandle!\n", esp_err_to_name(ret));
        return 0;
    }
    for(uint8_t i = 0; i < JKK_RADIO_MAX_STATIONS; i++) {
        char key[16] = {0};
        sprintf(key, JKK_RADIO_NVS_STATION_KEY, i);
        ret = nvs_find_key(nvsHandle, key, &type);
        if(ret == ESP_OK && type == NVS_TYPE_BLOB) {
            stationCount++;
        } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
            continue; // No more stations found
        }
    }

    nvs_close(nvsHandle);
    ESP_LOGI(TAG, "Found %d stations in NVS", stationCount);
    return stationCount;
}

esp_err_t JkkRadioStationSdRead(JkkRadio_t *jkkRadio) {
    FILE *fptr;
    int nvsStationCount = JkkRadioStationNvsCount();
    int sdStationCount = 0;

    if(nvsStationCount > 0) {
        if(jkkRadio->jkkRadioStations) {
            free(jkkRadio->jkkRadioStations);
            jkkRadio->jkkRadioStations = NULL;
        }
        jkkRadio->jkkRadioStations = heap_caps_calloc(nvsStationCount, sizeof(JkkRadioStations_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (jkkRadio->jkkRadioStations == NULL) {
            ESP_LOGE(TAG, "Memory allocation failed for jkkRadio->jkkRadioStations");
            return ESP_ERR_NO_MEM;
        }
        nvsStationCount = 0; // Reset count to fill from NVS
        for(uint8_t i = 0; i < JKK_RADIO_MAX_STATIONS; i++) {
            char key[16] = {0};
            sprintf(key, JKK_RADIO_NVS_STATION_KEY, i);
            if(JkkNvsBlobGet(key, JKK_RADIO_NVS_NAMESPACE, &jkkRadio->jkkRadioStations[nvsStationCount], NULL) == ESP_OK) {
                ESP_LOGI(TAG, "Station from NVS %d: URI=%s, NameShort=%s, NameLong=%s, Favorite=%s, Type=%d, Audio desc.=%s",
                     i, jkkRadio->jkkRadioStations[nvsStationCount].uri,
                     jkkRadio->jkkRadioStations[nvsStationCount].nameShort,
                     jkkRadio->jkkRadioStations[nvsStationCount].nameLong,
                     jkkRadio->jkkRadioStations[nvsStationCount].is_favorite ? "true" : "false",
                     jkkRadio->jkkRadioStations[nvsStationCount].type,
                     jkkRadio->jkkRadioStations[nvsStationCount].audioDes);
                nvsStationCount++;
            } else {
                continue; // No more stations found
            }
        }
        
        jkkRadio->station_count = nvsStationCount;
    }

    fptr = fopen("/sdcard/stations.txt", "r");
    if (fptr == NULL) {
        ESP_LOGE(TAG, "Error opening file: /sdcard/stations.txt and no stations in NVS");
        if(nvsStationCount == 0) {
            ESP_LOGW(TAG, "No stations found in NVS and /sdcard/stations.txt does not exist, using embedded stations");
            JkkRadioStationEmbeddedRead(jkkRadio, stations_start);
        }
        JkkRadioListForWWW();
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
        JkkLcdReloadRoller(jkkRadio);
#endif
        return ESP_OK;
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
        JkkRadioStationEmbeddedRead(jkkRadio, stations_start);
        return ESP_ERR_NOT_FOUND;
    }
    else if (sdStationCount > JKK_RADIO_MAX_STATIONS) {
        ESP_LOGW(TAG, "Too many stations (%d) in /sdcard/stations.txt, limiting to JKK_RADIO_MAX_STATIONS", sdStationCount);
        sdStationCount = JKK_RADIO_MAX_STATIONS; // Limit to JKK_RADIO_MAX_STATIONS stations
    }
    if( nvsStationCount < sdStationCount) {
        
        jkkRadio->jkkRadioStations = heap_caps_realloc(jkkRadio->jkkRadioStations, sdStationCount * sizeof(JkkRadioStations_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (jkkRadio->jkkRadioStations == NULL) {
            ESP_LOGE(TAG, "Memory reallocation failed for jkkRadio->jkkRadioStations");
            fclose(fptr);
            return ESP_ERR_NO_MEM;
        }
    }
    jkkRadio->radioStationChanged = false;
    
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
            
            if(jkkRadio->jkkRadioStations[index].addFrom != JKK_RADIO_ADD_FROM_WEB && (strcmp(uri, jkkRadio->jkkRadioStations[index].uri) || (nameShort && strcmp(nameShort, jkkRadio->jkkRadioStations[index].nameShort)) || (nameLong && strcmp(nameLong, jkkRadio->jkkRadioStations[index].nameLong)) || jkkRadio->jkkRadioStations[index].is_favorite != (is_favorite && strcmp(is_favorite, "1") == 0))) {
                // If the station is different from the one in NVS, update it
                jkkRadio->radioStationChanged = true;

                strncpy(jkkRadio->jkkRadioStations[index].uri, uri, sizeof(jkkRadio->jkkRadioStations[index].uri) - 1);

                if (nameShort) {
                    strncpy(jkkRadio->jkkRadioStations[index].nameShort, nameShort, sizeof(jkkRadio->jkkRadioStations[index].nameShort) - 1);
                } else {
                    jkkRadio->jkkRadioStations[index].nameShort[0] = '\0'; // Default to empty if not provided
                }
                if(nameLong) {
                    strncpy(jkkRadio->jkkRadioStations[index].nameLong, nameLong, sizeof(jkkRadio->jkkRadioStations[index].nameLong) - 1);
                } else {
                    jkkRadio->jkkRadioStations[index].nameLong[0] = '\0'; // Default to empty if not provided
                }
                if(is_favorite) {
                    jkkRadio->jkkRadioStations[index].is_favorite = (strcmp(is_favorite, "1") == 0);
                } else {
                    jkkRadio->jkkRadioStations[index].is_favorite = false; // Default to false if not provided
                }
                if(type) {
                    jkkRadio->jkkRadioStations[index].type = atoi(type);
                    
                } else {
                    jkkRadio->jkkRadioStations[index].type = JKK_RADIO_UNKNOWN; // Default to unknown type
                }
                if(audioDes) {
                    strncpy(jkkRadio->jkkRadioStations[index].audioDes, audioDes, sizeof(jkkRadio->jkkRadioStations[index].audioDes) - 1);
                } else {
                    jkkRadio->jkkRadioStations[index].audioDes[0] = '\0'; // Default to empty if not provided
                }
                jkkRadio->jkkRadioStations[index].addFrom = JKK_RADIO_ADD_FROM_SD; // Mark as added from SD card
                JkkNvsBlobSet(key, JKK_RADIO_NVS_NAMESPACE, &jkkRadio->jkkRadioStations[index], sizeof(JkkRadioStations_t)); // Save each station to NVS
                ESP_LOGI(TAG, "Updated station %d from file: URI=%s, NameShort=%s, NameLong=%s, Favorite=%s, Type=%d, Audio desc.=%s",
                         index,
                         jkkRadio->jkkRadioStations[index].uri,
                         jkkRadio->jkkRadioStations[index].nameShort,
                         jkkRadio->jkkRadioStations[index].nameLong,
                         jkkRadio->jkkRadioStations[index].is_favorite ? "true" : "false",
                         jkkRadio->jkkRadioStations[index].type,
                         jkkRadio->jkkRadioStations[index].audioDes);
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
            if(jkkRadio->jkkRadioStations[index].nameShort[0] != '\0' && jkkRadio->jkkRadioStations[index].uri[0] != '\0') {
                if(jkkRadio->jkkRadioStations[index].addFrom != JKK_RADIO_ADD_FROM_WEB){
                    JkkNvsErase(key, JKK_RADIO_NVS_NAMESPACE);
                }
                else {
                    index++;
                }
            }
        }
    }

    fclose(fptr);
    ESP_LOGI(TAG, "Loaded %d stations from /sdcard/stations.txt", index);

    ESP_LOGI(TAG, "Loaded stations:");
    for (int i = 0; i < index; i++) {
        ESP_LOGI(TAG, "Station %d: URI=%s, NameShort=%s, NameLong=%s, Favorite=%s, Type=%d, Audio desc.=%s",
                 i,
                 jkkRadio->jkkRadioStations[i].uri,
                 jkkRadio->jkkRadioStations[i].nameShort,
                 jkkRadio->jkkRadioStations[i].nameLong,
                 jkkRadio->jkkRadioStations[i].is_favorite ? "true" : "false",
                 jkkRadio->jkkRadioStations[i].type,
                 jkkRadio->jkkRadioStations[i].audioDes);   
    }
    jkkRadio->station_count = index;
    JkkRadioListForWWW();
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    JkkLcdReloadRoller(jkkRadio);
#endif
    return ESP_OK;
}

static int JkkRadioEqNvsCount(void){
    nvs_handle_t nvsHandle;
    nvs_type_t type = NVS_TYPE_ANY;
    int eqCount = 0;

    esp_err_t ret = nvs_open(JKK_RADIO_NVS_EQ_NAMESPACE, NVS_READONLY, &nvsHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS nvsHandle!\n", esp_err_to_name(ret));
        return 0;
    }
    for(uint8_t i = 0; i < JKK_RADIO_MAX_EQ_PRESETS; i++) {
        char key[16] = {0};
        sprintf(key, JKK_RADIO_NVS_EQUALIZER_KEY, i);
        ret = nvs_find_key(nvsHandle, key, &type);
        if(ret == ESP_OK && type == NVS_TYPE_BLOB) {
            eqCount++;
        } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
            break; // No more stations found
        }
    }

    nvs_close(nvsHandle);
    ESP_LOGI(TAG, "Found %d equalizers in NVS", eqCount);
    return eqCount;
}


esp_err_t JkkRadioEqSdRead(JkkRadio_t *jkkRadio) {
    FILE *fptr;
    int nvsEqCount = JkkRadioEqNvsCount();
    int sdEqCount = 0;

    if(jkkRadio->eqPresets) {
        free(jkkRadio->eqPresets);
        jkkRadio->eqPresets = NULL;
    }

    if(nvsEqCount > 0) {
        jkkRadio->eqPresets = heap_caps_calloc(nvsEqCount, sizeof(JkkRadioEqualizer_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (jkkRadio->eqPresets == NULL) {
            ESP_LOGE(TAG, "Memory allocation failed for jkkRadio->eqPresets");
            return ESP_ERR_NO_MEM;
        }
        for(uint8_t i = 0; i < nvsEqCount; i++) {
            char key[16] = {0};
            sprintf(key, JKK_RADIO_NVS_EQUALIZER_KEY, i);
            JkkNvsBlobGet(key, JKK_RADIO_NVS_EQ_NAMESPACE, &jkkRadio->eqPresets[i], NULL); 
            ESP_LOGI(TAG, "Equalizer from NVS %d: name=%s",
                     i, jkkRadio->eqPresets[i].name);
        }
        jkkRadio->eq_count = nvsEqCount;
    }

    fptr = fopen("/sdcard/eq.txt", "r");
    if (fptr == NULL) {
        ESP_LOGE(TAG, "Error opening file: /sdcard/eq.txt and no equalizers in NVS");
        if(nvsEqCount == 0) {
            ESP_LOGW(TAG, "No equalizers found in NVS and /sdcard/eq.txt does not exist, using embedded equalizers");
            jkkRadio->eqPresets = heap_caps_calloc(JKK_RADIO_MAX_EBMEDDED_EQ_PRESETS, sizeof(JkkRadioEqualizer_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            for(int i = 0; i < JKK_RADIO_MAX_EBMEDDED_EQ_PRESETS; i++){
                memcpy(&jkkRadio->eqPresets[i], &eq_embedded[i], sizeof(JkkRadioEqualizer_t));
            }
            jkkRadio->eq_count = JKK_RADIO_MAX_EBMEDDED_EQ_PRESETS;
        }
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
        JkkLcdReloadRoller(jkkRadio);
#endif
        return ESP_OK;
    }

    char lineStr[128];

    while(fgets(lineStr, sizeof(lineStr), fptr)) {
        if (lineStr[0] == '#' || lineStr[0] == '\n') continue; // Skip comments and empty lines
        sdEqCount++;
    }
    ESP_LOGI(TAG, "Found %d equalizers in /sdcard/eq.txt", sdEqCount);
    rewind(fptr); // Reset file pointer to the beginning
    if(sdEqCount == 0) {
        ESP_LOGW(TAG, "No equalizers found in /sdcard/eq.txt");
        fclose(fptr);
        jkkRadio->eqPresets = heap_caps_calloc(JKK_RADIO_MAX_EBMEDDED_EQ_PRESETS, sizeof(JkkRadioEqualizer_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        for(int i = 0; i < JKK_RADIO_MAX_EBMEDDED_EQ_PRESETS; i++){
            memcpy(&jkkRadio->eqPresets[i], &eq_embedded[i], sizeof(JkkRadioEqualizer_t));
        }
        jkkRadio->eq_count = JKK_RADIO_MAX_EBMEDDED_EQ_PRESETS;
        return ESP_ERR_NOT_FOUND;
    }
    else if (sdEqCount > JKK_RADIO_MAX_EQ_PRESETS) {
        ESP_LOGW(TAG, "Too many equalizers (%d) in /sdcard/eq.txt, limiting to JKK_RADIO_MAX_EQ_PRESETS", sdEqCount);
        sdEqCount = JKK_RADIO_MAX_EQ_PRESETS; // Limit to JKK_RADIO_MAX_EQ_PRESETS equalizers
    }
    if( nvsEqCount < sdEqCount) {
        if(jkkRadio->eqPresets) jkkRadio->eqPresets = heap_caps_realloc(jkkRadio->eqPresets, sdEqCount * sizeof(JkkRadioEqualizer_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        else jkkRadio->eqPresets = heap_caps_calloc(sdEqCount, sizeof(JkkRadioEqualizer_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (jkkRadio->eqPresets == NULL) {
            ESP_LOGE(TAG, "Memory reallocation failed for jkkRadio->eqPresets");
            fclose(fptr);
            return ESP_ERR_NO_MEM;
        }
    }
    
    int index = 0;
    while(fgets(lineStr, sizeof(lineStr), fptr)) {
        if (lineStr[0] == '#' || lineStr[0] == '\n') continue; // Skip comments and empty lines
        char *name = strtok(lineStr, ";,");
        int gains[10];
        bool different = false;
        for(int i = 0; i < 10; i++){
            char *gain = strtok(NULL, ";,\n");
            gains[i] = atoi(gain);
            if(different == false && jkkRadio->eqPresets[index].gain[i] != gains[i]){
                different = true;
            }
        }
        
        if (name) {
            char key[16] = {0};
            sprintf(key, JKK_RADIO_NVS_EQUALIZER_KEY, index);
            if(strcmp(name, jkkRadio->eqPresets[index].name) || different) {
                // If the equalizer is different from the one in NVS, update it
                strncpy(jkkRadio->eqPresets[index].name, name, sizeof(jkkRadio->eqPresets[index].name) - 1);

                for(int i = 0; i < 10; i++){
                    jkkRadio->eqPresets[index].gain[i] = gains[i];
                }
                
                JkkNvsBlobSet(key, JKK_RADIO_NVS_EQ_NAMESPACE, &jkkRadio->eqPresets[index], sizeof(JkkRadioEqualizer_t)); // Save each updated eq to NVS
                ESP_LOGI(TAG, "Updated equalizers %d from file: name=%s",
                         index,
                         jkkRadio->eqPresets[index].name);
            }
            index++;
            if (index >= sdEqCount) {
                ESP_LOGI(TAG, "Reached end of equalizers list(%d), stopping reading from file", sdEqCount);
                break; // Stop reading if we reach the maximum count
            }
        }
    }
    if(index < nvsEqCount) {
        // If there are more stations in NVS than in the file, remove the extra ones
        for(int i = index; i < nvsEqCount; i++) {
            char key[16] = {0};
            sprintf(key, JKK_RADIO_NVS_EQUALIZER_KEY, i);
            ESP_LOGI(TAG, "Removing eq %d from NVS: %s", i, key);
            JkkNvsErase(key, JKK_RADIO_NVS_EQ_NAMESPACE);
        }
    }
    fclose(fptr);
    ESP_LOGI(TAG, "Loaded %d stations from /sdcard/eq.txt", index);

    ESP_LOGI(TAG, "Loaded equalizers:");
    for (int i = 0; i < index; i++) {
        ESP_LOGI(TAG, "Updated eq %d: name=%s",
                 i,
                 jkkRadio->eqPresets[i].name); 
    }
    jkkRadio->eq_count = index;

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    JkkLcdReloadRoller(jkkRadio);
#endif
    return ESP_OK;
}