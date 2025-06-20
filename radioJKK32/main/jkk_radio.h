/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
*/

#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#define JKK_RADIO_NVS_NAMESPACE "jkk_radio"

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

typedef enum  {
    JKK_RADIO_STATION_FAV = -2, // Favorite station
    JKK_RADIO_STATION_PREV = -1, // Previous station
    JKK_RADIO_STATION_FIRST = 0, // First station
    JKK_RADIO_STATION_NEXT = 1, // Next station
} changeStation_e;

#ifdef __cplusplus
}
#endif