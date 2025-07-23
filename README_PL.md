# RadioJKK32 - Wielofunkcyjny internetowy odtwarzacz radiowy

**RadioJKK32** to zaawansowany projekt internetowego radia opartego na ESP32-A1S (ESP32-A1S Audio Kit), zbudowany z użyciem bibliotek ESP-ADF i zaprojektowany w celu zapewnienia płynnego słuchania muzyki z szeroką gamą funkcji i możliwości sterowania.

## 🌟 Główne funkcje

### 🌐 **Lokalny serwer WWW - NOWOŚĆ!**

- **Zdalne sterowanie** przez przeglądarkę internetową: głośność, wybór stacji, zmiana equalizera
- **Edycja listy stacji radiowych** - zmiany, dodawanie, usuwanie i zmiana kolejności
- **Automatyczne wykrywanie w sieci** dzięki mDNS/Bonjour/NetBIOS
- **Responsywny interfejs** działający na wszystkich urządzeniach
- **Dostęp lokalny** bez potrzeby połączenia z Internetem
- **Automatyczny zapis na karcie SD** aktualnej listy stacji

### Inne nowości

- Zmiana kolejności stacji radiowych przez WWW
- Automatyczny zapis aktualnej listy stacji z pamięci flash urządzenia (NVS) na kartę SD
- Możliwość pobrania aktualniej listy stacji w formacie .csv przez przeglądarkę
- Wybór equalizera w przeglądarce
- Zwiększona maksymalna liczba stacji do 50
- Maksymalna liczba equalizerów zwiększona do 20
- Więcej wbudowanych equalizerów (10)

### 📻 Odtwarzanie audio

- Obsługa wielu formatów audio: **MP3, AAC, OGG, WAV, FLAC, OPUS, M4A, AMR**
- Wysokiej jakości dekodowanie i odtwarzanie
- Automatyczne ponowne łączenie przy problemach z połączeniem

### 🔧 Przetwarzanie dźwięku

- **10-pasmowy equalizer** z predefiniowanymi ustawieniami
- Wskaźnik poziomu audio w czasie rzeczywistym (wymagany wyświetlacz)
- Możliwość włączania/wyłączania przetwarzania (equalizera) audio

### 💾 Nagrywanie

- **Nagrywanie na kartę SD** w formacie AAC
- Automatyczne tworzenie struktury folderów według daty
- Pliki informacyjne z metadanymi nagrań
- Wsparcie dla kart SD o dużej pojemności

### 📱 Interfejsy użytkownika

- **Lokalny serwer WWW** - główny sposób sterowania
- **OLED I2C (SSD1306/SH1107)** z graficznym interfejsem LVGL
- **Klawiatura GPIO** z obsługą długich naciśnięć
- **Kody QR** (opcja z wyświetlaczem) dla łatwej konfiguracji WiFi

### 💾 Zapis bieżących ustawień

- **Stacja, equalizer i głośność** są zapisywane i odtwarzane po uruchomieniu/restarcie
- **Bezpieczeństwo pamięci flash** - dane są zapisywane dopiero 10 sekund po zmianie, na wypadek kolejnych częstych zmian
- **Przewidywana trwałość flash** - przy bardzo intensywnym (kilkaset zmian na dobę) użytkowaniu minimum 15 lat

### 💾 Kopia listy stacji radiowych

- **Zapis listy stacji na kartę SD** w formacie zgodnym z używanym przez RadioJKK

### 🔗 Łączność

- **WiFi** z automatycznym provisioningiem przez aplikację ESP SoftAP Prov
- **mDNS/Bonjour, NetBIOS** dla łatwego odnajdywania w sieci
- **SNTP** dla synchronizacji czasu
- Obsługa konfiguracji przez aplikację ESP SoftAP

### ⚙️ Konfiguracja i zarządzanie

- **Konfiguracja przez kartę SD** (stacje, equalizer, WiFi)
- **Pamięć NVS** dla trwałego przechowywania ustawień
- **Automatyczne wczytywanie** konfiguracji przy starcie

