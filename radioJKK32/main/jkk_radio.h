/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
*/

#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_peripherals.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "board.h"
#include "audio_mem.h"
#include "audio_sys.h"
#include "jkk_audio_main.h"
#include "jkk_audio_sdwrite.h"

#define EPOCH_TIMESTAMP (978307200l)

#define JKK_RADIO_NVS_NAMESPACE "jkk_radio"
#define JKK_RADIO_NVS_EQ_NAMESPACE "jkk_radio_EQ"
#define JKK_RADIO_NVS_STATION_KEY "station%03X"
#define JKK_RADIO_NVS_EQUALIZER_KEY "equalizer%03X"

#define SD_RECORDS_PATH "/sdcard/rec"

#define JKK_RADIO_MAX_STATIONS (20) // Maximum number of radio stations
#define JKK_RADIO_MAX_EBMEDDED_STATIONS (4) // Maximum number of embedded radio stations

#define JKK_RADIO_MAX_EQ_PRESETS (10) // Maximum number of equalizers preset
#define JKK_RADIO_MAX_EBMEDDED_EQ_PRESETS (3) // Maximum number of embedded equalizers preset

typedef struct JkkRadioEqualizer_s {
    char name[16];
    int gain[10];
} JkkRadioEqualizer_t;

typedef struct JkkRadioStations_s {
    char uri[128]; // URI of the radio station
    char nameShort[32]; // Name of the radio station
    char nameLong[128]; // Description of the radio station
    char audioDes[16]; // Additional audio description of the station
    bool is_favorite; // Flag indicating if the station is marked as favorite
    enum {
        JKK_RADIO_UNKNOWN = 0, // Unknown type of station
        JKK_RADIO_MUSIC, // Music station
        JKK_RADIO_NEWS, // News station
        JKK_RADIO_TALK, // Talk station
        JKK_RADIO_SPORTS, // Sports station
        JKK_RADIO_REGIONAL, // Regional station
        JKK_RADIO_INTERNATIONAL, // International station
        JKK_RADIO_OTHER // Other type of station
    } type; // Type of the radio station
} JkkRadioStations_t;

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
    int eq_count;
    bool is_playing; // Flag indicating if the radio is currently playing
    bool is_ChangingStation; // Flag indicating if the radio is currently playing
    JkkRadioStations_t *jkkRadioStations; // Pointer to the array of radio stations
    JkkRadioEqualizer_t *eqPresets;
    char wifiSSID[32]; // WiFi SSID
    char wifiPassword[64]; // WiFi Password
} JkkRadio_t;

typedef enum  {
    JKK_RADIO_STATION_FAV = -2, // Favorite station
    JKK_RADIO_STATION_PREV = -1, // Previous station
    JKK_RADIO_STATION_FIRST = 0, // First station
    JKK_RADIO_STATION_NEXT = 1, // Next station
} changeStation_e;

typedef enum  {
    JKK_RADIO_CMD_SET_STATION = 100, 
    JKK_RADIO_CMD_SET_EQUALIZER = 101, 
    JKK_RADIO_CMD_SET_UNKNOW, 
} customCmd_e;

void JkkRadioSetStation(uint16_t station);

esp_err_t JkkRadioSendMessageToMain(int mess, int command);

#ifdef __cplusplus
}
#endif