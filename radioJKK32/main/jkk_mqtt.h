/* RadioJKK32 - MQTT Integration for Home Assistant
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * MQTT client with HA device-based discovery
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdbool.h>

#define JKK_MQTT_NVS_KEY_BROKER  "mqtt_broker"
#define JKK_MQTT_NVS_KEY_ENABLED "mqtt_on"
#define JKK_MQTT_NVS_KEY_USER    "mqtt_user"
#define JKK_MQTT_NVS_KEY_PASS    "mqtt_pass"

/**
 * @brief Set MQTT enabled/disabled state (saved to NVS).
 * @param enabled true=enabled, false=disabled
 * @return ESP_OK on success
 */
esp_err_t JkkMqttSetEnabled(bool enabled);

/**
 * @brief Check if MQTT is enabled in NVS (default: true).
 * @return true if enabled
 */
bool JkkMqttIsEnabled(void);

/**
 * @brief Initialize MQTT subsystem.
 * Discovers broker via mDNS (_mqtt._tcp) or uses NVS-stored address.
 * Must be called after WiFi is connected.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no broker found
 */
esp_err_t JkkMqttInit(void);

/**
 * @brief Publish current radio state to MQTT state topic.
 * Builds JSON from current radio getters and publishes to rjkk/{id}/state.
 * Safe to call even if MQTT is not connected (will be silently ignored).
 */
void JkkMqttPublishState(void);

/**
 * @brief Re-publish HA MQTT discovery payloads.
 * Call after station list changes (add/delete/reorder/edit) so HA
 * dropdown options update immediately without device restart.
 * Safe to call even if MQTT is not connected (will be silently ignored).
 */
void JkkMqttPublishDiscovery(void);

/**
 * @brief Check if MQTT client is connected to broker.
 * @return true if connected
 */
bool JkkMqttIsConnected(void);

/**
 * @brief Store MQTT broker address in NVS.
 * Format: "host" or "host:port" (port defaults to 1883).
 * @param address Broker address string
 * @return ESP_OK on success
 */
esp_err_t JkkMqttSetBrokerAddress(const char *address);

/**
 * @brief Read MQTT broker address from NVS.
 * @param address Buffer to store address
 * @param len Buffer length
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if not set
 */
esp_err_t JkkMqttGetBrokerAddress(char *address, size_t len);

/**
 * @brief Store MQTT credentials in NVS.
 * Pass NULL or empty string to clear.
 */
esp_err_t JkkMqttSetCredentials(const char *user, const char *pass);

/**
 * @brief Read MQTT username from RAM cache.
 */
esp_err_t JkkMqttGetUsername(char *buf, size_t len);

/**
 * @brief Read MQTT password from RAM cache.
 */
esp_err_t JkkMqttGetPassword(char *buf, size_t len);

/**
 * @brief Reconnect MQTT client with current settings.
 * Stops existing client and starts a new one using RAM-cached config.
 * Safe to call from internal-RAM task (event handler).
 */
void JkkMqttReconnect(void);

#ifdef __cplusplus
}
#endif
