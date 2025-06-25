/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * 
 * LCD I2C OLED 
 * 
 * Based on Espressif Systems (Shanghai) CO LTD example code
 *
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "driver/i2c_master.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#if CONFIG_JKK_RADIO_LCD_CONTROLLER_SH1107
#include "esp_lcd_sh1107.h"
#else
#include "esp_lcd_panel_vendor.h"
#endif

static const char *TAG = "LCD";

#define I2C_BUS_PORT I2C_NUM_1

#define JKK_RADIO_LCD_PIXEL_CLOCK_HZ    (1000 * 1000)
#define JKK_RADIO_PIN_NUM_SDA           18
#define JKK_RADIO_PIN_NUM_SCL           5
#define JKK_RADIO_PIN_NUM_RST           -1
#define JKK_RADIO_I2C_HW_ADDR           0x3C

// The pixel number in horizontal and vertical
#if CONFIG_JKK_RADIO_LCD_CONTROLLER_SSD1306
#define JKK_RADIO_LCD_H_RES              128
#define JKK_RADIO_LCD_V_RES              CONFIG_JKK_RADIO_SSD1306_HEIGHT
#elif CONFIG_JKK_RADIO_LCD_CONTROLLER_SH1107
#define JKK_RADIO_LCD_H_RES              64
#define JKK_RADIO_LCD_V_RES              128
#endif
// Bit number used to represent command and parameter
#define JKK_RADIO_LCD_CMD_BITS           8
#define JKK_RADIO_LCD_PARAM_BITS         8

lv_disp_t *disp = NULL;
lv_obj_t *radioLabel = NULL;
lv_obj_t *volLabel = NULL;
lv_obj_t *recLabel = NULL;
lv_obj_t *eqLabel = NULL;


static void Utf8ToAsciiPL(const char *input, char *output) {
    const unsigned char *src = (const unsigned char *)input;
    char *dst = output;

    while (*src) {
        unsigned char c1 = src[0];
        unsigned char c2 = src[1];

        // POLSKIE I ŁACIŃSKIE ZNAKI DIACENTRYCZNE
        if (c1 == 0xC4) {
            switch (c2) {
                case 0x84: *dst++ = 'A'; break; // Ą
                case 0x86: *dst++ = 'C'; break; // Ć
                case 0x98: *dst++ = 'E'; break; // Ę
                case 0x81: *dst++ = 'L'; break; // Ł
                case 0x83: *dst++ = 'N'; break; // Ń
                case 0x9A: *dst++ = 'S'; break; // Ś
                case 0xB9: *dst++ = 'Z'; break; // Ź
                case 0xBB: *dst++ = 'Z'; break; // Ż
                case 0x85: *dst++ = 'a'; break; // ą
                case 0x87: *dst++ = 'c'; break; // ć
                case 0x99: *dst++ = 'e'; break; // ę
                case 0x82: *dst++ = 'l'; break; // ł
                case 0x9B: *dst++ = 's'; break; // ś
                case 0xBA: *dst++ = 'z'; break; // ź
                case 0xBC: *dst++ = 'z'; break; // ż
                default: src += 2; continue;
            }
            src += 2;
        } else if (c1 == 0xC5) {
            switch (c2) {
                case 0x81: *dst++ = 'L'; break; // Ł
                case 0x82: *dst++ = 'l'; break; // ł
                case 0x83: *dst++ = 'N'; break; // Ń
                case 0x84: *dst++ = 'n'; break; // ń
                case 0x9A: *dst++ = 'S'; break; // Ś
                case 0x9B: *dst++ = 's'; break; // ś
                case 0xB9: *dst++ = 'Z'; break; // Ź
                case 0xBA: *dst++ = 'z'; break; // ź
                case 0xBB: *dst++ = 'Z'; break; // Ż
                case 0xBC: *dst++ = 'z'; break; // ż
                default: src += 2; continue;
            }
            src += 2;
        } else if (c1 == 0xC3) {
            switch (c2) {
                case 0x93: *dst++ = 'O'; break; // Ó
                case 0xB3: *dst++ = 'o'; break; // ó
                default: src += 2; continue;
            }
            src += 2;
        } else {
            *dst++ = *src++; // ASCII znak
        }
    }

    *dst = '\0';
}

