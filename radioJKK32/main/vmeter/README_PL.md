# ESP-ADF Volume Meter Element

[![ESP-ADF Version](https://img.shields.io/badge/ESP--ADF-v2.7+-blue.svg)](https://docs.espressif.com/projects/esp-adf/en/latest/)

Wydajny element audio dla ESP-ADF, który zapewnia monitorowanie poziomu głośności w czasie rzeczywistym z konfigurowalnymi callbackami. Idealny do tworzenia wskaźników głośności, mierników VU lub wizualizacji poziomu audio w projektach ESP32.

## Funkcje

- 🎚️ **Monitorowanie głośności w czasie rzeczywistym** - Oblicza wartości RMS dla obu kanałów stereo
- 🔧 **Konfigurowalne częstotliwości aktualizacji** - 1-100 Hz (zalecana 10-30 Hz) częstotliwość wywołań callback
- 📊 **Skalowanie logarytmiczne** - Zapewnia percepcyjnie dokładne poziomy 0-100%
- 🎯 **Niskie obciążenie procesora** - Zoptymalizowany szybki pierwiastek kwadratowy i wydajne obliczenia RMS
- 🔀 **Wsparcie stereo i mono** - Działa z strumieniami audio mono i stereo
- 📱 **Łatwa integracja** - Prosty API oparty na callback do aktualizacji interfejsu
- 🎵 **Różne częstotliwości próbkowania** - Obsługuje 8kHz do 192kHz
- 💾 **Wydajność pamięciowa** - Minimalny ślad pamięciowy bez dynamicznych alokacji podczas przetwarzania

## Kompatybilność

- **ESP-ADF**: v2.7 lub nowszy
- **ESP-IDF**: Kompatybilny z wersjami obsługiwanymi przez ESP-ADF
- **Formaty audio**: 16-bit PCM (wsparcie dla 8, 24, 32-bit w nagłówku, ale obecnie zaimplementowane tylko 16-bit)
- **Częstotliwości próbkowania**: 8kHz - 192kHz
- **Kanały**: Mono i Stereo

## Instalacja

1. Skopiuj `volume_meter.c` i `volume_meter.h` do swojego projektu ESP-ADF
2. Dodaj plik źródłowy do CMakeLists.txt lub konfiguracji komponentu
3. Dołącz nagłówek w głównym pliku aplikacji

## Szybki Start

### Podstawowe użycie

```c
#include "volume_meter.h"

// Funkcja callback do obsługi aktualizacji głośności
void my_volume_callback(int left_volume, int right_volume) {
    printf("Głośność: L=%d%% R=%d%%\n", left_volume, right_volume);
    // Tutaj aktualizuj paski głośności, LED-y lub wyświetlacz
}

// Utwórz miernik głośności z domyślnymi ustawieniami
audio_element_handle_t volume_meter = create_volume_meter(44100, 15, my_volume_callback);

// Dodaj do pipeline audio
audio_pipeline_register(pipeline, volume_meter, "volume_meter");
```

### Konfiguracja zaawansowana

```c
// Własna konfiguracja
volume_meter_cfg_t config = {
    .sample_rate = 44100,
    .channels = 2,
    .bits_per_sample = 16,
    .frame_size = 1024,
    .update_rate_hz = 20,
    .volume_callback = my_volume_callback
};

audio_element_handle_t volume_meter = volume_meter_init(&config);
```

### Użycie z domyślnym makrem konfiguracji

```c
volume_meter_cfg_t config = V_METER_CFG_DEFAULT();
config.update_rate_hz = 25;
config.volume_callback = my_volume_callback;

audio_element_handle_t volume_meter = volume_meter_init(&config);
```

## Dokumentacja API

### Struktura konfiguracji

```c
typedef struct {
    int sample_rate;           // Częstotliwość próbkowania (np. 44100)
    int channels;              // Liczba kanałów (1=mono, 2=stereo)
    int bits_per_sample;       // Bity na próbkę (obecnie tylko 16-bit obsługiwane)
    int frame_size;            // Rozmiar bufora w bajtach
    int update_rate_hz;        // Częstotliwość aktualizacji (1-100 Hz)
    void (*volume_callback)(int left_volume, int right_volume); // Funkcja callback
} volume_meter_cfg_t;
```

### Funkcje

#### `volume_meter_init(volume_meter_cfg_t *config)`
Inicjalizuje element miernika głośności z własną konfiguracją.

**Parametry:**
- `config`: Wskaźnik na strukturę konfiguracji

**Zwraca:** `audio_element_handle_t` lub `NULL` w przypadku błędu

#### `create_volume_meter(int sample_rate, int update_rate, void (*callback)(int, int))`
Tworzy miernik głośności z domyślnymi ustawieniami i podanymi parametrami.

**Parametry:**
- `sample_rate`: Częstotliwość próbkowania audio (8000-192000 Hz)
- `update_rate`: Częstotliwość callback (1-100 Hz)
- `callback`: Funkcja callback aktualizacji głośności

**Zwraca:** `audio_element_handle_t` lub `NULL` w przypadku błędu

#### `volume_meter_update_format(audio_element_handle_t self, int sample_rate, int channels, int bits_per_sample)`
Aktualizuje parametry formatu audio w czasie działania.

**Parametry:**
- `self`: Uchwyt elementu miernika głośności
- `sample_rate`: Nowa częstotliwość próbkowania
- `channels`: Nowa liczba kanałów (1-2)
- `bits_per_sample`: Nowa głębia bitowa (8, 16, 24, 32)

**Zwraca:** `ESP_OK` w przypadku sukcesu, kod błędu w przypadku niepowodzenia

#### `volume_meter_set_update_rate(audio_element_handle_t self, int update_rate_hz)`
Zmienia częstotliwość aktualizacji callback.

**Parametry:**
- `self`: Uchwyt elementu miernika głośności
- `update_rate_hz`: Nowa częstotliwość aktualizacji (1-100 Hz)

**Zwraca:** `ESP_OK` w przypadku sukcesu, kod błędu w przypadku niepowodzenia

## Przykład implementacji

Oto kompletny przykład pokazujący integrację z LVGL dla wizualnego miernika głośności:

```c
#include "volume_meter.h"
#include "lvgl.h"

lv_obj_t *volume_bar_left;
lv_obj_t *volume_bar_right;

void ui_volume_callback(int left_volume, int right_volume) {
    // Aktualizuj paski głośności LVGL
    lv_bar_set_value(volume_bar_left, left_volume, LV_ANIM_OFF);
    lv_bar_set_value(volume_bar_right, right_volume, LV_ANIM_OFF);
}

void setup_audio_pipeline() {
    // Utwórz pipeline
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);
    
    // Utwórz miernik głośności
    audio_element_handle_t volume_meter = create_volume_meter(44100, 20, ui_volume_callback);
    
    // Utwórz inne elementy audio (dekoder, itp.)
    // ...
    
    // Zarejestruj elementy
    audio_pipeline_register(pipeline, volume_meter, "volume_meter");
    
    // Połącz pipeline: [decoder] -> [volume_meter] -> [output]
    const char *link_tag[3] = {"decoder", "volume_meter", "output"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
    
    // Uruchom pipeline
    audio_pipeline_run(pipeline);
}
```

## Szczegóły techniczne

### Obliczanie głośności
- Używa obliczeń RMS (Root Mean Square) dla dokładnego pomiaru głośności
- Aproksymacja skalowania logarytmicznego przy użyciu tablic lookup dla percepcyjnie poprawnych poziomów
- Szybka implementacja pierwiastka kwadratowego zoptymalizowana dla ESP32
- Konfigurowalny próg ciszy i maksymalny limit głośności

### Charakterystyki wydajnościowe
- **Użycie CPU**: ~1-2% przy 44,1 kHz, 15 Hz częstotliwości aktualizacji
- **Pamięć**: ~3-5 KB zużycia RAM (włączając stos zadania)
- **Opóźnienie**: Minimalne opóźnienie przetwarzania (projekt pass-through)
- **Dokładność**: ±1% dokładności poziomu głośności

### Algorytm skalowania
Element używa aproksymacji logarytmicznej z 16 punktami progowymi do konwersji wartości RMS na percepcyjnie dokładne poziomy 0-100%. Interpolacja liniowa między progami zapewnia płynne przejścia poziomów.

## Przypadki użycia

- **Wizualizatory audio**: Tworzenie analizatorów spektrum i mierników VU
- **Aplikacje nagrywające**: Monitorowanie poziomów wejściowych podczas nagrywania
- **Aplikacje głosowe**: Detekcja aktywności głosowej i monitorowanie poziomu
- **Odtwarzacze muzyki**: Wyświetlanie głośności w czasie rzeczywistym i detekcja szczytów
- **Przetwarzanie audio**: Kompresja dynamiczna i automatyczna kontrola wzmocnienia

## Przykłady zastosowań

### Integracja z LED-ami
```c
void led_volume_callback(int left_volume, int right_volume) {
    // Kontroluj paski LED na podstawie poziomu głośności
    int left_leds = (left_volume * 10) / 100;  // 10 LED-ów
    int right_leds = (right_volume * 10) / 100;
    
    update_led_strip(left_leds, right_leds);
}
```

### Detekcja aktywności głosowej
```c
void voice_activity_callback(int left_volume, int right_volume) {
    static int silence_counter = 0;
    int avg_volume = (left_volume + right_volume) / 2;
    
    if (avg_volume > 15) {  // Próg aktywności głosowej
        silence_counter = 0;
        voice_activity_detected();
    } else {
        silence_counter++;
        if (silence_counter > 30) {  // 2 sekundy ciszy przy 15Hz
            voice_activity_stopped();
        }
    }
}
```

## Wsparcie i rozwój

Wkłady są mile widziane! Prosimy o przesyłanie pull requestów, zgłoszeń błędów lub próśb o funkcje.

## Licencja

Ten projekt jest licencjonowany na licencji MIT - zobacz plik [LICENSE](LICENSE) dla szczegółów.
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Autor

**Jaromir Kopp** - 2025

---

*Ten element został zaprojektowany dla ESP-ADF v2.7+ i został przetestowany z różnymi formatami audio i częstotliwościami próbkowania. W przypadku pytań lub wsparcia, prosimy o otwarcie issue na GitHub.*