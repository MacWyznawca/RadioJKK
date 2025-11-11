/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * Audio Main Pipeline and elements
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "http_stream.h"
#include "fatfs_stream.h"
#include "raw_stream.h"
#include "i2s_stream.h"
#include "esp_decoder.h"
#include "filter_resample.h"
#include "equalizer.h"
#include "RawSplit/raw_split.h"
#include "vmeter/volume_meter.h"
#include "display/jkk_mono_lcd.h"

#include "jkk_audio_main.h"

static const char *TAG = "A_Main";

#define NUMBER_BAND (10)

static  JkkAudioMain_t audioMain = {0}; // EXT_RAM_BSS_ATTR

static int _http_stream_event_handle(http_stream_event_msg_t *msg){
    if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS) {
        return ESP_OK;
    }
    if (msg->event_id == HTTP_STREAM_FINISH_TRACK) {
        return http_stream_next_track(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST) {
        return http_stream_fetch_again(msg->el);
    }
    return ESP_OK;
}

jkk_audio_state_t JkkAudioGetState(void) {
    return audioMain.audio_state;
}

bool JkkAudioIsPlaying(void) {
    if (audioMain.pipeline == NULL) {
        return false;
    }
    
    audio_element_state_t input_state = audio_element_get_state(audioMain.input);
    audio_element_state_t output_state = audio_element_get_state(audioMain.output);
    
    return (input_state == AEL_STATE_RUNNING && output_state == AEL_STATE_RUNNING); 
}

