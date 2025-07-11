/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * Audio write to SD Pipeline and elements
*/

#pragma once

#include <string.h>
#include "../jkk_radio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JKK_ROLLER_TIME_TO_HIDE (10)

typedef enum {
    JKK_ROLLER_MODE_HIDE = 0,
    JKK_ROLLER_MODE_STATION_LIST,
    JKK_ROLLER_MODE_EQUALIZER_LIST,
    JKK_ROLLER_MODE_UNKNOWN, 
} jkkRollerMode_t;

esp_err_t JkkLcdUiInit(JkkRadio_t *radio);

void JkkLcdStationTxt(char *stationName);

void JkkLcdVolumeInt(int vol);

void JkkLcdRec(bool rec);

void JkkLcdEqTxt(char *eqName);

void JkkLcdTimeTxt(char *timeName);

void JkkLcdButtonSet(int keyCode, int8_t pressed);

void JkkLcdShowRoller(bool show, uint8_t idx, jkkRollerMode_t mode);

void JkkLcdSetRollerOptions(char *options, uint8_t idx);

jkkRollerMode_t JkkLcdRollerMode(void);

void JkkLcdVolumeIndicatorCallback(int left_volume, int right_volume);

void JkkLcdSetCpuLoadMode(int8_t mode);

#ifdef __cplusplus
}
#endif