void JkkLcdStationTxt(char *stationName) {
    if(disp == NULL || radioLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    char stationNameTmp[128];
    Utf8ToAsciiPL(stationName, stationNameTmp);
    if (lvgl_port_lock(0)) {
        lv_label_set_text(radioLabel, stationNameTmp);
        lvgl_port_unlock();
    }
}

void JkkLcdRec(bool rec) {
    static bool recState = false;
    if(disp == NULL || recLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    if(recState == rec) {
        ESP_LOGD(TAG, "Recording state unchanged: %s", rec ? "true" : "false");
        return;
    }
    if (lvgl_port_lock(0)) {
        if(rec) lv_label_set_text(recLabel, "R");
        else lv_label_set_text(recLabel, "");
        lvgl_port_unlock();
        recState = rec;
    }
}

void JkkLcdVolumeInt(int vol) {
    if(disp == NULL || volLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    char volTxt[10];
    if(vol == 0){
        snprintf(volTxt, sizeof(volTxt), "Vol: MUTE");
    }
    else{
        snprintf(volTxt, sizeof(volTxt), "Vol: %d%%", vol);
    }
    if (lvgl_port_lock(0)) {
        lv_label_set_text(volLabel, volTxt);
        lvgl_port_unlock();
    }
}

void JkkLcdEqTxt(char *eqName) {
    if(disp == NULL || eqLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    if (lvgl_port_lock(0)) {
        lv_label_set_text(eqLabel, eqName);
        lvgl_port_unlock();
    }
}

void JkkLcdInit(void){
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = JKK_RADIO_PIN_NUM_SDA,
        .scl_io_num = JKK_RADIO_PIN_NUM_SCL,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = JKK_RADIO_I2C_HW_ADDR,
        .scl_speed_hz = JKK_RADIO_LCD_PIXEL_CLOCK_HZ,
        .control_phase_bytes = 1,               // According to SSD1306 datasheet
        .lcd_cmd_bits = JKK_RADIO_LCD_CMD_BITS,   // According to SSD1306 datasheet
        .lcd_param_bits = JKK_RADIO_LCD_CMD_BITS, // According to SSD1306 datasheet
#if CONFIG_JKK_RADIO_LCD_CONTROLLER_SSD1306
        .dc_bit_offset = 6,                     // According to SSD1306 datasheet
#elif CONFIG_JKK_RADIO_LCD_CONTROLLER_SH1107
        .dc_bit_offset = 0,                     // According to SH1107 datasheet
        .flags =
        {
            .disable_control_phase = 1,
        }
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = JKK_RADIO_PIN_NUM_RST,
    };
#if CONFIG_JKK_RADIO_LCD_CONTROLLER_SSD1306
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = JKK_RADIO_LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
#elif CONFIG_JKK_RADIO_LCD_CONTROLLER_SH1107
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh1107(io_handle, &panel_config, &panel_handle));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

#if CONFIG_JKK_RADIO_LCD_CONTROLLER_SH1107
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#endif

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = JKK_RADIO_LCD_H_RES * JKK_RADIO_LCD_V_RES,
        .double_buffer = true,
        .hres = JKK_RADIO_LCD_H_RES,
        .vres = JKK_RADIO_LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        }
    };
    disp = lvgl_port_add_disp(&disp_cfg);

    lv_disp_set_rotation(disp, LV_DISP_ROT_180);

    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_disp_get_scr_act(disp);
        radioLabel = lv_label_create(scr);
        lv_label_set_long_mode(radioLabel, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
        lv_obj_set_style_text_align(radioLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(radioLabel, "Radio JKK32");
        lv_obj_set_width(radioLabel, disp->driver->hor_res);
        lv_obj_align(radioLabel, LV_ALIGN_TOP_MID, 0, 0);

        volLabel = lv_label_create(scr);
        lv_obj_set_style_text_align(volLabel, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_text(volLabel, "");
        lv_obj_set_width(volLabel, 64);
        lv_obj_align(volLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lvgl_port_unlock();

        recLabel = lv_label_create(scr);
        lv_obj_set_style_text_align(recLabel, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(recLabel, "");
        lv_obj_set_width(recLabel, 16);
        lv_obj_align(recLabel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lvgl_port_unlock();

        eqLabel = lv_label_create(scr);
        lv_obj_set_style_text_align(eqLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(eqLabel, "");
        lv_obj_set_width(eqLabel, 40);
        lv_obj_align(eqLabel, LV_ALIGN_BOTTOM_MID, 8, 0);
        lvgl_port_unlock();
    }
}
