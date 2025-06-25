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
#include "driver/gpio.h"
#include <string.h>
#include "board.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "soc/io_mux_reg.h"
#include "soc/soc_caps.h"

static const char *TAG = "AI_THINKER_ESP32_A1S";

esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config)
{
    AUDIO_NULL_CHECK(TAG, i2c_config, return ESP_FAIL);
    if (port == I2C_NUM_0 || port == I2C_NUM_1) {
#if defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_VARIANT_7)
        i2c_config->sda_io_num = GPIO_NUM_18;
        i2c_config->scl_io_num = GPIO_NUM_23;
#elif defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_VARIANT_5)
        i2c_config->sda_io_num = GPIO_NUM_33;
        i2c_config->scl_io_num = GPIO_NUM_32;
#endif
    } else {
        i2c_config->sda_io_num = -1;
        i2c_config->scl_io_num = -1;
        ESP_LOGE(TAG, "i2c port %d is not supported", port);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_i2s_pins(int port, board_i2s_pin_t *i2s_config){
    AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);
    if (port == 0) {
        i2s_config->mck_io_num = GPIO_NUM_0;
#if defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_VARIANT_7)
        i2s_config->bck_io_num = GPIO_NUM_5;
#elif defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_VARIANT_5)
        i2s_config->bck_io_num = GPIO_NUM_27;
#endif
        i2s_config->ws_io_num = GPIO_NUM_25;
        i2s_config->data_out_num = GPIO_NUM_26;
        i2s_config->data_in_num = GPIO_NUM_35;
    } else if (port == 1) {
        i2s_config->bck_io_num = -1;
        i2s_config->ws_io_num = -1;
        i2s_config->data_out_num = -1;
        i2s_config->data_in_num = -1;
    } else {
        memset(i2s_config, -1, sizeof(board_i2s_pin_t));
        ESP_LOGE(TAG, "i2s port %d is not supported", port);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t get_spi_pins(spi_bus_config_t *spi_config, spi_device_interface_config_t *spi_device_interface_config){
    AUDIO_NULL_CHECK(TAG, spi_config, return ESP_FAIL);
    AUDIO_NULL_CHECK(TAG, spi_device_interface_config, return ESP_FAIL);

    spi_config->mosi_io_num = -1;
    spi_config->miso_io_num = -1;
    spi_config->sclk_io_num = -1;
    spi_config->quadwp_io_num = -1;
    spi_config->quadhd_io_num = -1;

    spi_device_interface_config->spics_io_num = -1;

    ESP_LOGW(TAG, "SPI interface is not supported");
    return ESP_OK;
}

// sdcard detect gpio
#if defined(CONFIG_AI_THINKER_ESP32_A1S_AUDIO_KIT_USING_SDCARD)
int8_t get_sdcard_intr_gpio(void)
{
    return SDCARD_INTR_GPIO;
}

int8_t get_sdcard_open_file_num_max(void)
{
    return SDCARD_OPEN_FILE_NUM_MAX;
}
#endif

// volume up button
int8_t get_input_volup_id(void)
{
    return BUTTON_VOLUP_ID;
}

// volume down button
int8_t get_input_voldown_id(void)
{
    return BUTTON_VOLDOWN_ID;
}

// pa enable
int8_t get_pa_enable_gpio(void)
{
    return PA_ENABLE_GPIO;
}

// mode button
int8_t get_input_mode_id(void)
{
    return BUTTON_MODE_ID;
}

// set button
int8_t get_input_set_id(void)
{
    return BUTTON_SET_ID;
}

// play button
int8_t get_input_play_id(void)
{
    return BUTTON_PLAY_ID;
}

// rec button, or key 2
int8_t get_input_rec_id(void)
{
    return BUTTON_REC_ID;
}

int8_t get_green_led_gpio(void)
{
    return RED_LED_GPIO;
}


int8_t get_headphone_detect_gpio(void)
{
    return HEADPHONE_DETECT;
}