## 🚀 Jak zacząć

### Wymagania sprzętowe

- **ESP32-A1S Audio Kit**
- **Karta microSD** (opcjonalnie)
- **Wyświetlacz OLED I2C** (opcjonalnie)

Przykładowa oferta: [App: ](https://s.click.aliexpress.com/e/_ooTic0A)[**AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_ooTic0A), [Web: ](https://s.click.aliexpress.com/e/_onbBPzW)[**AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_onbBPzW)



#### Zalecany wyświetlacz

OLED SSD1306 128x64 z magistralą I2C. Dobrze, jeśli ma wbudowane 4 przyciski lub zapewnij takie przyciski osobno dla wygodniejszego użytkowania, np. [OLED SSD1306 128x64 z czterema przyciskami](https://s.click.aliexpress.com/e/_oFKo8XC)

#### Połączenie wyświetlacza:

- SDA: **GPIO18**
- SCL: **GPIO5**

#### Połączenia opcjonalnych przycisków zewnętrznych:

- KEY4 [Góra] **GPIO23**
- KEY3 [Dół] **GPIO19**
- KEY2 [Eq/Rec] **GPIO13/MTCK** (uwaga: zmień ustawienia przełączników DIP)
- KEY1 [Stacje] **GPIO22**



### Instalacja

1. **Klonowanie repozytorium:**

   ```bash
   git clone --recurse-submodules https://github.com/MacWyznawca/RadioJKK.git
   cd radioJKK32
   ```

2. **Konfiguracja ESP-IDF i ESP-ADF:** Opis instalacji: [ESP-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#quick-start). Repozytorium: [ESP-ADF na GitHub](https://github.com/espressif/esp-adf).

   **Uwaga:** dla ESP-IDF 5.4.x i 5.5.x użyj:

   ```bash
   cd $IDF_PATH
   git apply $ADF_PATH/idf_patches/idf_v5.4_freertos.patch
   ```

3. **Kompilacja i wgranie:**

   ```bash
   idf.py build
   idf.py -p [nazwa/port COM] flash monitor
   ```

   Wyjście z monitora: Ctrl + ]

4. **Użycie prekompilowanego pliku:**\
   Wgraj dowolnym narzędziem do flashowania ESP32 wybrany plik z folderu `bin` pod adres 0x0, np.:

   ```bash
   esptool.py -p /dev/cu.usbserial-0001 write_flash 0x0 bin/RadioJKK_v1.bin
   ```

## 📌 Konfiguracja

### Konfiguracja WiFi

W przypadku skanowania kodu **QR** przejdź do punktu 3.

1. **Przy pierwszym uruchomieniu** urządzenie utworzy punkt dostępowy "JKK..."
2. **Połącz się** z tym punktem i użyj aplikacji ESP SoftAP
3. **Zeskanuj kod QR** wyświetlany na OLED lub wpisz dane ręcznie. PIN: jkk
4. **Wprowadź dane WiFi** swojej sieci

**Uwaga:** po pierwszej konfiguracji, jeśli serwer WWW nie odpowiada, zalecany jest restart urządzenia.

**Alternatywnie za pomocą karty SD**:

Utwórz plik `settings.txt` z nazwą sieci WiFi i hasłem oddzielonymi średnikiem (jedna linia tekstu):

```
mySSID;myPassword
```

Jeżeli nie chcesz uruchamiać serwera WWW, dodaj na końcu po średniku: `wwwoff`

```
mySSID;myPassword;wwwoff
```

### Lista stacji radiowych

Przez interfejs WWW lub kartę SD

Utwórz plik `stations.txt` na karcie SD w formacie:

```
http://stream.url;KrótkoNazwa;Długa nazwa stacji;0;1;opis_audio
```

**Przykład:**

```
http://mp3.polskieradio.pl:8904/;PR3;Polskie Radio Program Trzeci;0;1;
http://stream2.nadaje.com:9248/prw.aac;RW;Radio Wrocław;0;5;
```

### Predefiniowane ustawienia equalizera

Utwórz plik `eq.txt` na karcie SD:

```
flat;0;0;0;0;0;0;0;0;0;0
music;2;3;1;0;-1;-2;0;1;2;0
rock;4;5;3;1;-1;-3;-1;3;4;0
```

Zawsze 10 ustawień korekcji w dB.

**Uwaga:** wszystkie pliki konfiguracyjne należy zapisywać w głównym katalogu karty SD.

## 🌐 Serwer WWW

### Dostęp do serwera

- **Automatyczne wykrywanie:** `http://radiojkk32.local` (dzięki mDNS/Bonjour). NetBIOS: `RadioJKK`
- **Bezpośredni IP:** `http://[adres-ip-urządzenia]`
- **Port:** 80 (domyślny)

### Funkcje serwera WWW

- 📻 **Sterowanie odtwarzaniem** (play/pause/stop)
- 🔊 **Regulacja głośności** w czasie rzeczywistym
- 📋 **Zmiana stacji** z pełną listą dostępnych opcji
- 📋 **Edycja listy stacji** bez potrzeby fizycznego dostępu

## 🎠 Obsługa przycisków (tryb bez OLED)

| Przycisk      | Krótkie naciśnięcie | Długie naciśnięcie |
| ------------- | ------------------- | ------------------ |
| **PLAY** KEY3 | Poprzednia stacja   | Ulubiona stacja    |
| **SET** KEY4  | Następna stacja     | Pierwsza stacja    |
| **MODE** KEY2 | Następny equalizer  | Reset equalizera   |
| **REC** KEY1  | Start nagrywania    | Stop nagrywania    |
| **VOL+** KEY6 | Zwiększ głośność    | -                  |
| **VOL-** KEY5 | Zmniejsz głośność   | Wycisz             |

## 🖥️ Obsługa OLED (gdy włączone)

| Przycisk             | Krótkie naciśnięcie         | Długie naciśnięcie    |
| -------------------- | --------------------------- | --------------------- |
| **MODE** KEY1        | Lista stacji/Potwierdź      | Zamknij [ESC]         |
| **SET** KEY2         | Lista equalizer             | Nagrywanie start/stop |
| **VOL+/-** KEY4/KEY3 | Nawigacja w menu / Głośność | Wyciszenie / Ulubiona |

## ⚖️ Opcje konfiguracji

Projekt oferuje szerokie możliwości konfiguracji przez `menuconfig`:

```bash
idf.py menuconfig
```

### Dostępne opcje:

- **Typ wyświetlacza:** SSD1306/SH1107
- **Rozdzielczość:** 128x64
- **Typ przycisków:** GPIO
- **Wariant płytki:** ESP32-A1S
- **Karta SD:** włącz/wyłącz
- **Klawisze zewnętrzne:** opcjonalne

## 🌍 Internacjonalizacja

Projekt obsługuje polskie znaki diakrytyczne z automatyczną konwersją UTF-8 na ASCII dla wyświetlaczy monochromatycznych.

## 🤝 Wkład w projekt

Zapraszamy do współtworzenia projektu!

1. **Fork** repozytorium
2. **Utwórz branch** dla swojej funkcji
3. **Dodaj zmiany** z opisowymi commitami
4. **Prześlij Pull Request**

## 📄 Licencja

Ten projekt jest licencjonowany na licencji **MIT License** - zobacz plik [LICENSE](LICENSE) dla szczegółów.

## 👏 Podziękowania

- **Espressif Systems** za ESP-IDF i ESP-ADF
- **LVGL** za bibliotekę graficzną
- **Społeczność open-source** za wsparcie i inspirację

## 📞 Kontakt

- **Autor:** Jaromir K. Kopp (JKK)
- **GitHub:** [MacWyznawca](https://github.com/MacWyznawca)

---

**RadioJKK32** - Radio internetowe z kontrolą przez przeglądarkę 🎵