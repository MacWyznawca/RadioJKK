/* 
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * 
 * LCD i2c mono OLED port
 *
 */

#pragma once

#include <string.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LCD port and LVGL display
 * @return Pointer to LVGL display object or NULL on failure
 */
lv_disp_t *JkkLcdPortInit(void);

/**
 * @brief Lock LVGL mutex for thread-safe access
 * @param timeout_ms Timeout in milliseconds (0 for no timeout)
 * @return true if lock acquired, false on timeout
 */
bool JkkLcdPortLock(uint32_t timeout_ms);

/**
 * @brief Unlock LVGL mutex
 */
void JkkLcdPortUnlock(void);

/**
 * @brief Turn LCD panel on or off
 * @param turn_on true to turn on, false to turn off
 * @return Current state of the panel (true = on, false = off)
 */
bool JkkLcdPortOnOffLcd(bool turn_on);

/**
 * @brief Get current LCD panel state
 * @return true if panel is on, false if off
 */
bool JkkLcdPortGetLcdState(void);

#ifdef __cplusplus
}
#endif