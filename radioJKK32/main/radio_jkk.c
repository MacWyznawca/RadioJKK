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

#include "jkk_radio.h"

#include "audio_idf_version.h"

#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "periph_button.h"

#include "esp_random.h"
#include <esp_timer.h>

#include "esp_netif.h"

#include "RawSplit/raw_split.h"
#include "jkk_audio_main.h"
#include "jkk_audio_sdwrite.h"

#include "jkk_nvs.h"
#include "nvs.h"
#include "jkk_settings.h"

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
#include "display/jkk_mono_lcd.h"
#endif

#define JKK_RADIO_PREDEV_EQ (4) // Maximum number of radio stations

static const char *TAG = "RADIO";

static const EXT_RAM_BSS_ATTR int eq_Gains[JKK_RADIO_PREDEV_EQ][10] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 4, 3, 1, 0, -1, 0, 1, 3, 6},
    {0, 6, 6, 4, 0, -1, -1, 0, 6, 10},
    {-6, -6, -2, 1, 2, 3, 2, 1, -2, -6}
};


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
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    if(jkkRadio.current_eq == 0) {
        JkkLcdEqTxt("---");
    } else if(jkkRadio.current_eq == 1) {
        JkkLcdEqTxt("-_-");
    } else if(jkkRadio.current_eq == 2) {
        JkkLcdEqTxt("\\_/");
    } else if(jkkRadio.current_eq == 3) {
        JkkLcdEqTxt("_-_");
    }
#else
    display_service_set_pattern(jkkRadio.disp_serv, JKK_DISPLAY_PATTERN_BR_PULSE + jkkRadio.current_eq + 1, 1);
#endif
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

    if(nextStation == jkkRadio.current_station) {
        ESP_LOGI(TAG, "No change in station, current station: %d", jkkRadio.current_station);
        return;
    }

    audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, false);

    jkkRadio.current_station = nextStation;
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);

    ESP_LOGI(TAG, "Station change - Name: %s, Url: %s", jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong, jkkRadio.jkkRadioStations[jkkRadio.current_station].uri);
    JkkAudioSetUrl(jkkRadio.jkkRadioStations[jkkRadio.current_station].uri, false);
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    JkkLcdStationTxt(jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong);
#else
    display_service_set_pattern(jkkRadio.disp_serv, JKK_DISPLAY_PATTERN_BR_PULSE + jkkRadio.current_station + 1, 1);
#endif
    audio_pipeline_reset_ringbuffer(pipeline);
    audio_pipeline_reset_elements(pipeline);
    if(jkkRadio.audioSdWrite->is_recording){
        JkkAudioSdWriteStopStream();
        jkkRadio.audioSdWrite->is_recording = true;
    }
    audio_pipeline_run(pipeline);
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
    jkkRadio.player_volume = 10; // Set initial volume

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    ESP_LOGI(TAG, "Initialize I2C LCD display");
    JkkLcdInit();
#endif

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    JkkLcdVolumeInt(jkkRadio.player_volume);
#endif

    jkkRadio.audioMain = JkkAudioMain_init(3, 1, 1, 1); // in/out type: 3 - HTTP, 1 - I2S; processing type: 1 - EQUALIZER, 1 - RAW_SPLIT; split nr

    jkkRadio.audioSdWrite = JkkAudioSdWrite_init(1, 22050, 2); // 1 - AAC, sample_rate, channels

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    jkkRadio.set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "Start and wait for SDCARD to mount");
    audio_board_sdcard_init(jkkRadio.set, SD_MODE_1_LINE);

    JkkRadioSettingsRead(&jkkRadio);
    JkkRadioStationSdRead(&jkkRadio);

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
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
    JkkLcdStationTxt(jkkRadio.jkkRadioStations[jkkRadio.current_station].nameLong);
#else
    display_service_set_pattern(jkkRadio.disp_serv, JKK_DISPLAY_PATTERN_BR_PULSE + 1, 1);
#endif
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

    JkkChangeEq(0); // Set initial EQ

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
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
                JkkLcdRec(true);
#endif     
            }
            else {
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
                JkkLcdRec(false);
#endif
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
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD) 
            JkkLcdRec(false);
#endif
            JkkAudioSdWriteStopStream();
            continue;
        }

        if (msg.source_type == PERIPH_ID_SDCARD && msg.cmd == AEL_MSG_CMD_FINISH) {  
            JkkRadioSettingsRead(&jkkRadio);
            JkkRadioStationSdRead(&jkkRadio);
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

            audio_hal_enable_pa(jkkRadio.board_handle->audio_hal, true);

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
        if (msg.source_type == PERIPH_ID_BUTTON && (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_PRESSED)) {
            if ((int)msg.data == get_input_mode_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    JkkChangeEq(1);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    audio_element_state_t sdState = audio_element_get_state(jkkRadio.audioSdWrite->fatfs_wr);
                    jkkRadio.audioSdWrite->is_recording = (sdState == AEL_STATE_RUNNING);
                    if(jkkRadio.audioSdWrite->is_recording){
                        display_service_set_pattern(jkkRadio.disp_serv, DISPLAY_PATTERN_RECORDING_STOP, 1);
                        JkkLcdRec(false);
                        JkkAudioSdWriteStopStream();
                    }
                    else{
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
                                JkkLcdRec(true);
                            }
                        }
                    }
                }
            } else if ((int)msg.data == get_input_rec_id()) {
                ;
            } else if ((int)msg.data == get_input_set_id()) {
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    ESP_LOGI(TAG, "[Vol+] touch tap event");
                    jkkRadio.player_volume += jkkRadio.player_volume < 40 ? 5 : 10;
                    if (jkkRadio.player_volume > 100) {
                        jkkRadio.player_volume = 100;
                    }
                    audio_hal_set_volume(jkkRadio.board_handle->audio_hal, jkkRadio.player_volume);                
                    JkkLcdVolumeInt(jkkRadio.player_volume);
                    ESP_LOGI(TAG, "Volume jkkRadio.set to %d%%", jkkRadio.player_volume);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_NEXT);
                }

              //  cli_get_mp3_id3_info(audio_decoder);
            } else if ((int)msg.data == get_input_play_id()) {
                ESP_LOGI(TAG, "[Vol-] touch tap event");
                if(msg.cmd == PERIPH_BUTTON_RELEASE){
                    jkkRadio.player_volume -= jkkRadio.player_volume < 40 ? 5 : 10;
                    if (jkkRadio.player_volume < 0) {
                        jkkRadio.player_volume = 0;
                    }
                    audio_hal_set_volume(jkkRadio.board_handle->audio_hal, jkkRadio.player_volume);
                    JkkLcdVolumeInt(jkkRadio.player_volume);
                    ESP_LOGI(TAG, "Volume jkkRadio.set to %d%%", jkkRadio.player_volume);
                }
                else if(msg.cmd == PERIPH_BUTTON_LONG_PRESSED){
                    JkkChangeStation(jkkRadio.audioMain->pipeline, JKK_RADIO_STATION_PREV);
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
