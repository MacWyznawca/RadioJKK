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

/**
 * @brief Get binary data from NVS
 * @param key NVS key name
 * @param nameSpace NVS namespace
 * @param value Pointer to buffer to store the data
 * @param length Pointer to size of the buffer/actual data size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkNvsBlobGet(const char *key, const char *nameSpace, void *value, size_t *length);

/**
 * @brief Set binary data to NVS
 * @param key NVS key name
 * @param nameSpace NVS namespace
 * @param value Pointer to data to store
 * @param length Size of data to store
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkNvsBlobSet(const char *key, const char *nameSpace, const void *value, size_t length);

/**
 * @brief Set 64-bit unsigned integer value to NVS
 * @param key NVS key name
 * @param nameSpace NVS namespace
 * @param value 64-bit value to store
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkNvs64_set(const char *key, const char *nameSpace, uint64_t value);

/**
 * @brief Get 64-bit unsigned integer value from NVS
 * @param key NVS key name
 * @param nameSpace NVS namespace
 * @param value Pointer to store the retrieved value
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkNvs64_get(const char *key, const char *nameSpace, uint64_t *value);

/**
 * @brief Erase key or entire namespace from NVS
 * @param key NVS key name (NULL to erase entire namespace)
 * @param nameSpace NVS namespace
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkNvsErase(const char *key, const char *nameSpace);

#ifdef __cplusplus
}
#endif