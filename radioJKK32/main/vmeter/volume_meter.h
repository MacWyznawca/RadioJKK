#ifndef _VOLUME_METER_H_
#define _VOLUME_METER_H_

#include "audio_element.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Struktura konfiguracji elementu volume meter
 */
typedef struct {
    int sample_rate;           ///< Częstotliwość próbkowania (np. 44100)
    int channels;              ///< Liczba kanałów (2 dla stereo)
    int bits_per_sample;       ///< Bity na próbkę (16)
    int frame_size;            ///< Rozmiar bufora w bajtach
    int update_rate_hz;        ///< Częstotliwość aktualizacji (10-30 Hz)
    void (*volume_callback)(int left_volume, int right_volume); ///< Callback z wynikami (0-100)
} volume_meter_cfg_t;

#define V_METER_CFG_DEFAULT() {\
    .sample_rate = 44100, \
    .channels = 2,                                        \
    .bits_per_sample = 16,                                          \
    .frame_size = 2048,                                          \
    .update_rate_hz = 15,                                                      \
    .volume_callback = NULL, \
}

/**
 * @brief Inicjalizuje element volume meter
 * 
 * @param config Konfiguracja elementu
 * @return audio_element_handle_t Handle do elementu lub NULL w przypadku błędu
 */
audio_element_handle_t volume_meter_init(volume_meter_cfg_t *config);

/**
 * @brief Tworzy element volume meter z domyślną konfiguracją
 * 
 * @param sample_rate Częstotliwość próbkowania
 * @param update_rate Częstotliwość aktualizacji w Hz
 * @param callback Funkcja callback dla wyników
 * @return audio_element_handle_t Handle do elementu lub NULL w przypadku błędu
 */
audio_element_handle_t create_volume_meter(int sample_rate, int update_rate, 
                                         void (*callback)(int left, int right));

/**
 * @brief Aktualizuje parametry audio elementu volume meter
 * 
 * @param self Handle do elementu volume meter
 * @param sample_rate Nowa częstotliwość próbkowania
 * @param channels Nowa liczba kanałów (1=mono, 2=stereo)
 * @param bits_per_sample Nowa liczba bitów na próbkę (8, 16, 24, 32)
 * @return true jeśli aktualizacja się powiodła, false w przypadku błędu
 */
esp_err_t volume_meter_update_format(audio_element_handle_t self, 
                               int sample_rate, 
                               int channels, 
                               int bits_per_sample);

/**
 * @brief Zmienia częstotliwość aktualizacji callback'a
 * 
 * @param self Handle do elementu volume meter
 * @param update_rate_hz Nowa częstotliwość aktualizacji w Hz (1-100)
 * @return true jeśli aktualizacja się powiodła, false w przypadku błędu
 */
esp_err_t volume_meter_set_update_rate(audio_element_handle_t self, int update_rate_hz);

#ifdef __cplusplus
}
#endif

#endif /* _VOLUME_METER_H_ */