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

#ifdef __cplusplus
}
#endif