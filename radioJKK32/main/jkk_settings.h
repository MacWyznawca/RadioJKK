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

esp_err_t JkkRadioSettingsRead(JkkRadio_t *jkkRadio);
esp_err_t JkkRadioStationSdRead(JkkRadio_t *jkkRadio);

#ifdef __cplusplus
}
#endif