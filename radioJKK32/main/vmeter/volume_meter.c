#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "volume_meter.h"
#include "audio_mem.h"

static const char *TAG = "VOLUME_METER";

// Stałe dla obliczeń bez float (skalowanie)
#define MAX_VOLUME_LEVEL 100     // Maksymalny poziom głośności
#define MIN_VOLUME_THRESHOLD 10  // Próg ciszy (skalowany)
#define RMS_SHIFT 10            // Przesunięcie bitowe dla obliczeń RMS

// Dane wewnętrzne elementu
typedef struct {
    volume_meter_cfg_t cfg;
    int samples_per_frame;
    int bytes_per_sample;
    int frame_counter;
    int frames_per_update;
    long left_rms_sum;      // Suma kwadratów dla lewego kanału (skalowana)
    long right_rms_sum;     // Suma kwadratów dla prawego kanału (skalowana)
    int accumulation_count;
} volume_meter_t;

// Szybka aproksymacja pierwiastka kwadratowego (metoda Newtona)
static int fast_sqrt(long value) {
    if (value <= 0) return 0;
    
    int x = value;
    int y = (x + 1) / 2;
    
    // 4 iteracje powinny wystarczyć dla naszej dokładności
    for (int i = 0; i < 4; i++) {
        if (y >= x) break;
        x = y;
        y = (x + value / x) / 2;
    }
    
    return x;
}



// Konwersja wartości RMS na poziom 0-100
static int rms_to_level(int rms_value) {
    if (rms_value < MIN_VOLUME_THRESHOLD) {
        return 0; // Cisza
    }
    
    // Logarytmiczna aproksymacja przez lookup table
    // Wartości dostosowane eksperymentalnie dla audio 16-bit
    static const int rms_thresholds[] = {
        50,   150,   380,   800,   1200,  1800,  2500,  3500,
        4800,  6500,  8500,  11000, 14000, 17500, 21500, 26000
    };
    
    int level = 0;
    for (int i = 0; i < 16; i++) {
        if (rms_value >= rms_thresholds[i]) {
            level = (i + 1) * 6; // Każdy próg to +6 jednostek (0-96)
        } else {
            break;
        }
    }
    
    // Interpolacja liniowa między progami dla płynności
    if (level < 96 && level > 0) {
        int threshold_idx = (level / 6) - 1;
        int lower_threshold = rms_thresholds[threshold_idx];
        int upper_threshold = rms_thresholds[threshold_idx + 1];
        
        if (upper_threshold > lower_threshold) {
            int interpolation = ((rms_value - lower_threshold) * 6) / 
                              (upper_threshold - lower_threshold);
            level += interpolation;
        }
    }
    
    // Ograniczamy do zakresu 0-100
    if (level > MAX_VOLUME_LEVEL) level = MAX_VOLUME_LEVEL;
    if (level < 0) level = 0;
    
    return level;
}

// Główna funkcja przetwarzania audio
static int volume_meter_process(audio_element_handle_t self, char *in_buffer, int in_len) {
    int w_size = 0;
    
    int r_size = audio_element_input(self, in_buffer, in_len);

    if (r_size > 0) {
        w_size = audio_element_output(self, in_buffer, r_size);
    } else {
        return r_size;
    }

    volume_meter_t *meter = (volume_meter_t *)audio_element_getdata(self);
    
    // Analizujemy głośność tylko jeśli mamy dane stereo 16-bit
    if (meter->cfg.channels == 2 && meter->cfg.bits_per_sample == 16) {
        int16_t *samples = (int16_t *)in_buffer;
        int sample_count = r_size / (2 * sizeof(int16_t)); // 2 kanały, 16-bit każdy
        
        if (sample_count > 0) {
            // Obliczamy RMS dla każdego kanału bez dodatkowej alokacji pamięci
            long left_sum = 0, right_sum = 0;
            
            for (int i = 0; i < sample_count; i++) {
                // Lewy kanał
                long left_sample = (long)samples[i * 2];
                left_sum += (left_sample * left_sample) >> RMS_SHIFT;
                
                // Prawy kanał
                long right_sample = (long)samples[i * 2 + 1];
                right_sum += (right_sample * right_sample) >> RMS_SHIFT;
            }
            
            // Akumulujemy wartości
            meter->left_rms_sum += left_sum / sample_count;
            meter->right_rms_sum += right_sum / sample_count;
            meter->accumulation_count++;
            
            // Sprawdzamy czy czas na aktualizację
            meter->frame_counter++;
            if (meter->frame_counter >= meter->frames_per_update) {
                // Obliczamy średnie wartości RMS
                int avg_left_rms = fast_sqrt(meter->left_rms_sum / meter->accumulation_count);
                int avg_right_rms = fast_sqrt(meter->right_rms_sum / meter->accumulation_count);
                
                // Konwertujemy na poziomy 0-100
                int left_level = rms_to_level(avg_left_rms);
                int right_level = rms_to_level(avg_right_rms);
                
                // Wywołujemy callback z wynikami
                if (meter->cfg.volume_callback) {
                    meter->cfg.volume_callback(left_level, right_level);
                }
                
                // Resetujemy liczniki
                meter->frame_counter = 0;
                meter->left_rms_sum = 0;
                meter->right_rms_sum = 0;
                meter->accumulation_count = 0;
            }
        }
    }
    
    return w_size;
}

