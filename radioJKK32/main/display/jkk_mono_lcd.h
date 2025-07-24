/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * Monochrome LCD display interface functions
*/

#pragma once

#include <string.h>
#include "../jkk_radio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JKK_ROLLER_TIME_TO_HIDE (10)

typedef enum {
    JKK_ROLLER_MODE_HIDE = 0,
    JKK_ROLLER_MODE_STATION_LIST,
    JKK_ROLLER_MODE_EQUALIZER_LIST,
    JKK_ROLLER_MODE_UNKNOWN, 
} jkkRollerMode_t;

/**
 * @brief Initialize LCD user interface
 * @param radio Pointer to main radio structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkLcdUiInit(JkkRadio_t *radio);

/**
 * @brief Display station name on LCD
 * @param stationName Station name text to display
 */
void JkkLcdStationTxt(char *stationName);

/**
 * @brief Display volume level on LCD
 * @param vol Volume level (0-100)
 */
void JkkLcdVolumeInt(int vol);

/**
 * @brief Display recording status indicator
 * @param rec true to show recording, false to hide
 */
void JkkLcdRec(bool rec);

/**
 * @brief Display equalizer name on LCD
 * @param eqName Equalizer preset name
 */
void JkkLcdEqTxt(char *eqName);

/**
 * @brief Display time on LCD
 * @param timeName Time string to display
 */
void JkkLcdTimeTxt(char *timeName);

/**
 * @brief Display IP address on LCD
 * @param ipTxt IP address string to display
 */
void JkkLcdIpTxt(char *ipTxt);

/**
 * @brief Send button press event to LCD interface
 * @param keyCode LVGL key code (LV_KEY_*)
 * @param pressed 1 for pressed, 0 for released
 */
void JkkLcdButtonSet(int keyCode, int8_t pressed);

/**
 * @brief Show or hide roller selection interface
 * @param show true to show, false to hide
 * @param idx Selected item index
 * @param mode Roller mode (station list or equalizer list)
 */
void JkkLcdShowRoller(bool show, uint8_t idx, jkkRollerMode_t mode);

/**
 * @brief Set options for roller selection interface
 * @param options Newline-separated list of options
 * @param idx Currently selected index
 */
void JkkLcdSetRollerOptions(char *options, uint8_t idx);

/**
 * @brief Get current roller mode
 * @return Current roller mode
 */
jkkRollerMode_t JkkLcdRollerMode(void);

/**
 * @brief Volume meter callback for displaying audio levels
 * @param left_volume Left channel volume level (0-100)
 * @param right_volume Right channel volume level (0-100)
 */
void JkkLcdVolumeIndicatorCallback(int left_volume, int right_volume);

/**
 * @brief Set CPU load display mode
 * @param mode CPU load display mode
 */
void JkkLcdSetCpuLoadMode(int8_t mode);

/**
 * @brief Display QR code on LCD
 * @param url URL to encode in QR code (NULL to hide QR code)
 */
void JkkLcdQRcode(const char *url);

#ifdef __cplusplus
}
#endif