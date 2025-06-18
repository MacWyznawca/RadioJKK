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


#ifndef _RAW_SPLIT_H_
#define _RAW_SPLIT_H_

#include "audio_error.h"
#include "audio_element.h"
#include "audio_common.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Raw stream provides APIs to obtain the pipeline data without output stream or
 *        fill the pipeline data without input stream.
 *        The stream has two types / modes, reader and writer:
 *
 *        - AUDIO_STREAM_READER, e.g. [i2s]->[filter]->[raw],[i2s]->[codec-amr]->[raw]
 *        - AUDIO_STREAM_WRITER, e.g. [raw]->[codec-mp3]->[i2s]
 */

/**
 * Raw split configurations
 */
typedef struct {
    int                     out_rb_size;        /*!< Size of output ringbuffer */
    int                     task_stack;         /*!< Task stack size */
    int                     task_core;          /*!< Task running in core (0 or 1) */
    int                     task_prio;          /*!< Task priority (based on freeRTOS priority) */
    bool                    stack_in_ext;       /*!< Try to allocate stack in external memory */
    int                     multi_out_num;      /*!< The number of multiple output */
} raw_split_cfg_t;

#define RAW_OUT_TASK_STACK           (3 * 1024)
#define RAW_OUT_TASK_PRIO            (5)
#define RAW_OUT_TASK_CORE            (0)
#define RAW_OUT_RINGBUFFER_SIZE      (3 * 1024)

#define RAW_SPLIT_CFG_DEFAULT() {\
    .out_rb_size = RAW_OUT_RINGBUFFER_SIZE, \
    .task_stack = RAW_OUT_TASK_STACK,                                        \
    .task_core = RAW_OUT_TASK_CORE,                                          \
    .task_prio = RAW_OUT_TASK_PRIO,                                          \
    .stack_in_ext = true,                                                      \
    .multi_out_num = 0, \
}

/**
 * @brief      Initialize RAW split
 *
 * @param      cfg   The RAW split configuration - use RAW_SPLIT_CFG_DEFAULT() to simple initialize
 *
 * @return     The audio element handle
 */
audio_element_handle_t raw_split_init(raw_split_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif
