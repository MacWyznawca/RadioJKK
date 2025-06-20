/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 * Copyright (c) 2023 Tomoyuki Sakurai <y@trombik.org>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _AI_THINKER_ESP32_A1S_H_
#define _AI_THINKER_ESP32_A1S_H_

/**
 * @brief SDCARD Function Definition
 */
#if defined(CONFIG_AI_THINKER_ESP32_A1S_AUDIO_KIT_USING_SDCARD)
#define FUNC_SDCARD_EN            (1)
#define SDCARD_OPEN_FILE_NUM_MAX  5
#define SDCARD_INTR_GPIO          GPIO_NUM_34
#define ESP_SD_PIN_CLK            GPIO_NUM_14
#define ESP_SD_PIN_CMD            GPIO_NUM_15
#define ESP_SD_PIN_D0             GPIO_NUM_2
#define ESP_SD_PIN_D1             GPIO_NUM_4
#define ESP_SD_PIN_D2             GPIO_NUM_12
#define ESP_SD_PIN_D3             GPIO_NUM_13
#else
#define FUNC_SDCARD_EN            (0)
#define SDCARD_OPEN_FILE_NUM_MAX  5
#define SDCARD_INTR_GPIO          -1
#define ESP_SD_PIN_CLK            -1
#define ESP_SD_PIN_CMD            -1
#define ESP_SD_PIN_D0             -1
#define ESP_SD_PIN_D1             -1
#define ESP_SD_PIN_D2             -1
#define ESP_SD_PIN_D3             -1
#endif

/**
 * @brief LED Function Definition
 */
#define FUNC_SYS_LEN_EN           (1)
#define GREEN_LED_GPIO              -1 // GPIO_NUM_19
#define BLUE_LED_GPIO               -1
#define RED_LED_GPIO                GPIO_NUM_22 // TCA9554_GPIO_NUM_19


/**
 * @brief Audio Codec Chip Function Definition
 */
#define FUNC_AUDIO_CODEC_EN       (1)
#define AUXIN_DETECT_GPIO         (-1)
#define HEADPHONE_DETECT          (-1)
#define PA_ENABLE_GPIO            GPIO_NUM_21
#define CODEC_ADC_I2S_PORT        (0)
#define CODEC_ADC_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define CODEC_ADC_SAMPLE_RATE     (48000)
#define RECORD_HARDWARE_AEC       (false)
#define BOARD_PA_GAIN             (10) /* Power amplifier gain defined by board (dB) */

extern audio_hal_func_t AUDIO_CODEC_ES8388_DEFAULT_HANDLE;
#define AUDIO_CODEC_DEFAULT_CONFIG(){                   \
        .adc_input  = AUDIO_HAL_ADC_INPUT_LINE1,        \
        .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,         \
        .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,        \
        .i2s_iface = {                                  \
            .mode = AUDIO_HAL_MODE_SLAVE,               \
            .fmt = AUDIO_HAL_I2S_NORMAL,                \
            .samples = AUDIO_HAL_48K_SAMPLES,           \
            .bits = AUDIO_HAL_BIT_LENGTH_16BITS,        \
        },                                              \
};

/**
 * @brief Button Function Definition
 */
#define FUNC_BUTTON_EN            (1)
#define INPUT_KEY_NUM             6

#if defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_BUTTON_KEY_GPIO)
#define BUTTON_VOLUP_ID           GPIO_NUM_5  /* KEY6 */
#define BUTTON_VOLDOWN_ID         GPIO_NUM_18 /* KEY5 */
#define BUTTON_SET_ID             GPIO_NUM_23 /* KEY4 */
#define BUTTON_PLAY_ID            GPIO_NUM_19 /* KEY3 */
#define BUTTON_REC_ID             GPIO_NUM_13 /* KEY2 */
#define BUTTON_MODE_ID            GPIO_NUM_36 /* KEY1 GPIO_NUM_36 */

#define INPUT_KEY_DEFAULT_INFO() {                  \
    {                                               \
        .type = PERIPH_ID_BUTTON,                   \
        .user_id = INPUT_KEY_USER_ID_REC,           \
        .act_id = BUTTON_REC_ID,                    \
    },                                              \
    {                                               \
        .type = PERIPH_ID_BUTTON,                   \
        .user_id = INPUT_KEY_USER_ID_MODE,          \
        .act_id = BUTTON_MODE_ID,                   \
    },                                              \
    {                                               \
        .type = PERIPH_ID_BUTTON,                   \
        .user_id = INPUT_KEY_USER_ID_SET,           \
        .act_id = BUTTON_SET_ID,                    \
    },                                              \
    {                                               \
        .type = PERIPH_ID_BUTTON,                   \
        .user_id = INPUT_KEY_USER_ID_PLAY,          \
        .act_id = BUTTON_PLAY_ID,                   \
    },                                              \
    {                                               \
        .type = PERIPH_ID_BUTTON,                   \
        .user_id = INPUT_KEY_USER_ID_VOLUP,         \
        .act_id = BUTTON_VOLUP_ID,                  \
    },                                              \
    {                                               \
        .type = PERIPH_ID_BUTTON,                   \
        .user_id = INPUT_KEY_USER_ID_VOLDOWN,       \
        .act_id = BUTTON_VOLDOWN_ID,                \
    }                                               \
}
#elif defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_BUTTON_KEY_ADC)
#define ADC_DETECT_GPIO           GPIO_NUM_36
#define BUTTON_VOLUP_ID           5 /* KEY6 */
#define BUTTON_VOLDOWN_ID         4 /* KEY5 */
#define BUTTON_SET_ID             3 /* KEY4 */
#define BUTTON_PLAY_ID            2 /* KEY3 */
#define BUTTON_REC_ID             1 /* KEY2 */
#define BUTTON_MODE_ID            0 /* KEY1 */
#define INPUT_KEY_DEFAULT_INFO() {                  \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_REC,           \
        .act_id = BUTTON_REC_ID,                    \
    },                                              \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_MODE,          \
        .act_id = BUTTON_MODE_ID,                   \
    },                                              \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_SET,           \
        .act_id = BUTTON_SET_ID,                    \
    },                                              \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_PLAY,          \
        .act_id = BUTTON_PLAY_ID,                   \
    },                                              \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_VOLUP,         \
        .act_id = BUTTON_VOLUP_ID,                  \
    },                                              \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_VOLDOWN,       \
        .act_id = BUTTON_VOLDOWN_ID,                \
    }                                               \
}
#endif

#endif
