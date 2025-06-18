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
    audio_element_handle_t raw_read; // raw stream to read data
    audio_element_handle_t resample; // resample filter to convert sample rate
    audio_element_handle_t encoder; // encoder
    audio_element_handle_t fatfs_wr; // fatfs stream to write data
    bool needResample; // true if resampling is needed
    int encoder_type; // 0 - none, 1 - aac, 2 - wav
    int sample_rate; // sample rate of audio stream
    int channels; // number of channels
    int bits; // bits per sample
    bool is_recording; // Flag indicating if the radio is currently recording
} JkkAudioSdWrite_t;

esp_err_t JkkAudioSdWriteResChange(int sample_rate, int channels, int bits);

esp_err_t JkkAudioSdWriteSetUri(const char *uri);

esp_err_t JkkAudioSdWriteRestartStream(bool withElements);

esp_err_t JkkAudioSdWriteStartStream(const char *uri) ;

void JkkAudioSdWriteStopStream(void);

JkkAudioSdWrite_t *JkkAudioSdWrite_init(int encoder_type, int sample_rate, int channels);

void JkkAudioSdWrite_deinit(void);

#ifdef __cplusplus
}
#endif