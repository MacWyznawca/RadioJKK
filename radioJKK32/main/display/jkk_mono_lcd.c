/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * 
 * LCD I2C OLED 
 * 
 * Based on Espressif Systems (Shanghai) CO LTD example code
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "lvgl.h"


#if CONFIG_JKK_RADIO_LCD_CONTROLLER_SH1107
#include "esp_lcd_sh1107.h"
#else
#include "esp_lcd_panel_vendor.h"
#endif

#include "jkk_mono_lcd.h"

static const char *TAG = "LCD";

#define I2C_BUS_PORT  I2C_NUM_1

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

#define JKK_RADIO_LVGL_TASK_STACK_SIZE   (5 * 1024)
#define JKK_RADIO_LVGL_TASK_PRIORITY     4
#define JKK_RADIO_LVGL_PALETTE_SIZE      8

#define JKK_RADIO_LVGL_TICK_PERIOD_MS    3

static EXT_RAM_BSS_ATTR uint8_t oled_buffer[JKK_RADIO_LCD_H_RES * JKK_RADIO_LCD_V_RES / 8];

static TaskHandle_t dispaskHandle = NULL;

static SemaphoreHandle_t lvglMux = NULL;

//static int taskMaxSleep_ms = 1000;

lv_disp_t *display = NULL;
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
    if(display == NULL || radioLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    char stationNameTmp[130] = {0};
    Utf8ToAsciiPL(stationName, stationNameTmp);
    if(JkkLcdPortLock(0)){;
        if (strlen(stationNameTmp) > 127) {
            // If the station name is longer than 20 characters, truncate it to fit the display
            char truncatedName[128] = {0};
            strncpy(truncatedName, stationNameTmp, 127);
            truncatedName[20] = '\0'; // Ensure null-termination
            lv_label_set_text(radioLabel, truncatedName);
        } else {
            lv_label_set_text(radioLabel, stationNameTmp);
        }
        // Unlock the port
        JkkLcdPortUnlock();
    } else {
        ESP_LOGE(TAG, "Failed to lock the port for updating station text");
    }
}

void JkkLcdRec(bool rec) {
    static bool recState = false;
    if(display == NULL || recLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    if(recState == rec) {
        ESP_LOGD(TAG, "Recording state unchanged: %s", rec ? "true" : "false");
        return;
    }
    if(JkkLcdPortLock(0)){;
        if(rec) lv_label_set_text_static(recLabel, "R");
        else lv_label_set_text_static(recLabel, "");
        JkkLcdPortUnlock();
        recState = rec;
    }
}

void JkkLcdVolumeInt(int vol) {
    if(display == NULL || volLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    char volTxt[10] = {0};
    if(vol == 0){
        snprintf(volTxt, sizeof(volTxt), "MUTE");
    }
    else{
        snprintf(volTxt, sizeof(volTxt), "V:%d%%", vol);
    }
    if(JkkLcdPortLock(0)){
        lv_label_set_text(volLabel, volTxt);
        JkkLcdPortUnlock();
    }
}

void JkkLcdEqTxt(char *eqName) {
    if(display == NULL || eqLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    if(JkkLcdPortLock(0)){
        lv_label_set_text(eqLabel, eqName);
        JkkLcdPortUnlock();
    }
}

bool JkkLcdPortLock(uint32_t timeout_ms){
    assert(lvglMux && "LvglJkkInit must be called first");

    const TickType_t timeoutTicks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvglMux, timeoutTicks) == pdTRUE;
}

void JkkLcdPortUnlock(void){
    assert(lvglMux && "lvgl_port_init must be called first");
    xSemaphoreGiveRecursive(lvglMux);
}

static bool jkk_lcd_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx){
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void jkk_lcd_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map){
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);

    // This is necessary because LVGL reserves 2 x 4 bytes in the buffer, as these are assumed to be used as a palette. Skip the palette here
    // More information about the monochrome, please refer to https://docs.lvgl.io/9.2/porting/display.html#monochrome-displays
    px_map += JKK_RADIO_LVGL_PALETTE_SIZE;

    uint16_t hor_res = lv_display_get_physical_horizontal_resolution(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            /* The order of bits is MSB first
                        MSB           LSB
               bits      7 6 5 4 3 2 1 0
               pixels    0 1 2 3 4 5 6 7
                        Left         Right
            */
            bool chroma_color = (px_map[(hor_res >> 3) * y  + (x >> 3)] & 1 << (7 - x % 8));

            /* Write to the buffer as required for the display.
            * It writes only 1-bit for monochrome displays mapped vertically.*/
            uint8_t *buf = oled_buffer + hor_res * (y >> 3) + (x);
            if (chroma_color) {
                (*buf) &= ~(1 << (y % 8));
            } else {
                (*buf) |= (1 << (y % 8));
            }
        }
    }
    // pass the draw buffer to the driver
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, oled_buffer);
}

