/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * Audio write to SD Pipeline and elements
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
#include "fatfs_stream.h"
#include "periph_sdcard.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "aac_encoder.h"
#include "wav_encoder.h"
#include "audio_common.h"

#include "jkk_audio_sdwrite.h"

#define DEFAULT_AAC_BITRATE (80 * 1024) // Default bitrate for AAC encoder

static const char *TAG = "A_SD";

static EXT_RAM_BSS_ATTR JkkAudioSdWrite_t audioSd = {0}; // EXT_RAM_BSS_ATTR

esp_err_t JkkAudioSdWriteResChange(int sample_rate, int channels, int bits) {
    if(audioSd.resample == NULL) {
        ESP_LOGE(TAG, "Resample filter is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if(audioSd.encoder_type != 1){
        ESP_LOGW(TAG, "Resample filter can only be changed when encoder type is AAC");
        return ESP_OK; // No resample change needed for non-AAC encoders
    }
    esp_err_t ret = rsp_filter_change_src_info(audioSd.resample, sample_rate, channels, bits);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to change resample filter source info: %s", esp_err_to_name(ret));
        return ret; 
    }
    audioSd.sample_rate = sample_rate;
    audioSd.channels = channels;
    audioSd.bits = bits;
    ESP_LOGI(TAG, "Resample filter source info changed to sample_rate=%d, channels=%d, bits=%d", sample_rate, channels, bits);

    return ESP_OK;
}