// Funkcja otwierania elementu
static esp_err_t volume_meter_open(audio_element_handle_t self) {
    ESP_LOGI(TAG, "Volume meter element opened");
    return ESP_OK;
}

// Funkcja zamykania elementu
static esp_err_t volume_meter_close(audio_element_handle_t self) {
    ESP_LOGI(TAG, "Volume meter element closed");
    return ESP_OK;
}

// Funkcja niszczenia elementu
static esp_err_t volume_meter_destroy(audio_element_handle_t self) {
    volume_meter_t *meter = (volume_meter_t *)audio_element_getdata(self);
    if (meter) {
        audio_free(meter);
    }
    return ESP_OK;
}

// Funkcja aktualizacji formatu audio
esp_err_t volume_meter_update_format(audio_element_handle_t self, 
                               int sample_rate, 
                               int channels, 
                               int bits_per_sample) {
    if (!self) {
        ESP_LOGE(TAG, "Element handle is null");
        return ESP_ERR_INVALID_ARG;
    }
    
    volume_meter_t *meter = (volume_meter_t *)audio_element_getdata(self);
    if (!meter) {
        ESP_LOGE(TAG, "Volume meter data is null");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Walidacja parametrów
    if (sample_rate < 8000 || sample_rate > 192000) {
        ESP_LOGE(TAG, "Invalid sample rate: %d", sample_rate);
        return false;
    }
    
    if (channels < 1 || channels > 2) {
        ESP_LOGE(TAG, "Invalid channels: %d (supported 1-2)", channels);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (bits_per_sample != 8 && bits_per_sample != 16 && 
        bits_per_sample != 24 && bits_per_sample != 32) {
        ESP_LOGE(TAG, "Invalid bits per sample: %d (supported 8,16,24,32)", bits_per_sample);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Aktualizujemy konfigurację
    meter->cfg.sample_rate = sample_rate;
    meter->cfg.channels = channels;
    meter->cfg.bits_per_sample = bits_per_sample;
    
    // Przeliczamy parametry wewnętrzne
    meter->bytes_per_sample = bits_per_sample / 8;
    meter->samples_per_frame = meter->cfg.frame_size / (meter->bytes_per_sample * channels);
    
    // Przeliczamy częstotliwość aktualizacji
    meter->frames_per_update = sample_rate / (meter->samples_per_frame * meter->cfg.update_rate_hz);
    if (meter->frames_per_update < 1) meter->frames_per_update = 1;
    
    // Resetujemy liczniki
    meter->frame_counter = 0;
    meter->left_rms_sum = 0;
    meter->right_rms_sum = 0;
    meter->accumulation_count = 0;
    
    ESP_LOGI(TAG, "Format updated: %dHz, %d channels, %d bits", 
             sample_rate, channels, bits_per_sample);
    ESP_LOGI(TAG, "New frames per update: %d", meter->frames_per_update);
    
    return ESP_OK;
}

// Funkcja zmiany częstotliwości aktualizacji
esp_err_t volume_meter_set_update_rate(audio_element_handle_t self, int update_rate_hz) {
    if (!self) {
        ESP_LOGE(TAG, "Element handle is null");
        return ESP_ERR_INVALID_ARG;
    }
    
    volume_meter_t *meter = (volume_meter_t *)audio_element_getdata(self);
    if (!meter) {
        ESP_LOGE(TAG, "Volume meter data is null");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (update_rate_hz < 1 || update_rate_hz > 100) {
        ESP_LOGE(TAG, "Invalid update rate: %d (supported 1-100 Hz)", update_rate_hz);
        return ESP_ERR_INVALID_ARG;
    }
    
    meter->cfg.update_rate_hz = update_rate_hz;
    
    // Przeliczamy częstotliwość aktualizacji
    meter->frames_per_update = meter->cfg.sample_rate / (meter->samples_per_frame * update_rate_hz);
    if (meter->frames_per_update < 1) meter->frames_per_update = 1;
    
    // Resetujemy liczniki
    meter->frame_counter = 0;
    meter->left_rms_sum = 0;
    meter->right_rms_sum = 0;
    meter->accumulation_count = 0;
    
    ESP_LOGI(TAG, "Update rate changed to %d Hz, frames per update: %d", 
             update_rate_hz, meter->frames_per_update);
    
    return ESP_OK;
}

// Funkcja tworzenia elementu volume meter
audio_element_handle_t volume_meter_init(volume_meter_cfg_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Config is null");
        return NULL;
    }
    
    volume_meter_t *meter = audio_calloc(1, sizeof(volume_meter_t));
    if (!meter) {
        ESP_LOGE(TAG, "Failed to allocate memory for volume meter");
        return NULL;
    }
    // Kopiujemy konfigurację
    memcpy(&meter->cfg, config, sizeof(volume_meter_cfg_t));  
    // Obliczamy parametry przetwarzania
    meter->bytes_per_sample = config->bits_per_sample / 8;
    meter->samples_per_frame = config->frame_size / (meter->bytes_per_sample * config->channels);
    
    // Obliczamy ile framów potrzebujemy do osiągnięcia żądanej częstotliwości aktualizacji
    meter->frames_per_update = config->sample_rate / (meter->samples_per_frame * config->update_rate_hz);
    if (meter->frames_per_update < 1) meter->frames_per_update = 1;
    
    ESP_LOGI(TAG, "Volume meter initialized: %dHz, %d channels, %d bits, update rate: %dHz", 
             config->sample_rate, config->channels, config->bits_per_sample, config->update_rate_hz);
    ESP_LOGI(TAG, "Frames per update: %d", meter->frames_per_update);
    
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = volume_meter_open;
    cfg.close = volume_meter_close;
    cfg.process = volume_meter_process;
    cfg.destroy = volume_meter_destroy;
    cfg.buffer_len = 1 * 1024;
    cfg.tag = "v_meter";
    cfg.task_stack = 3 * 1024;
    cfg.task_prio = 4;
    cfg.task_core = 0;
    cfg.stack_in_ext = true;
    cfg.out_rb_size = 0;

    audio_element_handle_t el = audio_element_init(&cfg);
    if (!el) {
        ESP_LOGE(TAG, "Failed to create volume meter element");
        audio_free(meter);
        return NULL;
    }
    
    audio_element_setdata(el, meter);
    audio_element_info_t info = {0};
    audio_element_setinfo(el, &info);
    return el;
}

// Przykład callback funkcji
static void example_volume_callback(int left_volume, int right_volume) {
    ESP_LOGI(TAG, "Volume levels: L=%d%% R=%d%%", left_volume, right_volume);
    
    // Tutaj możesz zaktualizować swoje słupki głośności
    // update_volume_bars(left_volume, right_volume);
}

// Uproszczona funkcja tworzenia z domyślnymi parametrami
audio_element_handle_t create_volume_meter(int sample_rate, int update_rate, 
                                         void (*callback)(int left, int right)) {
    volume_meter_cfg_t cfg = {
        .sample_rate = sample_rate,
        .channels = 2,                    // Stereo
        .bits_per_sample = 16,           // 16-bit
        .frame_size = 1024,              // Rozmiar bufora w bajtach
        .update_rate_hz = update_rate,   // Częstotliwość aktualizacji (10-30 Hz)
        .volume_callback = callback ? callback : example_volume_callback
    };
    
    return volume_meter_init(&cfg);
}