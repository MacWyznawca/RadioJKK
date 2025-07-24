/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * Audio write to SD Pipeline and elements
*/

#pragma once

#include <string.h>

#include "audio_common.h"
#include "audio_element.h"
#include "audio_pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JkkAudioSdWrite_s {
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t raw_read;
    audio_element_handle_t resample;
    audio_element_handle_t encoder;
    audio_element_handle_t fatfs_wr;
    bool needResample;
    int encoder_type;
    int sample_rate;
    int channels;
    int bits;
    bool is_recording;
    SemaphoreHandle_t recording_mutex; // DODANE
} JkkAudioSdWrite_t;

/**
 * @brief Change resample filter parameters
 * @param sample_rate New sample rate
 * @param channels Number of channels
 * @param bits Bits per sample
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioSdWriteResChange(int sample_rate, int channels, int bits);

/**
 * @brief Set output file URI for recording
 * @param uri File path for recording output
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioSdWriteSetUri(const char *uri);

/**
 * @brief Restart the audio recording pipeline
 * @param withElements Whether to restart with all elements
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioSdWriteRestartStream(bool withElements);

/**
 * @brief Start audio recording stream
 * @param uri Optional file path for recording output
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioSdWriteStartStream(const char *uri);

/**
 * @brief Stop audio recording stream
 */
void JkkAudioSdWriteStopStream(void);

/**
 * @brief Initialize audio SD write pipeline
 * @param encoder_type Encoder type (0 - none, 1 - AAC, 2 - WAV)
 * @param sample_rate Initial sample rate
 * @param channels Number of audio channels
 * @return Pointer to initialized structure or NULL on failure
 */
JkkAudioSdWrite_t *JkkAudioSdWrite_init(int encoder_type, int sample_rate, int channels);

/**
 * @brief Check if audio is currently recording
 * @return true if recording, false otherwise
 */
bool JkkAudioSdWriteIsRecording(void);

/**
 * @brief Deinitialize audio SD write pipeline
 */
void JkkAudioSdWrite_deinit(void);

#ifdef __cplusplus
}
#endif