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

#include "esp_log.h"
#include "board.h"
#include "audio_mem.h"

#include "periph_sdcard.h"
#include "periph_led.h"
#include "led_indicator.h"
#include "periph_button.h"
#include "driver/gpio.h"

#if defined(CONFIG_AI_THINKER_ESP32_A1S_AUDIO_KIT_USING_SDCARD)
#include "periph_sdcard.h"
#endif
#include "led_indicator.h"
#if defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_BUTTON_KEY_ADC)
#include "periph_adc_button.h"
#elif defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_BUTTON_KEY_GPIO)
#include "periph_button.h"
#endif

static const char *TAG = "AUDIO_BOARD";

static audio_board_handle_t board_handle = 0;

typedef struct led_indicator_impl {
    gpio_num_t              gpio_num;
    esp_periph_handle_t     periph_handle;
} led_indicator_impl_t;

audio_board_handle_t audio_board_init(void){
    if (board_handle) {
        ESP_LOGW(TAG, "The board has already been initialized!");
        return board_handle;
    }
    board_handle = (audio_board_handle_t) audio_calloc(1, sizeof(struct audio_board_handle));
    AUDIO_MEM_CHECK(TAG, board_handle, return NULL);
    board_handle->audio_hal = audio_board_codec_init();

    return board_handle;
}

audio_hal_handle_t audio_board_codec_init(void){
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_hal_handle_t codec_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES8388_DEFAULT_HANDLE);
    AUDIO_NULL_CHECK(TAG, codec_hal, return NULL);
    return codec_hal;
}

static esp_err_t as1_led_indicator_pattern(void *handle, int pat, int value){
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    led_indicator_handle_t h = (led_indicator_handle_t)handle;
    
    ESP_LOGD(TAG, "pat:%d, gpio:%d", pat, h->gpio_num);
    switch (pat) {
        case DISPLAY_PATTERN_WIFI_SETTING:
            periph_led_blink(h->periph_handle, h->gpio_num, 500, 500, false, -1, value);
            break;
        case DISPLAY_PATTERN_WIFI_CONNECTED:
            periph_led_blink(h->periph_handle, h->gpio_num, 1000, 200, true, 5, value);
            break;
        case DISPLAY_PATTERN_WIFI_DISCONNECTED:
            periph_led_blink(h->periph_handle, h->gpio_num, 200, 500, false, -1, value);
            break;
        case DISPLAY_PATTERN_TURN_ON:
        case DISPLAY_PATTERN_WAKEUP_ON:
            periph_led_blink(h->periph_handle, h->gpio_num, 100, 0, false, -1, value);
            break;
        case DISPLAY_PATTERN_TURN_OFF:
        case DISPLAY_PATTERN_WAKEUP_FINISHED:
        case DISPLAY_PATTERN_SPEECH_OVER:
            periph_led_blink(h->periph_handle, h->gpio_num, 0, 100, false, -1, value);
            break;
        case DISPLAY_PATTERN_RECORDING_START:
            periph_led_blink(h->periph_handle, h->gpio_num, 25, 100, false, 4, value);
            break;
        case DISPLAY_PATTERN_RECORDING_STOP:
            periph_led_blink(h->periph_handle, h->gpio_num, 25, 50, false, 6, value);
            break;
        case JKK_DISPLAY_PATTERN_RW_A:
            periph_led_blink(h->periph_handle, h->gpio_num, 200, 800, true, -1, value);
            break;
        case JKK_DISPLAY_PATTERN_RW_3:
            periph_led_blink(h->periph_handle, h->gpio_num, 300, 700, true, -1, value);
            break;
        case JKK_DISPLAY_PATTERN_RWK:
            periph_led_blink(h->periph_handle, h->gpio_num, 1900, 100, true, -1, value);
            break;
        case JKK_DISPLAY_PATTERN_RAM:
            periph_led_blink(h->periph_handle, h->gpio_num, 1500, 500, true, -1, value);
            break;
        default: {
            if(pat > JKK_DISPLAY_PATTERN_BR_PULSE && pat < JKK_DISPLAY_PATTERN_BR_PULSE + 10) {
                uint8_t nrP = pat - JKK_DISPLAY_PATTERN_BR_PULSE;
                if (nrP > 10) {
                    ESP_LOGW(TAG, "Blinks nr out of range: %d", nrP);
                    return ESP_ERR_INVALID_ARG;
                }
                ESP_LOGW(TAG, "Blinks nr: %d", nrP);
                periph_led_blink(h->periph_handle, h->gpio_num, 100, 100, true, nrP << 1, PERIPH_LED_IDLE_LEVEL_HIGH);
            }
            break;
        }
    }
    return ESP_OK;
}

display_service_handle_t audio_board_led_init(void){
    gpio_reset_pin((gpio_num_t)get_green_led_gpio());
    led_indicator_handle_t led = led_indicator_init((gpio_num_t)get_green_led_gpio());
    display_service_config_t display = {
        .based_cfg = {
            .task_stack = 0,
            .task_prio  = 0,
            .task_core  = 0,
            .task_func  = NULL,
            .service_start = NULL,
            .service_stop = NULL,
            .service_destroy = NULL,
            .service_ioctl = as1_led_indicator_pattern,
            .service_name = "DISP_serv",
            .user_data = NULL,
        },
        .instance = led,
    };
#if defined(CONFIG_JKK_RADIO_USING_EXT_KEYS) 
    return NULL;
#else
    return display_service_create(&display);
#endif
}

esp_err_t audio_board_key_init(esp_periph_set_handle_t set)
{
    periph_button_cfg_t btn_cfg = {
#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
        .gpio_mask = (1ULL << get_input_mode_id())     | \
                     (1ULL << get_input_volup_id())      | \
                     (1ULL << get_input_set_id())      | \
                     (1ULL << get_input_voldown_id()),
#else
        .gpio_mask = (1ULL << get_input_volup_id())     | \
                     (1ULL << get_input_voldown_id())   | \
                     (1ULL << get_input_mode_id())      | \
                     (1ULL << get_input_set_id())       | \
                     (1ULL << get_input_rec_id())       | \
                     (1ULL << get_input_play_id()),
#endif
        .long_press_time_ms = 700
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);
    AUDIO_NULL_CHECK(TAG, button_handle, return ESP_ERR_ADF_MEMORY_LACK);
    esp_err_t ret = ESP_OK;
    ret = esp_periph_start(set, button_handle);
    return ret;
}

esp_err_t audio_board_sdcard_init(esp_periph_set_handle_t set, periph_sdcard_mode_t mode)
{
    if (mode > SD_MODE_4_LINE) {
        ESP_LOGE(TAG, "Please select the correct sd mode!, current mode is %d", mode);
        return ESP_FAIL;
    }
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = get_sdcard_intr_gpio(),
        .mode = mode,
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    esp_err_t ret = esp_periph_start(set, sdcard_handle);
    int retry_time = 5;
    bool mount_flag = false;
    while (retry_time --) {
        if (periph_sdcard_is_mounted(sdcard_handle)) {
            mount_flag = true;
            break;
        } else {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
    if (mount_flag == false) {
        ESP_LOGE(TAG, "Sdcard mount failed");
        return ESP_FAIL;
    }
    return ret;
}

audio_board_handle_t audio_board_get_handle(void)
{
    return board_handle;
}

esp_err_t audio_board_deinit(audio_board_handle_t audio_board)
{
    esp_err_t ret = ESP_OK;
    ret = audio_hal_deinit(audio_board->audio_hal);

    audio_free(audio_board);
    board_handle = NULL;
    return ret;
}


