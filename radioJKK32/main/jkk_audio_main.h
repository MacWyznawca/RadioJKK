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
} JkkAudioMain_t;


esp_err_t JkkAudioEqSetAll(const int *eqGainArray);

esp_err_t JkkAudioEqSetInfo(int rate, int ch);

esp_err_t JkkAudioI2sSetClk(int rate, int bits, int ch, bool out);

esp_err_t JkkAudioSetUrl(const char *url, bool out);

esp_err_t JkkAudioRestartStream(void);

JkkAudioMain_t *JkkAudioMain_init(int inType, int outType, int processingType, int rawSplitNr);

esp_err_t JkkAudioMainOnOffProcessing(bool on, audio_event_iface_handle_t evt);

bool JkkAudioMainProcessState(void);

void JkkAudioMain_deinit(void);

#ifdef __cplusplus
}
#endif