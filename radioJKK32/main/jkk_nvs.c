
/* 
    * JKK NVS Storage
    * 
    * This file is part of the ESP32 Radio JKK project.
    * It provides functions to read and write data to the NVS (Non-Volatile Storage) of the ESP32.
    * 
    * Copyright (C) 2025 Jaromir Kopp (JKK)
    * This project is licensed under the MIT License.
*/

#include "string.h"
#include "stdio.h"
#include "stdlib.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "jkk_nvs.h"

static const char *TAG = "JKK_NVS";

esp_err_t JkkNvsBlobSet(const char *key, const char *nameSpace, const void *value, size_t length){
    nvs_handle nvsHandle;
    esp_err_t ret = nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS nvsHandle!\n", esp_err_to_name(ret));
        return ESP_ERR_NVS_INVALID_HANDLE;
    } else {
        ret = nvs_set_blob(nvsHandle, key, value, length);
        ESP_LOGI(TAG, "Operation %s", (ret == ESP_OK) ? "Done" : "Failed");
        if (ret != ESP_OK) {
            goto exit;
        }
        ret = nvs_commit(nvsHandle);
        ESP_LOGI(TAG, "Committing operation %s", (ret == ESP_OK) ? "Done" : "Failed");
    }
exit:
    nvs_close(nvsHandle);
    return ret;
}

esp_err_t JkkNvsBlobGet(const char *key, const char *nameSpace, void *value, size_t *length){
    nvs_handle nvsHandle;
    esp_err_t ret = nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS nvsHandle!\n", esp_err_to_name(ret));
        goto exit;
    } else {
        ESP_LOGI(TAG, "Read from NVS ... ");
        size_t lenTmp = 0;
        ret = nvs_get_blob(nvsHandle, key, NULL, &lenTmp);
        if (length != NULL && *length > 0 && *length != lenTmp) {
            ESP_LOGE(TAG, "Length mismatch: expected %d, got %d", *length, lenTmp);
            nvs_close(nvsHandle);
            return ESP_ERR_NVS_INVALID_LENGTH;
        }
        ret = nvs_get_blob(nvsHandle, key, value, &lenTmp);
        ESP_LOGI(TAG, "Operation %s", (ret == ESP_OK) ? "Done" : "Failed");
        nvs_close(nvsHandle);
    }
exit:
    return ret;
}

esp_err_t JkkNvsErase(const char *key, const char *nameSpace){
    nvs_handle nvsHandle = 0;
    esp_err_t ret = nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
     if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS nvsHandle!\n", esp_err_to_name(ret));
        return ESP_ERR_NVS_INVALID_HANDLE;
    }
    if (!key) {
        ret = nvs_erase_all(nvsHandle);
    } else {
        ret = nvs_erase_key(nvsHandle, key);
    }
    nvs_commit(nvsHandle);
    nvs_close(nvsHandle);

    return ret;
}