esp_err_t JkkAudioSdWriteSetUri(const char *uri) {
    if(audioSd.fatfs_wr == NULL || uri == NULL) {
        ESP_LOGE(TAG, "Fatfs stream writer is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = audio_element_set_uri(audioSd.fatfs_wr, uri);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set URI for fatfs stream writer: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Fatfs stream writer URI set to %s", uri);
    return ESP_OK;
}

void JkkAudioSdWriteStopStream(void) {
    if(audioSd.pipeline == NULL) {
        ESP_LOGE(TAG, "Audio pipeline is not initialized");
        return ;
    }

    audio_pipeline_stop(audioSd.pipeline);
    audio_pipeline_wait_for_stop(audioSd.pipeline);
    audio_pipeline_reset_ringbuffer(audioSd.pipeline);
    audio_pipeline_reset_elements(audioSd.pipeline);
    audio_pipeline_reset_items_state(audioSd.pipeline);

    audioSd.is_recording = false;
}

esp_err_t JkkAudioSdWriteStartStream(const char *uri) {
    if(audioSd.pipeline == NULL) {
        ESP_LOGE(TAG, "Audio pipeline is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ESP_OK;

    if(uri){
        ret = JkkAudioSdWriteSetUri(uri);
         if(ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set sd write uri: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    audio_pipeline_reset_ringbuffer(audioSd.pipeline);
    audio_pipeline_reset_elements(audioSd.pipeline);
    audio_pipeline_reset_items_state(audioSd.pipeline);

    ret = audio_pipeline_run(audioSd.pipeline);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to run audio pipeline: %s", esp_err_to_name(ret));
        return ret;
    }
    audioSd.is_recording = true;

    ESP_LOGI(TAG, "Audio pipeline restarted successfully");
    return ESP_OK;
}

esp_err_t JkkAudioSdWriteRestartStream(bool withElements) {
    if(audioSd.pipeline == NULL) {
        ESP_LOGE(TAG, "Audio pipeline is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ESP_OK;
    audio_pipeline_stop(audioSd.pipeline);
    audio_pipeline_wait_for_stop(audioSd.pipeline);
    audio_pipeline_reset_ringbuffer(audioSd.pipeline);
    audio_pipeline_reset_elements(audioSd.pipeline);
    audio_pipeline_reset_items_state(audioSd.pipeline);

    ret = audio_pipeline_run(audioSd.pipeline);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to run audio pipeline: %s", esp_err_to_name(ret));
        return ret;
    }
    audioSd.is_recording = true;

    ESP_LOGI(TAG, "Audio pipeline restarted successfully");
    return ESP_OK;
}

JkkAudioSdWrite_t *JkkAudioSdWrite_init(int encoder_type, int sample_rate, int channels) {
    audioSd.encoder_type = encoder_type;
    audioSd.sample_rate = sample_rate;
    audioSd.channels = channels;

    ESP_LOGI(TAG, "[0.1] Create jkkRadio.audioMain->pipeline_save");
    audio_pipeline_cfg_t pipeline_save_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audioSd.pipeline = audio_pipeline_init(&pipeline_save_cfg);
    if(audioSd.pipeline == NULL) {
        ESP_LOGE(TAG, "Failed to create audio pipeline");
        return NULL;
    }
    ESP_LOGI(TAG, "[0.2] Create jkkRadio.audioMain->raw_read stream to read data");
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    audioSd.raw_read = raw_stream_init(&raw_cfg);
    if(audioSd.raw_read == NULL) {
        ESP_LOGE(TAG, "Failed to create raw stream reader");
        return NULL;
    }   
    ESP_LOGI(TAG, "Pointer raw_stream_reader=%p", audioSd.raw_read);
    
    if(audioSd.encoder_type == 1) {
        ESP_LOGI(TAG, "[0.3] Create jkkRadio.audioMain->filter_resample_write to resample data");
        rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
        rsp_cfg.src_rate = sample_rate;
        rsp_cfg.src_ch = channels;
        rsp_cfg.dest_rate = sample_rate;
        rsp_cfg.dest_ch = channels;
        rsp_cfg.mode = RESAMPLE_ENCODE_MODE;
        rsp_cfg.task_core = 1; // Use core 1 for resampling
        audioSd.resample = rsp_filter_init(&rsp_cfg);
        if(audioSd.resample == NULL) {
            ESP_LOGE(TAG, "Failed to create resample filter");
            return NULL;
        }
        audioSd.needResample = true;
    } else {
        audioSd.resample = NULL;
    }   
    ESP_LOGI(TAG, "Pointer filter_resample_write=%p", audioSd.resample);
    if(encoder_type == 1) { // AAC
        ESP_LOGI(TAG, "[0.4] Create jkkRadio.audioMain->aac_encoder to encode data");
        aac_encoder_cfg_t wav_cfg = DEFAULT_AAC_ENCODER_CONFIG();
        wav_cfg.sample_rate = sample_rate;
        wav_cfg.bitrate = DEFAULT_AAC_BITRATE;
        wav_cfg.task_core = 1;
        audioSd.encoder = aac_encoder_init(&wav_cfg);
        if(audioSd.encoder == NULL) {
            ESP_LOGE(TAG, "Failed to create AAC encoder");
            return NULL;
        }
    } else if(encoder_type == 2) { // WAV
        ESP_LOGI(TAG, "[0.4] Create jkkRadio.audioMain->wav_encoder to encode data");
        wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
        wav_cfg.task_core = 1;
        audioSd.encoder = wav_encoder_init(&wav_cfg);
        if(audioSd.encoder == NULL) {
            ESP_LOGE(TAG, "Failed to create WAV encoder");
            return NULL;
        }
    } else {
        audioSd.encoder = NULL;
    }
    ESP_LOGI(TAG, "Pointer aac_encoder=%p", audioSd.encoder);
    ESP_LOGI(TAG, "[0.5] Create jkkRadio.audioMain->fatfs_wr_stream to write data");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_WRITER;
    fatfs_cfg.task_core = 1;
    fatfs_cfg.ext_stack = true;
    fatfs_cfg.task_stack = 4 * 1024 + 512;
    audioSd.fatfs_wr = fatfs_stream_init(&fatfs_cfg);
    if(audioSd.fatfs_wr == NULL) {
        ESP_LOGE(TAG, "Failed to create fatfs stream writer");
        return NULL;
    }
    ESP_LOGI(TAG, "Pointer fatfs_stream_writer=%p", audioSd.fatfs_wr);
    ESP_LOGI(TAG, "[0.6] Register all elements to pipeline_save");

    audio_pipeline_register(audioSd.pipeline, audioSd.raw_read, "RAW");

    int elCount = 2; // Always: RAW and FILE
    if(audioSd.resample) {
        audio_pipeline_register(audioSd.pipeline, audioSd.resample, "RESMAPLE");
        elCount++;
    }
    if(audioSd.encoder) {
        audio_pipeline_register(audioSd.pipeline, audioSd.encoder, "ENCODE");
        elCount++;
    }
    audio_pipeline_register(audioSd.pipeline, audioSd.fatfs_wr, "FILE");    

    const char *link_tag[elCount]; 
    link_tag[0] = "RAW";

    int link_idx = 1;
    if( audioSd.resample != NULL) {
        link_tag[link_idx++] = "RESMAPLE";
    }
    if( audioSd.encoder != NULL) {
        link_tag[link_idx++] = "ENCODE";
    }
    link_tag[link_idx] = "FILE"; 

    audio_pipeline_link(audioSd.pipeline, &link_tag[0], elCount);
    ESP_LOGI(TAG, "Link tags: %s, %s, %s, %s", link_tag[0], 
             (link_idx > 0) ? link_tag[1] : "",
             (link_idx > 1) ? link_tag[2] : "",
             (link_idx > 2) ? link_tag[3] : ""); 

    return &audioSd;
}

void JkkAudioSdWrite_deinit(void) {
    if(audioSd.pipeline) {
        audio_pipeline_stop(audioSd.pipeline);
        audio_pipeline_wait_for_stop(audioSd.pipeline);
        audio_pipeline_terminate(audioSd.pipeline);
        if(audioSd.raw_read) {
            audio_pipeline_unregister(audioSd.pipeline, audioSd.raw_read);
        }
        if(audioSd.resample) {
            audio_pipeline_unregister(audioSd.pipeline, audioSd.resample);
        }
        if(audioSd.encoder) {
            audio_pipeline_unregister(audioSd.pipeline, audioSd.encoder);
        }
        if(audioSd.fatfs_wr) {
            audio_pipeline_unregister(audioSd.pipeline, audioSd.fatfs_wr);
        }
        audio_pipeline_deinit(audioSd.pipeline);
        audioSd.pipeline = NULL;
    }
    if(audioSd.raw_read) {
        audio_element_deinit(audioSd.raw_read);
        audioSd.raw_read = NULL;
    }
    if(audioSd.resample) {
        audio_element_deinit(audioSd.resample);
        audioSd.resample = NULL;
    }
    if(audioSd.encoder) {
        audio_element_deinit(audioSd.encoder);
        audioSd.encoder = NULL;
    }
    if(audioSd.fatfs_wr) {
        audio_element_deinit(audioSd.fatfs_wr);
        audioSd.fatfs_wr = NULL;
    }
}