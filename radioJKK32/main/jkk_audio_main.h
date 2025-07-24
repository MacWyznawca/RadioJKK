/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * Audio Main Pipeline and elements
*/

#pragma once

#include <string.h>

#include "audio_common.h"
#include "audio_element.h"
#include "audio_pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JKK_MAX_PIPELINE_ELEMENTS (8)

typedef enum {
    JKK_AUDIO_STATE_STOPPED = 0,
    JKK_AUDIO_STATE_PLAYING,
    JKK_AUDIO_STATE_PAUSED,
    JKK_AUDIO_STATE_ERROR
} jkk_audio_state_t;

typedef struct JkkAudioMain_s {
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t input;
    audio_element_handle_t vmeter;
    audio_element_handle_t decoder;
    audio_element_handle_t split;
    audio_element_handle_t processing;
    audio_element_handle_t output;
    const char *linkElementsAll[JKK_MAX_PIPELINE_ELEMENTS];
    const char *linkElementsNoProcessing[JKK_MAX_PIPELINE_ELEMENTS - 1];
    int linkElementsAllCount;
    bool lineWithProcess;
    int input_type; // 0 - raw, 1 - i2s, 2 - fatfs, 3 - http
    int output_type; // 0 - raw, 1 - i2s, 2 - fatfs, 3 - http
    int processing_type; // 0 - none, 1 - equalizer
    int channels; // number of channels
    int sample_rate; // sample rate of audio stream
    int bits; // bits per sample
    int raw_split_nr; // number of raw split outputs
    jkk_audio_state_t audio_state;
    bool audio_was_paused; // true if audio was paused before
} JkkAudioMain_t;

/**
 * @brief Get current audio playback state
 * @return Current audio state
 */
jkk_audio_state_t JkkAudioGetState(void);

/**
 * @brief Start/Resume audio playback
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioPlay(void);

/**
 * @brief Pause audio playback
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioPause(void);

/**
 * @brief Stop audio playback
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioStop(void);

/**
 * @brief Check if audio is currently playing
 * @return true if playing, false otherwise
 */
bool JkkAudioIsPlaying(void);

/**
 * @brief Toggle play/pause state
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioTogglePlayPause(void);

/**
 * @brief Set equalizer gain for all bands
 * @param eqGainArray Array of gain values for each band
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioEqSetAll(const int *eqGainArray);

/**
 * @brief Set equalizer processing element info
 * @param rate Sample rate for equalizer
 * @param ch Number of channels for equalizer
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioEqSetInfo(int rate, int ch);

/**
 * @brief Set I2S clock parameters
 * @param rate Sample rate for I2S
 * @param bits Bits per sample for I2S
 * @param ch Number of channels for I2S
 * @param out true if setting for output, false for input
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioI2sSetClk(int rate, int bits, int ch, bool out);

/**
 * @brief Set URL for HTTP/FATFS stream
 * @param url URL to set for the stream
 * @param out true if setting for output stream, false for input stream
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioSetUrl(const char *url, bool out);

/**
 * @brief Restart audio stream
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioRestartStream(void);

/**
 * @brief Initialize audio main pipeline
 * @param inType Input stream type (0 - RAW, 1 - I2S, 2 - FATFS, 3 - HTTP)
 * @param outType Output stream type (0 - RAW, 1 - I2S, 2 - FATFS, 3 - HTTP)
 * @param processingType Processing type (0 - NONE, 1 - EQUALIZER)
 * @param rawSplitNr Number of raw split outputs
 * @return Pointer to initialized JkkAudioMain_t structure or NULL on failure
 */
JkkAudioMain_t *JkkAudioMain_init(int inType, int outType, int processingType, int rawSplitNr);

/**
 * @brief Enable or disable audio processing in the pipeline
 * @param on true to enable processing, false to disable
 * @param evt Event interface handle for processing events
 * @return ESP_OK on success, error code on failure
 */
esp_err_t JkkAudioMainOnOffProcessing(bool on, audio_event_iface_handle_t evt);

/**
 * @brief Check if audio processing is currently active
 * @return true if processing is active, false otherwise    
 */
bool JkkAudioMainProcessState(void);

/**
 * @brief Deinitialize audio main pipeline and all elements 
 */
void JkkAudioMain_deinit(void);

#ifdef __cplusplus
}
#endif