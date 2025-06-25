/*  Part of RadioJKK32 - Multifunction Internet Radio Player - project
 * 
    1. RadioJKK32 is a multifunctional internet radio player designed to provide a seamless listening experience based on ESP-ADF.
    2. It supports various audio formats and streaming protocols, allowing users to access a wide range of radio stations.
    4. It includes advanced audio processing capabilities, such as equalization and resampling, to enhance the sound experience.
    5. The device is built on the ESP32-A1S audio dev board, leveraging its powerful processing capabilities and connectivity options.
    6. The project is open-source and licensed under the MIT License, allowing for free use, modification, and distribution.

    Based on the AI Thinker ESP32-A1S Audio Kit board definition.

 *  Copyright (C) 2025 Jaromir Kopp (JKK)
*/

#ifndef _AUDIO_BOARD_H_
#define _AUDIO_BOARD_H_

#include "audio_hal.h"
#include "board_def.h"
#include "board_pins_config.h"
#include "esp_peripherals.h"
#include "display_service.h"
#if defined(CONFIG_AI_THINKER_ESP32_A1S_AUDIO_KIT_USING_SDCARD)
#include "periph_sdcard.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define JKK_DISPLAY_PATTERN_RW_A (100)
#define JKK_DISPLAY_PATTERN_RW_3 (101)
#define JKK_DISPLAY_PATTERN_RWK  (110)
#define JKK_DISPLAY_PATTERN_RAM  (120)
#define JKK_DISPLAY_PATTERN_BR_PULSE  (200)

/**
 * @brief Audio board handle
 */
struct audio_board_handle {
    audio_hal_handle_t audio_hal; /*!< audio hardware abstract layer handle */
    audio_hal_handle_t adc_hal;   /*!< adc hardware abstract layer handle */
};

typedef struct audio_board_handle *audio_board_handle_t;

/**
 * @brief Initialize audio board
 *
 * @return The audio board handle
 */
audio_board_handle_t audio_board_init(void);

/**
 * @brief Initialize codec chip
 *
 * @return The audio hal handle
 */
audio_hal_handle_t audio_board_codec_init(void);

/**
 * @brief Initialize adc
 *
 * @return The adc hal handle
 */
#if defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_BUTTON_KEY_ADC)
audio_hal_handle_t audio_board_adc_init(void);
#endif

/**
 * @brief Initialize led peripheral and display service
 *
 * @return The audio display service handle
 */
display_service_handle_t audio_board_led_init(void);

/**
 * @brief Initialize key peripheral
 *
 * @param set The handle of esp_periph_set_handle_t
 *
 * @return
 *     - ESP_OK, success
 *     - Others, fail
 */
esp_err_t audio_board_key_init(esp_periph_set_handle_t set);

/**
 * @brief Initialize sdcard peripheral
 *
 * @param set The handle of esp_periph_set_handle_t
 *
 * @return
 *     - ESP_OK, success
 *     - Others, fail
 */
#if defined(CONFIG_AI_THINKER_ESP32_A1S_AUDIO_KIT_USING_SDCARD)
esp_err_t audio_board_sdcard_init(esp_periph_set_handle_t set, periph_sdcard_mode_t mode);
#endif

/**
 * @brief Query audio_board_handle
 *
 * @return The audio board handle
 */
audio_board_handle_t audio_board_get_handle(void);

/**
 * @brief Uninitialize the audio board
 *
 * @param audio_board The handle of audio board
 *
 * @return  0       success,
 *          others  fail
 */
esp_err_t audio_board_deinit(audio_board_handle_t audio_board);

int8_t get_red_led_gpio(void);
int8_t get_green_led_gpio(void);

#ifdef __cplusplus
}
#endif

#endif
