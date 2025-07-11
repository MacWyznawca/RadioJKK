# JkkRadio Internet Radio dla **AI Thinker ESP32-A1S**  
  
## **Funkcje:**  
- Odtwarzanie stacji radiowych internetowych z listy.  
- Nagrywanie strumienia na kartę SD w formacie AAC.  
- Kontrola głośności.  
- Wybór ustawień korektora graficznego.  
- Zapis wybranego equalizera i stacji radiowej.
- Obsługa błędów odtwarzania.
  
Praca w toku!  
  
## **Wymagania sprzętowe:**  
#### Płytka deweloperska **AI Thinker ESP32-A1S**  
Specyfikacja techniczna:  
- ESP32-D0WD rev. 3.1  
- 4 MB Flash  
- 8 MB PSRAM (4 MB dostępne dla systemu)
- Koder audio es8388 z przedwzmacniaczem  
- Wzmacniacze audio NS4150 2x 3W (4Ω)   
- Złącze baterii   
- Złącze USB UART  
- Gniazdo karty microSD  
Przykładowa oferta: [App: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_ooTic0A), [Web: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_onbBPzW) (affiliate)

![AI Thinker ESP32-A1S](img/ESP32A1S.jpeg)
  
#### Karta MicroSD (do 64 GB)  

#### Zalecany wyświetlacz

