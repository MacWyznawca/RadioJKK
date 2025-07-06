/* 
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * 
 * LCD i2c mono OLED port
 *
 */

#pragma once

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

lv_disp_t *JkkLcdPortInit(void);

bool JkkLcdPortLock(uint32_t timeout_ms);

void JkkLcdPortUnlock(void);

#ifdef __cplusplus
}
#endif