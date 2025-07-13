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

#include "jkk_tools.h"

#include "jkk_lcd_port.h"

#include "../jkk_radio.h"

#if CONFIG_JKK_RADIO_LCD_CONTROLLER_SH1107
#include "esp_lcd_sh1107.h"
#else
#include "esp_lcd_panel_vendor.h"
#endif

#include "jkk_mono_lcd.h"

static const char *TAG = "LCD";

static JkkRadio_t *jkkRadio = NULL;

static lv_obj_t *radioLabel = NULL;
static lv_obj_t *volLabel = NULL;
static lv_obj_t *recLabel = NULL;
static lv_obj_t *eqLabel = NULL;
static lv_obj_t *timeLabel = NULL;
static lv_obj_t *rolerLabel = NULL;
static lv_obj_t *lineVMeter = NULL;
static lv_obj_t *lineVolume = NULL;
static lv_obj_t *qr = NULL;

static lv_obj_t *roller = NULL;
static lv_group_t *rollGroup;
static jkkRollerMode_t rollerMode = JKK_ROLLER_MODE_HIDE;

static lv_timer_t *rollerTimer = NULL;
// static lv_timer_t *stationScrollTimer = NULL;

static lv_indev_t *indev_encoder = NULL;
static uint32_t key = 0;
static int8_t keyPressed = -1;

static lv_point_precise_t lineVM_points[2] =  {{.x = 63, .y = 0}, {.x = 64, .y = 0}};
static lv_point_precise_t lineVol_points[2] =  {{.x = 63, .y = 0}, {.x = 64, .y = 0}};

//void ScrollLabTimerHandler(lv_timer_t * timer){
//    lv_timer_pause(timer);
//    lv_label_set_long_mode(radioLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
//}

void RollerHideTimerHandler(lv_timer_t * timer){
    lv_timer_pause(timer);
    JkkLcdButtonSet(LV_KEY_ESC, 1);
    lv_obj_add_flag(roller, LV_OBJ_FLAG_HIDDEN); 
    lv_obj_add_flag(rolerLabel, LV_OBJ_FLAG_HIDDEN); 
    rollerMode = JKK_ROLLER_MODE_HIDE;
}