OLED SSD1306 128x64 z magistralą i2c. Dobrze, jeśli ma wbudowane 4 przyciski lub zapewnij takie przyciski osobno dla wygodniejszego użytkowania np. [OLED SSD1306 128x64 z czterema przyciskami](https://s.click.aliexpress.com/e/_oFKo8XC)

[![Przykładowy wyświetlacz](img/OLED-i2c.jpeg)](https://s.click.aliexpress.com/e/_oFKo8XC)

Jeśli projekt jest kompilowany w idf.py menuconfig, możesz również wybrać wyświetlacz SH1107 w konfiguracji i2c.

Połączenie:
- SDA: **GPIO18**
- SCL: **GPIO5**

Połączenia przycisków zewnętrznych:
- KEY4 [Góra] GPIO23
- KEY3 [Dół] GPIO19
- KEY2 [Eq/Rec] GPIO13/MTCK (uwaga: zmień ustawienia przełączników dip)
- KEY1 [Stacje] GPIO22

![połączenie wyświetlacza i2c i klawiatury zewnętrznej](img/ESP32A1S-OLED-connections.jpeg)
  
## Użycie prekompilowanego pliku:  
Wgraj dowolnym narzędziem do flashowania ESP32 z wybranym plikiem z folderu `bin` z adresu 0x0. np. komendą:   
```
esptool.py -p /dev/cu.usbserial-0001 write_flash 0x0 bin/RadioJKK_v0.bin  
```
- `RadioJKK_v0.bin` - wersja bez wyświetlacza, tylko płytka deweloperska AI Thinker ESP32-A1S
- `RadioJKK_LCD_board_keys_v0.bin` - wersja dla wyświetlacza SSD1306 128x64 i2c
- `RadioJKK_LCD_ext_keys_v0.bin` - wersja dla wyświetlacza SSD1306 128x64 i2c z możliwością podłączenia przycisków zewnętrznych

  
### Przygotowanie karty microSD:  
- Format FAT32 (MS-DOS).  
- Plik tekstowy (zwykły tekst) `settings.txt` z nazwą sieci WiFi i hasłem oddzielonymi średnikiem (jedna linia tekstu):  
```
mySSID;myPassword
```

Po restarcie ustawienia WiFi są zapisane w pamięci flash NVS i obecność karty SD w czytniku nie jest już konieczna.
  
- Plik tekstowy (zwykły tekst) `stations.txt` z listą stacji radiowych (maksymalnie **40**) w formacie csv (pola oddzielone średnikiem).  
`url stacji;krótka nazwa;opis;1 lub 0 (ulubione);typ` np.:  
```
http://stream2.nadaje.com:9248/prw.aac;RW;Radio Wrocław - Radio Publiczne;0;5  
http://stream2.nadaje.com:9228/ram.aac;RAM;Radio RAM;0;1  
http://stream2.nadaje.com:9238/rwkultura.aac;RWK;Radio Wrocław Kultura;1;1  
http://mp3.polskieradio.pl:8904/;Trójka;Polskie Radio Program Trzeci;0;1  
```

- Plik tekstowy (zwykły tekst) `eq.txt` z listą ustawień korektora graficznego (maksymalnie 10) w formacie csv (pola oddzielone przecinkiem lub średnikiem).  
`nazwa;0;0;0;0;0;0;0;0;0;0` (zawsze 10 ustawień korekcji w dB) np.:  
```
flat;0;0;0;0;0;0;0;0;0;0
music;0;4;3;1;0;-1;0;1;3;6
rock;0;6;6;4;0;-1;-1;0;6;10
```
  
Pliki powinny być umieszczone w katalogu głównym karty microSD.  

Lista stacji i korektory są przechowywane w pamięci flash NVS. Aby zmienić jedną lub więcej stacji (korektorów), wgraj nową listę na kartę SD i umieść ją w czytniku. Po aktualizacji listy, która zajmuje do 3 sekund, możesz usunąć kartę SD, jeśli nie chcesz nagrywać strumieni. Nowe ustawienia sieci WiFi będą użyte po restarcie.
  
Przykładowe pliki są w repozytorium.  
  
## Kompilacja z plików źródłowych:  
Oprogramowanie jest testowane z ESP-IDF 5.4.1 lub 5.4.2 i ESP-ADF v2.7 lub nowszym.  
  
Opis instalacji [ESP-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#quick-start). Repozytorium [ESP-ADF na GitHub](https://github.com/espressif/esp-adf).  

### Instalacja z GitHub
```bash
git clone --recurse-submodules https://github.com/MacWyznawca/RadioJKK.git
```
lub
```bash
git submodule update --init --recursive
```
  
## Użytkowanie:  
- **Klawisz 6** [krótko] głośniej.  
- **Klawisz 5** [krótko] ciszej, [długo] wyciszenie.  
- **Klawisz 4** [krótko] następna stacja z listy, [długo] powrót do pozycji 1. LED miga zgodnie z numerem na liście.  
- **Klawisz 3** [krótko] poprzednia stacja z listy, [długo] ulubiona (pierwsza ulubiona z listy). LED miga zgodnie z numerem na liście.  
- **Klawisz 2** [krótko] rozpocznij nagrywanie (LED miga dwa razy co 3 sekundy), [długo] zatrzymaj nagrywanie (LED miga 3 razy).  
- **Klawisz 1** [krótko] przejdź do następnego ustawienia korektora, [długo] powrót do ustawień bez korekcji.  

### Sterowanie w wersji z wyświetlaczem:
- **Klawisz 4** [krótko] głośniej. Gdy wyświetlana jest lista stacji lub korektora - przewijanie przez listę.
- **Klawisz 3** [krótko] ciszej. Gdy wyświetlana jest lista stacji lub korektora - przewijanie przez listę. [długo] wyciszenie.  
- **Klawisz 2** [krótko] wywołaj menu korektora, [długo] rozpocznij nagrywanie, naciśnij ponownie: zatrzymaj nagrywanie.  
- **Klawisz 1** [krótko] wywołaj menu stacji radiowych. Gdy wyświetlany jest Roller (stacje lub equalizery) zatwierdzenie wyboru. [długo] wyjdź z listy bez zatwierdzania wyboru (ESC).  

Wybrana stacja i equalizer są zapisywane w pamięci NVS i odtwarzane przy uruchomieniu. Zapis następuje po 10 sekundach od zmiany, aby ograniczyć ilóść zapisów.

W przypadku błędu przy zmianie stacji następuje powrót do poprzednio dotwarzanej.
    
Pliki zapisów audio są zapisywane w folderze `rec/recording_date` z dodatkowym wspólnym plikiem tekstowym zawierającym ścieżkę pliku audio, nazwę stacji i czas rozpoczęcia nagrywania.