static void jkk_lcd_lvgl_port_task(void *arg){
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = 0;

    while (1) {
        if (JkkLcdPortLock(0)) { 
            /* Handle LVGL */
            task_delay_ms = lv_timer_handler();
            JkkLcdPortUnlock();
        } else {
            task_delay_ms = 1; /*Keep trying*/
        }

        if (task_delay_ms == LV_NO_TIMER_READY) {
            task_delay_ms = 1000;
        }

        /* Minimal dealy for the task. When there is too much events, it takes time for other tasks and interrupts. */
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static void IncreaseLvglTick(void *arg){
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(JKK_RADIO_LVGL_TICK_PERIOD_MS);
}

static uint32_t MsTick(void){
    return esp_timer_get_time() / 1000;
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
    ESP_LOGI(TAG, "Using SSD1306 controller");
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = JKK_RADIO_LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
#elif CONFIG_JKK_RADIO_LCD_CONTROLLER_SH1107
    ESP_LOGI(TAG, "Using SH1107 controller");
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh1107(io_handle, &panel_config, &panel_handle));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    esp_lcd_panel_mirror(panel_handle, true, true);

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

#if CONFIG_JKK_RADIO_LCD_CONTROLLER_SH1107
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#endif

    ESP_LOGI(TAG, "Initialize LVGL");
    lv_init();
    lv_tick_set_cb(MsTick);
    // create a lvgl display
    display = lv_display_create(JKK_RADIO_LCD_H_RES, JKK_RADIO_LCD_V_RES);
    // associate the i2c panel handle to the display
    lv_display_set_user_data(display, panel_handle);
    // create draw buffer
    void *buf = NULL;
    ESP_LOGI(TAG, "Allocate separate LVGL draw buffers");
    // LVGL reserves 2 x 4 bytes in the buffer, as these are assumed to be used as a palette.
    size_t draw_buffer_sz = JKK_RADIO_LCD_H_RES * JKK_RADIO_LCD_V_RES / 8 + JKK_RADIO_LVGL_PALETTE_SIZE;
    buf = heap_caps_calloc(1, draw_buffer_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(buf);


    // LVGL9 suooprt new monochromatic format.
    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);
    // initialize LVGL draw buffers
    lv_display_set_buffers(display, buf, NULL, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_FULL);
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, jkk_lcd_lvgl_flush_cb);


    ESP_LOGI(TAG, "Register io panel event callback for LVGL flush ready notification");
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = jkk_lcd_notify_lvgl_flush_ready,
    };
    /* Register done callback */
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display);

    ESP_LOGI(TAG, "Create LVGL task");

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &IncreaseLvglTick,
        .name = "lvglTick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, JKK_RADIO_LVGL_TICK_PERIOD_MS * 1000));

    lvglMux = xSemaphoreCreateRecursiveMutex();
    xTaskCreatePinnedToCoreWithCaps(jkk_lcd_lvgl_port_task, "LVGL", JKK_RADIO_LVGL_TASK_STACK_SIZE, NULL, JKK_RADIO_LVGL_TASK_PRIORITY, &dispaskHandle, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if(JkkLcdPortLock(0)){
        lv_obj_t *scr = lv_display_get_screen_active(display);
        radioLabel = lv_label_create(scr);
        lv_obj_set_style_text_font(radioLabel, &lv_font_unscii_16, 0);
        lv_label_set_long_mode(radioLabel, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll */
        lv_obj_set_style_text_align(radioLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(radioLabel, "Radio JKK32 - Multifunction Internet Radio Player");
        lv_obj_set_width(radioLabel, JKK_RADIO_LCD_H_RES);
        lv_obj_align(radioLabel, LV_ALIGN_TOP_MID, 0, 0);

        volLabel = lv_label_create(scr);
        lv_obj_set_style_text_font(volLabel, &lv_font_unscii_8, 0);
        lv_obj_set_style_text_align(volLabel, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_text(volLabel, "");
        lv_obj_set_width(volLabel, 64);
        lv_obj_align(volLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        recLabel = lv_label_create(scr);
        lv_obj_set_style_text_font(recLabel, &lv_font_unscii_8, 0);
        lv_obj_set_style_text_align(recLabel, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(recLabel, "");
        lv_obj_set_width(recLabel, 16);
        lv_obj_align(recLabel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

        eqLabel = lv_label_create(scr);
        lv_obj_set_style_text_font(eqLabel, &lv_font_unscii_8, 0);
        lv_obj_set_style_text_align(eqLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(eqLabel, "");
        lv_obj_set_width(eqLabel, 42);
        lv_obj_align(eqLabel, LV_ALIGN_BOTTOM_MID, 8, 0);

        JkkLcdPortUnlock();  
    }
}
