/*
 * Raw multiout split stream
 * @author      Jaromir Kopp
 * @date        2025
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

static esp_err_t raw_split_destroy(audio_element_handle_t self){
    ESP_LOGI(TAG, "raw_split_destroy");
    return ESP_OK;
}

static esp_err_t raw_split_open(audio_element_handle_t self){
    ESP_LOGI(TAG, "raw_split_open");
    return ESP_OK;
}

static esp_err_t raw_split_close(audio_element_handle_t self){
    ESP_LOGI(TAG, "raw_split_close");
    return ESP_OK;
}

static int raw_split_process(audio_element_handle_t self, char *in_buffer, int in_len){
    int ret = 0;
    
    int r_size = audio_element_input(self, in_buffer, in_len);


    if (r_size > 0) {
        audio_element_multi_output(self, in_buffer, r_size, 0);
        ret = audio_element_output(self, in_buffer, r_size);
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

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.destroy = raw_split_destroy;
    cfg.process = raw_split_process;
    cfg.open = raw_split_open;
    cfg.close = raw_split_close;
    cfg.buffer_len = BUF_SIZE;
    cfg.multi_out_rb_num = config->multi_out_num;
    cfg.tag = "rsplit";
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.out_rb_size = config->out_rb_size;
    cfg.stack_in_ext = config->stack_in_ext;
    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, {return NULL;});
    audio_element_info_t info = {0};
    audio_element_setinfo(el, &info);
    ESP_LOGD(TAG, "raw_split_init");
    return el;
}
