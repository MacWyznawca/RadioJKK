/* 
    * JKK NVS Storage
    * 
    * This file is part of the ESP32 Radio JKK project.
    * It provides functions to read and write data to the NVS (Non-Volatile Storage) of the ESP32.
    * 
    * Copyright (C) 2025 Jaromir Kopp (JKK)
    * This project is licensed under the MIT License.
*/

#pragma once

#include <esp_err.h>
#include "esp_partition.h"

#ifdef __cplusplus
extern "C"
{
#endif

esp_err_t JkkNvsBlobGet(const char *key, const char *nameSpace, void *value, size_t *length);
esp_err_t JkkNvsBlobSet(const char *key, const char *nameSpace, const void *value, size_t length);
esp_err_t JkkNvs64_set(const char *key, const char *nameSpace, uint64_t value);
esp_err_t JkkNvs64_get(const char *key, const char *nameSpace, uint64_t *value);
esp_err_t JkkNvsErase(const char *key, const char *nameSpace);

#ifdef __cplusplus
}
#endif