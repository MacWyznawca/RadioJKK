/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * Audio write to SD Pipeline and elements
*/

#pragma once

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

void JkkLcdInit(void);

bool JkkLcdPortLock(uint32_t timeout_ms);

void JkkLcdPortUnlock(void);

void JkkLcdStationTxt(char *stationName);

void JkkLcdVolumeInt(int vol);

void JkkLcdRec(bool rec);

void JkkLcdEqTxt(char *eqName);


#ifdef __cplusplus
}
#endif