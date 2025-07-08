/*
 * ESP-ADF Audio Element
 * Dynamic volume calculator (for display)
 * @author      Jaromir Kopp 
 * @date        2025
*/

#ifndef _VOLUME_METER_H_
#define _VOLUME_METER_H_

#include "audio_element.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration structure of volume meter element
 */
typedef struct {
    int sample_rate;           ///< Sample rate (e.g. 44100)
    int channels;              ///< No. of channels (2 for stereo).
    int bits_per_sample;       ///< Bits per sample (16). Currently, only 16 bits is supported
    int frame_size;            ///< Buffer size in bytes
    int update_rate_hz;        ///< Update frequency (10-30 Hz)
    void (*volume_callback)(int left_volume, int right_volume); ///< Callback function with results (values 0-100)
} volume_meter_cfg_t;

#define V_METER_CFG_DEFAULT() {\
    .sample_rate = 44100,    \
    .channels = 2,           \
    .bits_per_sample = 16,   \
    .frame_size = 1024,      \
    .update_rate_hz = 15,    \
    .volume_callback = NULL, \
}

/**
 * @brief Initializes the volume meter element.
 *
 * @param config Element configuration.
 * @return audio_element_handle_t Handle to element or NULL in case of error.
 */
audio_element_handle_t volume_meter_init(volume_meter_cfg_t *config);

/**
 * @brief Creates volume meter element with default configuration.
 *
 * @param sample_rate Sampling frequency.
 * @param update_rate Update frequency in Hz.
 * @param callback Callback function for the results.
 * * @return audio_element_handle_t Handle to element or NULL in case of error.
 */
audio_element_handle_t create_volume_meter(int sample_rate, int update_rate, 
                                         void (*callback)(int left, int right));

/**
 * @brief Updates the audio parameters of the volume meter element.
 *
 * @param self Handle to volume meter element.
 * @param sample_rate New sample rate.
 * @param channels New number of channels (1=mono, 2=stereo). 
 * @param bits_per_sample New number of bits per sample (8, 16, 24, 32). Currently, only 16 bits is supported
 * @return true if update succeeded, false if error.
 */
esp_err_t volume_meter_update_format(audio_element_handle_t self, 
                               int sample_rate, 
                               int channels, 
                               int bits_per_sample);

/**
 * @brief Changes the frequency of callback updates.
 *
 * @param self Handle to volume meter element.
 * @param update_rate_hz New update frequency in Hz (1-100).
 * @return true if update succeeded, false if error.
 */
esp_err_t volume_meter_set_update_rate(audio_element_handle_t self, int update_rate_hz);

#ifdef __cplusplus
}
#endif

/**
 * Examole of use:
 * 
 *  volume_meter_cfg_t vmcfg = V_METER_CFG_DEFAULT();
 *  vmcfg.update_rate_hz = 14;
 *  vmcfg.frame_size = 768;
 *  vmcfg.volume_callback = JkkLcdVolumeIndicatorCallback;
 *  audioMain.vmeter = volume_meter_init(&vmcfg);
 *  audio_pipeline_register(audioMain.pipeline, audioMain.vmeter, "VM");
 * 
 * void JkkLcdVolumeIndicatorCallback(int left_volume, int right_volume) {
 *      line_points[1].x = 66 + (right_volume * 60 / 100);
 *      line_points[0].x = 60 - ((left_volume * 60) / 100);
 *      if(JkkLcdPortLock(0)){
 *          lv_obj_invalidate(line);
 *          JkkLcdPortUnlock();
 *      }
 *  }
 * 
 */

#endif /* _VOLUME_METER_H_ */