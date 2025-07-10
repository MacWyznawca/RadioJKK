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
#define ESP_SD_PIN_D1             -1
#define ESP_SD_PIN_D2             -1
#define ESP_SD_PIN_D3             -1
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
#define BOARD_PA_GAIN             (5) /* Power amplifier gain defined by board (dB) */

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
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
#define BUTTON_REC_ID             -1  /* KEY6 GPIO_NUM_5 */
#define BUTTON_PLAY_ID            -1 /* KEY5 GPIO_NUM_18 */
#define BUTTON_VOLUP_ID           GPIO_NUM_23 /* KEY4  GPIO_NUM_23 SET_ID */ 
#define BUTTON_VOLDOWN_ID         GPIO_NUM_19 /* KEY3 *GPIO_NUM_19 REC_ID */
#define BUTTON_SET_ID             GPIO_NUM_13 /* KEY2 GPIO_NUM_13 REC_ID */
#if defined(CONFIG_JKK_RADIO_USING_EXT_KEYS)
#define BUTTON_MODE_ID            GPIO_NUM_22 /* KEY1 GPIO_NUM_12MODE_ID */ 
#else
#define BUTTON_MODE_ID            GPIO_NUM_36 /* KEY1 GPIO_NUM_36 MODE_ID */
#endif // JKK_RADIO_USING_EXT_KEYS

#define INPUT_KEY_DEFAULT_INFO() {                  \
    {                                               \
        .type = PERIPH_ID_BUTTON,                   \
        .user_id = INPUT_KEY_USER_ID_REC,           \
        .act_id = BUTTON_SET_ID,                    \
    },                                              \
    {                                               \
        .type = PERIPH_ID_BUTTON,                   \
        .user_id = INPUT_KEY_USER_ID_MODE,          \
        .act_id = BUTTON_MODE_ID,                   \
    },                                              \
    {                                               \
        .type = PERIPH_ID_BUTTON,                   \
        .user_id = INPUT_KEY_USER_ID_SET,           \
        .act_id = BUTTON_VOLUP_ID,                    \
    },                                              \
    {                                               \
        .type = PERIPH_ID_BUTTON,                   \
        .user_id = INPUT_KEY_USER_ID_PLAY,          \
        .act_id = BUTTON_VOLDOWN_ID,                   \
    }                                               \
}

#else // !CONFIG_JKK_RADIO_USING_I2C_LCD

#define BUTTON_VOLUP_ID           GPIO_NUM_5  /* KEY6 */
#define BUTTON_VOLDOWN_ID         GPIO_NUM_18 /* KEY5 */
#define BUTTON_SET_ID             GPIO_NUM_23 /* KEY4  */ 
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
#endif // CONFIG_JKK_RADIO_USING_I2C_LCD
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
