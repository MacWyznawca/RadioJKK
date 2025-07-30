/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * Web server interface for radio control
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start HTTP web server
 * Initializes and starts the web server with all endpoints for radio control
 */
void start_web_server(void);

/**
 * @brief Stop HTTP web server
 * Stops the web server and frees associated resources
 */
void stop_web_server(void);

/**
 * @brief Update current station ID for web interface
 * @param id Current station ID (-1 for error state)
 */
void JkkRadioWwwSetStationId(int8_t id);

/**
 * @brief Update station list for web interface
 * @param stationList Formatted station list string
 */
void JkkRadioWwwUpdateStationList(const char *stationList);

/**
 * @brief Update equalizer list for web interface
 * @param list Formatted equalizer list string
 */
void JkkRadioWwwUpdateEqList(const char *list);

/**
 * @brief Update volume level for web interface
 * @param volume Current volume level (0-100)
 */
void JkkRadioWwwUpdateVolume(uint8_t volume);

/**
 * @brief Update current equalizer ID for web interface
 * @param id Current equalizer preset ID
 */
void JkkRadioWwwSetEqId(uint8_t id);

/**
 * @brief Update recording status for web interface
 * @param rec Recording status (1 for recording, 0 for not recording)
 */
void JkkRadioWwwUpdateRecording(int8_t rec);

#ifdef __cplusplus
}
#endif