esp_err_t JkkAudioPlay(void) {
    if (audioMain.pipeline == NULL) {
        ESP_LOGE(TAG, "Audio pipeline is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = ESP_OK;
    
    if (audioMain.audio_state == JKK_AUDIO_STATE_PAUSED) {
        // Resume from pause
        ESP_LOGI(TAG, "Resuming audio playback");
        ret = audio_pipeline_resume(audioMain.pipeline);
        if (ret == ESP_OK) {
            audioMain.audio_state = JKK_AUDIO_STATE_PLAYING;
            audioMain.audio_was_paused = false;
        }
    } else if (audioMain.audio_state == JKK_AUDIO_STATE_STOPPED) {
        // Start playback
        ESP_LOGI(TAG, "Starting audio playback");
        ret = audio_pipeline_run(audioMain.pipeline);
        if (ret == ESP_OK) {
            audioMain.audio_state = JKK_AUDIO_STATE_PLAYING;
            audioMain.audio_was_paused = false;
        }
    } else {
        ESP_LOGW(TAG, "Audio is already playing");
        return ESP_OK;
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start/resume audio: %s", esp_err_to_name(ret));
        audioMain.audio_state = JKK_AUDIO_STATE_ERROR;
    }
    
    return ret;
}

esp_err_t JkkAudioPause(void) {
    if (audioMain.pipeline == NULL) {
        ESP_LOGE(TAG, "Audio pipeline is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (audioMain.audio_state != JKK_AUDIO_STATE_PLAYING) {
        ESP_LOGW(TAG, "Audio is not playing");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Pausing audio playback");
    esp_err_t ret = audio_pipeline_pause(audioMain.pipeline);
    
    if (ret == ESP_OK) {
        audioMain.audio_state = JKK_AUDIO_STATE_PAUSED;
        audioMain.audio_was_paused = true;
    } else {
        ESP_LOGE(TAG, "Failed to pause audio: %s", esp_err_to_name(ret));
        audioMain.audio_state = JKK_AUDIO_STATE_ERROR;
    }
    
    return ret;
}

esp_err_t JkkAudioStop(void) {
    if (audioMain.pipeline == NULL) {
        ESP_LOGE(TAG, "Audio pipeline is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (audioMain.audio_state == JKK_AUDIO_STATE_STOPPED) {
        ESP_LOGW(TAG, "Audio is already stopped");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping audio playback");
    esp_err_t ret = ESP_OK;
    
    ret |= audio_pipeline_stop(audioMain.pipeline);
    ret |= audio_pipeline_wait_for_stop(audioMain.pipeline);
    ret |= audio_pipeline_reset_ringbuffer(audioMain.pipeline);
    ret |= audio_pipeline_reset_elements(audioMain.pipeline);
    ret |= audio_pipeline_reset_items_state(audioMain.pipeline);
    
    if (ret == ESP_OK) {
        audioMain.audio_state = JKK_AUDIO_STATE_STOPPED;
        audioMain.audio_was_paused = false;
    } else {
        ESP_LOGE(TAG, "Failed to stop audio: %s", esp_err_to_name(ret));
        audioMain.audio_state = JKK_AUDIO_STATE_ERROR;
    }
    
    return ret;
}

esp_err_t JkkAudioTogglePlayPause(void) {
    if (audioMain.audio_state == JKK_AUDIO_STATE_PLAYING) {
        return JkkAudioPause();
    } else if (audioMain.audio_state == JKK_AUDIO_STATE_PAUSED || audioMain.audio_state == JKK_AUDIO_STATE_STOPPED) {
        return JkkAudioPlay();
    }
    
    ESP_LOGW(TAG, "Cannot toggle play/pause in current state: %d", audioMain.audio_state);
    return ESP_ERR_INVALID_STATE;
}

esp_err_t JkkAudioRestartStream(void) {
    if(audioMain.pipeline == NULL) {
        ESP_LOGE(TAG, "Audio pipeline is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = JkkAudioStop();
    if (ret == ESP_OK) {
        ret = JkkAudioPlay();
    }
    
    return ret;
}

esp_err_t JkkAudioSetUrl(const char *url, bool out) {
    if((((out ? audioMain.output_type : audioMain.input_type) != 3) && ((out ? audioMain.output_type : audioMain.input_type) != 2)) || (out ? audioMain.output : audioMain.input) == NULL) {
        ESP_LOGE(TAG, "HTTP/FATFS stream is not initialized or not of type HTTP/FATFS");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = audio_element_set_uri((out ? audioMain.output : audioMain.input), url);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set URI for %s stream: %s", (out ? "output" : "input"), esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t JkkAudioEqSetAll(const int *eqGainArray){
    if(eqGainArray == NULL || audioMain.processing == NULL || audioMain.processing_type != 1) {
        ESP_LOGE(TAG, "Equalizer processing element is not initialized or not of type EQUALIZER");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ESP_OK;
    for (int i = 0; i < NUMBER_BAND; i++) {
        ret = equalizer_set_gain_info(audioMain.processing, i, eqGainArray[i], true);
        if(ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set equalizer gain for band %d", i);
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t JkkAudioEqSetInfo(int rate, int ch) {
    if(audioMain.processing == NULL || audioMain.processing_type != 1) {
        ESP_LOGE(TAG, "Equalizer processing element is not initialized or not of type EQUALIZER");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = equalizer_set_info(audioMain.processing, rate, ch);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set equalizer info: %s", esp_err_to_name(ret));
        return ret;
    }
    audioMain.sample_rate = rate;
    audioMain.channels = ch;
    return ret;
}

esp_err_t JkkAudioI2sSetClk(int rate, int bits, int ch, bool out) {
    if((out ? audioMain.output_type : audioMain.input_type) != 1 || (out ? audioMain.output : audioMain.input) == NULL) {
        ESP_LOGE(TAG, "I2S stream is not initialized or not of type I2S");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = i2s_stream_set_clk((out ? audioMain.output : audioMain.input), rate, bits, ch);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set I2S clock: %s", esp_err_to_name(ret));
        return ret;
    }
    if(out) {
        audioMain.sample_rate = rate;
        audioMain.bits = bits;
        audioMain.channels = ch;
    }
    return ret;
}

JkkAudioMain_t *JkkAudioMain_init(int inType, int outType, int processingType, int rawSplitNr) {

    char *inTypeStr[] = {"RAW", "I2S", "FATFS", "HTTP", "A2DP", "BT"};
    char *outTypeStr[] = {"RAW", "I2S", "FATFS", "HTTP", "A2DP", "BT"};
    char *processingTypeStr[] = {"NONE", "EQUALIZER"};

    ESP_LOGI(TAG, "[1.0] Create main pipeline");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audioMain.pipeline = audio_pipeline_init(&pipeline_cfg);

    if( audioMain.pipeline == NULL) {
        ESP_LOGE(TAG, "Failed to create audio pipeline");
        return NULL;
    }

    switch (inType) {
        case 0: {// RAW
            ESP_LOGI(TAG, "[1.1] Create raw stream to read data");
            raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
            raw_cfg.type = AUDIO_STREAM_READER;
            audioMain.input = raw_stream_init(&raw_cfg);
            ESP_LOGI(TAG, "Pointer raw_stream_reader=%p", audioMain.input);
            if (audioMain.input == NULL) {
                ESP_LOGE(TAG, "Failed to create raw stream reader");
                return NULL;
            }
        }
        break;
        case 1: {// I2S
            ESP_LOGI(TAG, "[1.1] Create i2s stream to read data");
            i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
            i2s_cfg.stack_in_ext = true;
            i2s_cfg.task_stack = 4 * 1024 + 512;
            i2s_cfg.type = AUDIO_STREAM_READER;
            audioMain.input = i2s_stream_init(&i2s_cfg);
            ESP_LOGI(TAG, "Pointer i2s_stream_reader=%p", audioMain.input);
            if (audioMain.input == NULL) {
                ESP_LOGE(TAG, "Failed to create i2s stream reader");
                return NULL;
            }
        }
        break;
        case 2: {// FATFS
            ESP_LOGI(TAG, "[1.1] Create fatfs stream to read data");
            fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
            fatfs_cfg.type = AUDIO_STREAM_READER;
            fatfs_cfg.ext_stack = true;
            fatfs_cfg.task_stack = 4 * 1024 + 512;
            audioMain.input = fatfs_stream_init(&fatfs_cfg);
            ESP_LOGI(TAG, "Pointer fatfs_stream_reader=%p", audioMain.input);
            if (audioMain.input == NULL) {
                ESP_LOGE(TAG, "Failed to create fatfs stream reader");
                return NULL;
            }
        }
        break;
        case 3: {// HTTP
            ESP_LOGI(TAG, "[1.1] Create http stream to read data");
            http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
            http_cfg.event_handle = _http_stream_event_handle;
            http_cfg.type = AUDIO_STREAM_READER;
            http_cfg.enable_playlist_parser = true;
            http_cfg.auto_connect_next_track = false;
            http_cfg.stack_in_ext = true;
            http_cfg.task_stack = 7 * 1024;

            http_cfg.user_agent = "RadioJKK32/1.0";
            
            audioMain.input = http_stream_init(&http_cfg);
            ESP_LOGI(TAG, "Pointer http_stream_reader=%p", audioMain.input);
            if (audioMain.input == NULL) {
                ESP_LOGE(TAG, "Failed to create http stream reader");
                return NULL;
            }
        } 
        break;
        default:{
            ESP_LOGE(TAG, "Unknown input type %d", inType);
            return NULL;
        }
    }

    audioMain.input_type = inType;
    audio_pipeline_register(audioMain.pipeline, audioMain.input, inTypeStr[inType]);
    ESP_LOGI(TAG, "[1.1] Register stream to audio pipeline with tag '%s'", inTypeStr[inType]);

    if(inType == 2 || inType == 3) {
        ESP_LOGI(TAG, "[1.2] Create decoder to decode audio data");
        audio_decoder_t auto_decode[] = {
            DEFAULT_ESP_OGG_DECODER_CONFIG(),
            DEFAULT_ESP_MP3_DECODER_CONFIG(),
            DEFAULT_ESP_WAV_DECODER_CONFIG(),
            DEFAULT_ESP_PCM_DECODER_CONFIG(),       
            DEFAULT_ESP_AAC_DECODER_CONFIG(),
            DEFAULT_ESP_FLAC_DECODER_CONFIG(),
            DEFAULT_ESP_AMRNB_DECODER_CONFIG(),
            DEFAULT_ESP_AMRWB_DECODER_CONFIG(),
            DEFAULT_ESP_OPUS_DECODER_CONFIG(),
            DEFAULT_ESP_M4A_DECODER_CONFIG(),
            DEFAULT_ESP_TS_DECODER_CONFIG(),
        };
        esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
        auto_dec_cfg.stack_in_ext = true;
        auto_dec_cfg.task_stack = 4 * 1024 + 512;
        audioMain.decoder = esp_decoder_init(&auto_dec_cfg, auto_decode, sizeof(auto_decode) / sizeof(audio_decoder_t));

        ESP_LOGI(TAG, "Pointer audio_decoder=%p", audioMain.decoder);
        if (audioMain.decoder == NULL) {    
            ESP_LOGE(TAG, "Failed to create audio decoder");
            return NULL;
        }
        audio_pipeline_register(audioMain.pipeline, audioMain.decoder, "DEC");
        ESP_LOGI(TAG, "[1.2] Register decoder to audio pipeline with tag 'DEC'");
    }
    else {
        audioMain.decoder = NULL;
    }

    if(rawSplitNr > 0) {
        ESP_LOGI(TAG, "[1.3] Create raw split to split audio data");
        raw_split_cfg_t rs_cfg = RAW_SPLIT_CFG_DEFAULT();
        rs_cfg.multi_out_num = rawSplitNr;
        rs_cfg.task_prio = 7;
        audioMain.split = raw_split_init(&rs_cfg);
        ESP_LOGI(TAG, "Pointer raw_split=%p", audioMain.split);
        if (audioMain.split == NULL) {
            ESP_LOGE(TAG, "Failed to create raw split");
            return NULL;
        }
        audio_pipeline_register(audioMain.pipeline, audioMain.split, "RS");
        ESP_LOGI(TAG, "[1.3] Register raw split to audio pipeline with tag 'RS'");
        audioMain.raw_split_nr = rawSplitNr;
    }
    else {
        audioMain.split = NULL;
    }
    
    if(processingType == 1) {
        ESP_LOGI(TAG, "[1.4] Create equalizer to process audio data");
        equalizer_cfg_t eq_cfg = DEFAULT_EQUALIZER_CONFIG();
        eq_cfg.task_prio = 7;
        eq_cfg.channel = 2;
        eq_cfg.samplerate = 22050;
        eq_cfg.stack_in_ext = true;
        audioMain.processing = equalizer_init(&eq_cfg);
        ESP_LOGI(TAG, "Pointer equalizer=%p", audioMain.processing);
        if (audioMain.processing == NULL) {
            ESP_LOGE(TAG, "Failed to create equalizer");
            return NULL;
        }
    }
    else {
        audioMain.processing = NULL;
    }

    audioMain.processing_type = processingType;

    if( audioMain.processing != NULL){
        audio_pipeline_register(audioMain.pipeline, audioMain.processing, processingTypeStr[audioMain.processing_type]);
        ESP_LOGI(TAG, "[1.1] Register processing to audio pipeline with tag '%s'",  processingTypeStr[audioMain.processing_type]);
    }

#if defined(CONFIG_JKK_RADIO_USING_I2C_LCD)
    volume_meter_cfg_t vmcfg = V_METER_CFG_DEFAULT();
    vmcfg.update_rate_hz = 14;
    vmcfg.frame_size = 768;
    vmcfg.volume_callback = JkkLcdVolumeIndicatorCallback;
    audioMain.vmeter = volume_meter_init(&vmcfg);
    audio_pipeline_register(audioMain.pipeline, audioMain.vmeter, "VM");
#else
    audioMain.vmeter = NULL;
#endif

    switch ( outType) {
        case 0: {// RAW  
            ESP_LOGI(TAG, "[1.5] Create raw stream to write data");
            raw_stream_cfg_t raw_out_cfg = RAW_STREAM_CFG_DEFAULT();
            raw_out_cfg.type = AUDIO_STREAM_WRITER;
            audioMain.output = raw_stream_init(&raw_out_cfg);
            ESP_LOGI(TAG, "Pointer raw_stream_writer=%p", audioMain.output);
            if (audioMain.output == NULL) {
                ESP_LOGE(TAG, "Failed to create raw stream writer");
                return NULL;
            }
        }
        break;
        case 1: {// I2S
            ESP_LOGI(TAG, "[1.5] Create i2s stream to write data");
            i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
            i2s_cfg.stack_in_ext = true;
            i2s_cfg.task_stack = 4 * 1024;
            i2s_cfg.type = AUDIO_STREAM_WRITER;
            audioMain.output = i2s_stream_init(&i2s_cfg);
            ESP_LOGI(TAG, "Pointer i2s_stream_writer=%p", audioMain.output);
            if (audioMain.output == NULL) {
                ESP_LOGE(TAG, "Failed to create i2s stream writer");
                return NULL;
            }
        }
        break;
        case 2: {// FATFS
            ESP_LOGI(TAG, "[1.5] Create fatfs stream to write data");
            fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
            fatfs_cfg.type = AUDIO_STREAM_WRITER;
            fatfs_cfg.ext_stack = true;
            fatfs_cfg.task_stack = 4 * 1024 + 512;
            audioMain.output = fatfs_stream_init(&fatfs_cfg);
            ESP_LOGI(TAG, "Pointer fatfs_stream_writer=%p", audioMain.output);
            if (audioMain.output == NULL) { 
                ESP_LOGE(TAG, "Failed to create fatfs stream writer");
                return NULL;
            }
        }
        break;
        case 3: {// HTTP
            ESP_LOGI(TAG, "[1.5] Create http stream to write data");
            http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
            http_cfg.type = AUDIO_STREAM_WRITER;
            http_cfg.stack_in_ext = true;
            http_cfg.task_stack = 4 * 1024 + 512;
            audioMain.output = http_stream_init(&http_cfg);
            ESP_LOGI(TAG, "Pointer http_stream_writer=%p", audioMain.output);
            if (audioMain.output == NULL) {
                ESP_LOGE(TAG, "Failed to create http stream writer");
                return NULL;
            }
        }
        break;
        default: {
            ESP_LOGE(TAG, "Unknown output type %d", outType);
            return NULL;
        }
    }

    audioMain.output_type = outType;
    audio_pipeline_register(audioMain.pipeline, audioMain.output, outTypeStr[outType]);
    ESP_LOGI(TAG, "[1.5] Register output to audio pipeline with tag '%s'", outTypeStr[outType]);

    audioMain.linkElementsNoProcessing[0] = inTypeStr[inType];
    audioMain.linkElementsAll[0] = inTypeStr[inType];

    int link_idx_all = 1;
    int link_idx_No_proc = 1;
    if( audioMain.decoder != NULL) {
        audioMain.linkElementsNoProcessing[link_idx_No_proc++] = "DEC";
        audioMain.linkElementsAll[link_idx_all++] = "DEC";    
    }
    if( audioMain.split != NULL) {
        audioMain.linkElementsNoProcessing[link_idx_No_proc++] = "RS";
        audioMain.linkElementsAll[link_idx_all++] = "RS";
    }
    if( audioMain.processing != NULL) {
        audioMain.linkElementsAll[link_idx_all++] = processingTypeStr[audioMain.processing_type];
    }
    if( audioMain.vmeter != NULL) {
        audioMain.linkElementsAll[link_idx_all++] = "VM";
    }

    audioMain.linkElementsAllCount = link_idx_all + 1;

    audioMain.linkElementsNoProcessing[link_idx_No_proc] = outTypeStr[outType]; 
    audioMain.linkElementsAll[link_idx_all] = outTypeStr[outType]; 

    ESP_LOGI(TAG, "Link tags: %s, %s, %s, %s, %s, %s, %s", audioMain.linkElementsAll[0], 
             (link_idx_all > 0) ? audioMain.linkElementsAll[1] : "",
             (link_idx_all > 1) ? audioMain.linkElementsAll[2] : "",
             (link_idx_all > 2) ? audioMain.linkElementsAll[3] : "",
             (link_idx_all > 3) ? audioMain.linkElementsAll[4] : "",
             (link_idx_all > 4) ? audioMain.linkElementsAll[5] : "",
             (link_idx_all > 5) ? audioMain.linkElementsAll[6] : "");

    audio_pipeline_link(audioMain.pipeline, &audioMain.linkElementsAll[0], audioMain.linkElementsAllCount); 

    ESP_LOGI(TAG, "Link tags short: %s, %s, %s, %s, %s, %s", audioMain.linkElementsNoProcessing[0], 
             (link_idx_No_proc > 0) ? audioMain.linkElementsNoProcessing[1] : "",
             (link_idx_No_proc > 1) ? audioMain.linkElementsNoProcessing[2] : "",
             (link_idx_No_proc > 2) ? audioMain.linkElementsNoProcessing[3] : "",
             (link_idx_No_proc > 3) ? audioMain.linkElementsNoProcessing[4] : "",
             (link_idx_No_proc > 4) ? audioMain.linkElementsNoProcessing[5] : "");
    
    ESP_LOGI(TAG, "[1.6] Link elements together: %d", audioMain.linkElementsAllCount);

    audioMain.lineWithProcess = true;

    return &audioMain;
}

esp_err_t JkkAudioMainOnOffProcessing(bool on, audio_event_iface_handle_t evt){
    if(audioMain.lineWithProcess == on) {
        ESP_LOGI(TAG, "Pipeline same state, no change");
        return ESP_OK;
    }
    
    esp_err_t ret = audio_pipeline_stop(audioMain.pipeline);
    ret |= audio_pipeline_wait_for_stop(audioMain.pipeline);
    ret |= audio_pipeline_reset_ringbuffer(audioMain.pipeline);
    ret |= audio_pipeline_reset_elements(audioMain.pipeline);
    ret |= audio_pipeline_reset_items_state(audioMain.pipeline);

    if(ret != ESP_OK) return ret;

    ret = audio_pipeline_unlink(audioMain.pipeline);

    audio_pipeline_remove_listener(audioMain.pipeline);

    if(on) {
        ret |= audio_pipeline_link(audioMain.pipeline, &audioMain.linkElementsAll[0], audioMain.linkElementsAllCount);
    }
    else {
        ret |= audio_pipeline_link(audioMain.pipeline, &audioMain.linkElementsNoProcessing[0], audioMain.linkElementsAllCount - 2);
    }

    audio_pipeline_set_listener(audioMain.pipeline, evt);

   // ret |= audio_pipeline_run(audioMain.pipeline);
    ret |= audio_pipeline_resume(audioMain.pipeline);
    if(ret == ESP_OK) audioMain.lineWithProcess = on;
    return ret;
}

bool JkkAudioMainProcessState(void){
    return audioMain.lineWithProcess;
}

void JkkAudioMain_deinit(void) {
    if (audioMain.pipeline != NULL) {
        audio_pipeline_stop(audioMain.pipeline);
        audio_pipeline_wait_for_stop(audioMain.pipeline);
        audio_pipeline_terminate(audioMain.pipeline);
        ESP_LOGI(TAG, "[1.7] Unregister all elements from audio pipeline");

        audio_pipeline_unregister(audioMain.pipeline, audioMain.input);

        if (audioMain.decoder != NULL) {
            audio_pipeline_unregister(audioMain.pipeline, audioMain.decoder);
        }
        if (audioMain.split != NULL) {
            audio_pipeline_unregister(audioMain.pipeline, audioMain.split);
        }
        if (audioMain.vmeter != NULL) {
            audio_pipeline_unregister(audioMain.pipeline, audioMain.vmeter);
        }
        if (audioMain.processing != NULL) {     
            audio_pipeline_unregister(audioMain.pipeline, audioMain.processing);
        }
        audio_pipeline_unregister(audioMain.pipeline, audioMain.output);

        audio_pipeline_remove_listener(audioMain.pipeline);

        /* Release all resources */
        audio_pipeline_deinit(audioMain.pipeline);
        audioMain.pipeline = NULL;
        ESP_LOGI(TAG, "[1.7] Audio pipeline deinitialized");
    }

    ESP_LOGI(TAG, "[1.8] Deinit all elements");
    if (audioMain.input != NULL) {
        audio_element_deinit(audioMain.input);
        audioMain.input = NULL;
    }
    if (audioMain.decoder != NULL) {
        audio_element_deinit(audioMain.decoder);
        audioMain.decoder = NULL;   
    }
    if (audioMain.split != NULL) {
        audio_element_deinit(audioMain.split);
        audioMain.split = NULL;
    }
    if (audioMain.vmeter != NULL) {
        audio_element_deinit(audioMain.vmeter);
        audioMain.vmeter = NULL;
    }
    if (audioMain.processing != NULL) {
        audio_element_deinit(audioMain.processing);
        audioMain.processing = NULL;
    }
    if (audioMain.output != NULL) {
        audio_element_deinit(audioMain.output);
        audioMain.output = NULL;
    }

    audioMain.input_type = -1;
    audioMain.output_type = -1;
    audioMain.processing_type = -1;
    ESP_LOGI(TAG, "[1.9] Audio main deinitialized");
}
