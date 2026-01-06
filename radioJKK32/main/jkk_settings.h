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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read WiFi settings from SD card or NVS
 * @param jkkRadio Pointer to main radio structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkRadioSettingsRead(JkkRadio_t *jkkRadio);

/**
 * @brief Read radio station list from SD card and NVS
 * @param jkkRadio Pointer to main radio structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkRadioStationSdRead(JkkRadio_t *jkkRadio);

/**
 * @brief Read equalizer presets from SD card and NVS
 * @param jkkRadio Pointer to main radio structure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkRadioEqSdRead(JkkRadio_t *jkkRadio);

/**
 * @brief Write WiFi settings to SD card settings.txt (preserving 3rd field if present)
 */
esp_err_t JkkRadioSettingsWriteWifi(const char *ssid, const char *pass, bool runWebServer);

#ifdef __cplusplus
}
#endif