void JkkLcdStationTxt(char *stationName) {
    if(radioLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    char stationNameTmp[130] = {0};
    Utf8ToAsciiPL(stationName, stationNameTmp);
    if(JkkLcdPortLock(0)){   
      //  lv_label_set_long_mode(radioLabel, LV_LABEL_LONG_CLIP);   
        lv_label_set_text(radioLabel, stationNameTmp);       
     //   lv_timer_reset(stationScrollTimer);
     //   lv_timer_resume(stationScrollTimer);
        JkkLcdPortUnlock();
    } else {
        ESP_LOGE(TAG, "Failed to lock the port for updating station text");
    }
}

void JkkLcdRec(bool rec) {
    static bool recState = false;
    if(recLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    if(recState == rec) {
        ESP_LOGD(TAG, "Recording state unchanged: %s", rec ? "true" : "false");
        return;
    }
    if(JkkLcdPortLock(0)){
        if(rec) lv_label_set_text_static(recLabel, "rec");
        else lv_label_set_text_static(recLabel, "");
        JkkLcdPortUnlock();
        recState = rec;
    }
}

void JkkLcdVolumeInt(int vol) {
    if(volLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }

    lineVol_points[1].x = 64 + ((vol * 63) / 100);
    lineVol_points[0].x = 63 - ((vol * 63) / 100);

    char volTxt[10] = {0};
    if(vol == 0){
        snprintf(volTxt, sizeof(volTxt), "MUTE");
    }
    else{
        snprintf(volTxt, sizeof(volTxt), "V%d%%", vol);
    }
    if(JkkLcdPortLock(0)){
        lv_label_set_text(volLabel, volTxt);
        lv_obj_invalidate(lineVolume);
        JkkLcdPortUnlock();
    }
}

void JkkLcdEqTxt(char *eqName) {
    if(eqLabel == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    if(JkkLcdPortLock(0)){
        lv_label_set_text(eqLabel, eqName);
        JkkLcdPortUnlock();
    }
}

void JkkLcdTimeTxt(char *timeName) {
    if(timeName == NULL) {
        ESP_LOGE(TAG, "Display not initialized");
        return;
    }
    if(JkkLcdPortLock(0)){
        lv_label_set_text(timeLabel, timeName);
        JkkLcdPortUnlock();
    }
}

void JkkLcdButtonSet(int keyCode, int8_t pressed) {
    if(keyCode < LV_KEY_ENTER || keyCode > LV_KEY_ESC) {
        ESP_LOGE(TAG, "Invalid key code: %d", keyCode);
        return;
    }
    if(pressed < 0 || pressed > 1) {
        ESP_LOGE(TAG, "Invalid pressed state: %d", pressed);
        return;
    }
    ESP_LOGI(TAG, "JkkNavigationButtonsRead: key=%d, keyPressed=%d", keyCode, pressed);


    if(keyCode != LV_KEY_ESC && JkkLcdPortLock(0)){
        lv_timer_reset(rollerTimer);
        lv_timer_resume(rollerTimer);
        JkkLcdPortUnlock();
    }

    key = keyCode;
    keyPressed = pressed;
}

void JkkLcdVolumeIndicatorCallback(int left_volume, int right_volume) {
    lineVM_points[1].x = 64 + ((right_volume * 63) / 100);
    lineVM_points[0].x = 63 - ((left_volume * 63) / 100);
    if(JkkLcdPortLock(0)){
        lv_obj_invalidate(lineVMeter);
        JkkLcdPortUnlock();
    }
}

static void JkkNavigationButtonsRead(lv_indev_t *indev_drv, lv_indev_data_t *data){
    static uint32_t last_key = 0;
  //  static int8_t lastPressed = -1;

    if(keyPressed == 1) {
        if (key == LV_KEY_LEFT) {
            data->key = LV_KEY_LEFT;
            last_key = LV_KEY_LEFT;
            data->state = LV_INDEV_STATE_PRESSED;
        } else if (key == LV_KEY_RIGHT) {
            data->key = LV_KEY_RIGHT;
            last_key = LV_KEY_RIGHT;
            data->state = LV_INDEV_STATE_PRESSED;
        } else if (key == LV_KEY_ENTER) {
            data->key = LV_KEY_ENTER;
            last_key = LV_KEY_ENTER;
            data->state = LV_INDEV_STATE_PRESSED;
        } else if (key == LV_KEY_ESC) {
            data->key = LV_KEY_ESC;
            last_key = LV_KEY_ESC;
            data->state = LV_INDEV_STATE_PRESSED;
        }
        else {
            data->key = last_key;
            data->state = LV_INDEV_STATE_RELEASED;
            last_key = 0;
        }
    }
    
    key = 0;
}

static void JkkLcdShowObj(lv_obj_t *obj, bool show){
    if(obj == NULL) {
        ESP_LOGE(TAG, "Invalid object");
        return;
    }
    if(JkkLcdPortLock(0)){
        if(show) {
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
            if(obj == roller) {
                lv_obj_clear_flag(rolerLabel, LV_OBJ_FLAG_HIDDEN);
                if(eqLabel && jkkRadio){
                    switch (rollerMode) {
                        case JKK_ROLLER_MODE_STATION_LIST:{
                            if(JkkLcdPortLock(0)){
                                char stationNameTmp[256] = {0};
                                Utf8ToAsciiPL(jkkRadio->jkkRadioStations[jkkRadio->current_station].nameLong, stationNameTmp);
                                lv_label_set_text(rolerLabel, stationNameTmp);
                                JkkLcdPortUnlock();
                            }
                        }
                        break;
                        case JKK_ROLLER_MODE_EQUALIZER_LIST:{
                            if(JkkLcdPortLock(0)){
                                lv_label_set_text(rolerLabel, jkkRadio->eqPresets[jkkRadio->current_eq].name);
                                JkkLcdPortUnlock();
                            }
                        }
                        break;
                        default:
                        break;
                    }
                }
            }

            lv_timer_reset(rollerTimer);
            lv_timer_resume(rollerTimer);
        } else {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); 
            if(obj == roller) {
                lv_obj_add_flag(rolerLabel, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(rolerLabel, "");
                rollerMode = JKK_ROLLER_MODE_HIDE;
            }
            lv_timer_pause(rollerTimer);
        }
        JkkLcdPortUnlock();
    }
}

static void RadioRollerHandler(lv_event_t * e){
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);

    int indx = -1;

    if(code == LV_EVENT_KEY){
        indx = lv_roller_get_selected(obj);
        if(eqLabel && jkkRadio){
            switch (rollerMode) {
                case JKK_ROLLER_MODE_STATION_LIST:{
                    if(indx >= 0 && indx < jkkRadio->station_count){
                        if(JkkLcdPortLock(0)){
                            char stationNameTmp[256] = {0};
                            Utf8ToAsciiPL(jkkRadio->jkkRadioStations[indx].nameLong, stationNameTmp);
                            lv_label_set_text(rolerLabel, stationNameTmp);
                            JkkLcdPortUnlock();
                        }
                    }
                    else {
                        lv_label_set_text(rolerLabel, "");
                    }
                }
                break;
                case JKK_ROLLER_MODE_EQUALIZER_LIST:{
                    if(indx >= 0 && indx < jkkRadio->eq_count){
                        if(JkkLcdPortLock(0)){
                            lv_label_set_text(rolerLabel, jkkRadio->eqPresets[indx].name);
                            JkkLcdPortUnlock();
                        }
                    }
                    else {
                        lv_label_set_text(rolerLabel, "");
                    }
                }
                break;
                default:
                break;
            }
        }

        ESP_LOGI(TAG, "Roller event code: %d, indx: %d\n", code, indx);
    }

    if(code == LV_EVENT_VALUE_CHANGED){
        indx = lv_roller_get_selected(obj);
        ESP_LOGI(TAG, "Selected: %d\n", indx);
        customCmd_e command = JKK_RADIO_CMD_SET_UNKNOW;
        switch (rollerMode) {
            case JKK_ROLLER_MODE_STATION_LIST:
                command = JKK_RADIO_CMD_SET_STATION;
            break;
            case JKK_ROLLER_MODE_EQUALIZER_LIST:
                command = JKK_RADIO_CMD_SET_EQUALIZER;
            break;
            default:
            break;
        }
        JkkLcdShowRoller(false, UINT8_MAX, JKK_ROLLER_MODE_HIDE);
        JkkRadioSendMessageToMain(indx, command);
    }
}

jkkRollerMode_t JkkLcdRollerMode(void){
    if(JkkLcdPortLock(0)){
        rollerMode = lv_obj_has_flag(roller, LV_OBJ_FLAG_HIDDEN) ? JKK_ROLLER_MODE_HIDE : rollerMode;
        JkkLcdPortUnlock();
    }
    return rollerMode;
}

void JkkLcdShowRoller(bool show, uint8_t idx, jkkRollerMode_t mode){
    if(mode > JKK_ROLLER_MODE_HIDE && mode < JKK_ROLLER_MODE_UNKNOWN) rollerMode = mode;
    JkkLcdShowObj(roller, show);
    
    if(idx < lv_roller_get_option_count(roller)) {
        if(JkkLcdPortLock(0)){
            lv_roller_set_selected(roller, idx, LV_ANIM_ON);
            JkkLcdPortUnlock();
        }
    }
}

void JkkLcdSetRollerOptions(char *options, uint8_t idx){
    if(JkkLcdPortLock(0)){
        lv_roller_set_options(roller, options, LV_ROLLER_MODE_INFINITE);
        if(idx < lv_roller_get_option_count(roller)) {
            lv_roller_set_selected(roller, idx, LV_ANIM_OFF);
        }
        JkkLcdPortUnlock();
    }
}

void JkkLcdQRcode(const char *url){
    if(JkkLcdPortLock(0)){
        if(url){
            lv_qrcode_update(qr, url, strlen(url));
            lv_obj_clear_flag(qr, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(qr, LV_OBJ_FLAG_HIDDEN);
        }
        JkkLcdPortUnlock();
    }
    
}

esp_err_t JkkLcdUiInit(JkkRadio_t *radio){

    jkkRadio = radio;

    lv_disp_t *display = JkkLcdPortInit();

    if(JkkLcdPortLock(0)){
        lv_obj_t *scr = lv_display_get_screen_active(display);

        indev_encoder = lv_indev_create();
        lv_indev_set_type(indev_encoder, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(indev_encoder, JkkNavigationButtonsRead);
        lv_indev_enable(indev_encoder, true);
        lv_indev_set_display(indev_encoder, display);

        radioLabel = lv_label_create(scr);
        lv_obj_set_style_text_font(radioLabel, &lv_font_unscii_16, 0);
        lv_label_set_long_mode(radioLabel, LV_LABEL_LONG_SCROLL_CIRCULAR); /* Circular scroll LV_LABEL_LONG_SCROLL_CIRCULAR */  
        lv_obj_set_style_text_align(radioLabel, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_text(radioLabel, "Radio JKK32 - Multifunction Internet Radio Player");
        lv_obj_set_width(radioLabel, lv_display_get_horizontal_resolution(display));
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
        lv_obj_set_width(recLabel, 26);
        lv_obj_align(recLabel, LV_ALIGN_RIGHT_MID, 0, 0);

        eqLabel = lv_label_create(scr);
        lv_obj_set_style_text_font(eqLabel, &lv_font_unscii_8, 0);
        lv_obj_set_style_text_align(eqLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(eqLabel, "");
        lv_obj_set_width(eqLabel, 42);
        lv_obj_align(eqLabel, LV_ALIGN_BOTTOM_MID, -2, 0);

        timeLabel = lv_label_create(scr);
        lv_obj_set_style_text_font(timeLabel, &lv_font_unscii_8, 0);
        lv_obj_set_style_text_align(timeLabel, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(timeLabel, "");
        lv_obj_set_width(timeLabel, 42);
        lv_obj_align(timeLabel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

        static lv_style_t style_VMeter;
        lv_style_init(&style_VMeter);
        lv_style_set_line_width(&style_VMeter, 3);

        lineVMeter = lv_line_create(scr);
        lv_line_set_points(lineVMeter, lineVM_points, 2);     /*Set the points*/
        lv_obj_add_style(lineVMeter, &style_VMeter, 0);
        lv_obj_set_width(lineVMeter, 128),
        lv_obj_set_height(lineVMeter, 3),
        lv_obj_align(lineVMeter, LV_ALIGN_BOTTOM_MID, 0, -16);

        static lv_style_t style_Volume;
        lv_style_init(&style_Volume);
        lv_style_set_line_width(&style_Volume, 2);

        lineVolume = lv_line_create(scr);
        lv_line_set_points(lineVolume, lineVol_points, 2);     /*Set the points*/
        lv_obj_add_style(lineVolume, &style_Volume, 0);
        lv_obj_set_width(lineVolume, 128),
        lv_obj_set_height(lineVolume, 2),
        lv_obj_align(lineVolume, LV_ALIGN_BOTTOM_MID, 0, -13);

        static lv_style_t style_rollLab;
        lv_style_init(&style_rollLab);
        lv_style_set_bg_opa(&style_rollLab, LV_OPA_COVER);
        lv_style_set_border_width(&style_rollLab, 6);
        lv_style_set_border_color(&style_rollLab, lv_color_white());

        lv_color_t bg_color = lv_color_white(); // lv_color_white();
        lv_color_t fg_color = lv_color_black(); // lv_color_black();

        qr = lv_qrcode_create(scr);
        lv_qrcode_set_size(qr, 68);
        lv_qrcode_set_dark_color(qr, fg_color);
        lv_qrcode_set_light_color(qr, bg_color);
        lv_obj_set_style_border_color(qr, bg_color, 0);
        lv_obj_set_style_border_width(qr, 3, 0);
        lv_obj_align(qr, LV_ALIGN_LEFT_MID, -4, 0);
        lv_obj_add_flag(qr, LV_OBJ_FLAG_HIDDEN);

        rolerLabel = lv_label_create(scr);
        lv_obj_set_style_text_font(rolerLabel, &lv_font_unscii_8, 0);
        lv_obj_set_style_text_align(rolerLabel, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_text(rolerLabel, "");
        lv_obj_add_style(rolerLabel, &style_rollLab, 0);
        lv_obj_set_width(rolerLabel, 72);
        lv_obj_align(rolerLabel, LV_ALIGN_LEFT_MID, -6, 0);
        lv_obj_add_flag(rolerLabel, LV_OBJ_FLAG_HIDDEN);

        rollGroup = lv_group_create();

        static lv_style_t style_rol;
        lv_style_init(&style_rol);
        lv_style_set_opa(&style_rol, LV_OPA_COVER); 
        lv_style_set_text_font(&style_rol, &lv_font_unscii_8);
        lv_style_set_radius(&style_rol, 0);
        
        roller = lv_roller_create(scr);  
        lv_roller_set_options(roller, "-", LV_ROLLER_MODE_INFINITE);
        
        lv_obj_add_style(roller, &style_rol, 0);
        lv_obj_set_width(roller, 68);

        lv_obj_set_style_text_line_space(roller, 2, 0);
        lv_roller_set_visible_row_count(roller, 5);
        lv_obj_align(roller, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_roller_set_selected(roller, 0, LV_ANIM_OFF);
        lv_obj_add_event_cb(roller, RadioRollerHandler, LV_EVENT_ALL, NULL);

        lv_group_add_obj(rollGroup, roller);

        lv_indev_set_group(indev_encoder, rollGroup);

        lv_obj_add_flag(roller, LV_OBJ_FLAG_HIDDEN);
        rollerMode = JKK_ROLLER_MODE_HIDE;

        rollerTimer = lv_timer_create_basic();
        lv_timer_pause(rollerTimer);
        lv_timer_set_cb(rollerTimer, RollerHideTimerHandler);
        lv_timer_set_period(rollerTimer, JKK_ROLLER_TIME_TO_HIDE * 1000);

        JkkLcdPortUnlock();  
    }
    return ESP_OK;
}
