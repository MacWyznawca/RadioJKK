/*
 * Raw multiout split stream
 * @author      Jaromir Kopp
 * @date        2025
 * 
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "esp_log.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "audio_element.h"
#include "raw_split.h"
#include "audio_type_def.h"
static const char *TAG = "SPLITTER";

#define BUF_SIZE (3 * 1024)

typedef struct raw_split {
    unsigned char *buf;
} raw_split_t;

static esp_err_t raw_split_destroy(audio_element_handle_t self){
    raw_split_t *raw_split = (raw_split_t *)audio_element_getdata(self);
    audio_free(raw_split);
    return ESP_OK;
}

static esp_err_t raw_split_open(audio_element_handle_t self){

    ESP_LOGD(TAG, "raw_split_open");

    raw_split_t *raw_split = (raw_split_t *)audio_element_getdata(self);

    raw_split->buf = (unsigned char *)audio_calloc(1, BUF_SIZE);
    if (raw_split->buf == NULL) {
        ESP_LOGE(TAG, "calloc buffer failed. (line %d)", __LINE__);
        return ESP_ERR_NO_MEM;
    }
    memset(raw_split->buf, 0, BUF_SIZE);
    return ESP_OK;
}

static esp_err_t raw_split_close(audio_element_handle_t self){
    ESP_LOGD(TAG, "raw_split_close");
    raw_split_t *raw_split = (raw_split_t *)audio_element_getdata(self);
    if (raw_split->buf) {
        audio_free(raw_split->buf);
        raw_split->buf = NULL;
    }
    return ESP_OK;
}

static int raw_split_process(audio_element_handle_t self, char *in_buffer, int in_len){
    raw_split_t *raw_split = (raw_split_t *)audio_element_getdata(self);
    int ret = 0;
    
    int r_size = audio_element_input(self, (char *)raw_split->buf, BUF_SIZE);

    if (r_size > 0) {
        audio_element_multi_output(self, (char *)raw_split->buf, r_size, 0);
        ret = audio_element_output(self, (char *)raw_split->buf, r_size);
    } else {
        ret = r_size;
    }
    return ret;
}

audio_element_handle_t raw_split_init(raw_split_cfg_t *config){
    if (config == NULL) {
        ESP_LOGE(TAG, "config is NULL. (line %d)", __LINE__);
        return NULL;
    }
    raw_split_t *raw_split = audio_calloc(1, sizeof(raw_split_t));
    AUDIO_MEM_CHECK(TAG, raw_split, return NULL);
    if (raw_split == NULL) {
        ESP_LOGE(TAG, "audio_calloc failed for raw_split. (line %d)", __LINE__);
        return NULL;
    }
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.destroy = raw_split_destroy;
    cfg.process = raw_split_process;
    cfg.open = raw_split_open;
    cfg.close = raw_split_close;
    cfg.buffer_len = 0;
    cfg.multi_out_rb_num = config->multi_out_num;
    cfg.tag = "rsplit";
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.out_rb_size = config->out_rb_size;
    cfg.stack_in_ext = config->stack_in_ext;
    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, {audio_free(raw_split); return NULL;});
    raw_split->buf = NULL;
    audio_element_setdata(el, raw_split);
    audio_element_info_t info = {0};
    audio_element_setinfo(el, &info);
    ESP_LOGD(TAG, "raw_split_init");
    return el;
}
