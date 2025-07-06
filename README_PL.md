# JkkRadio internetowe radio dla **AI Thinker ESP32-A1S**  
  
## **Możliwości:**  
- Odtwarzanie radiowych stacji internetowych z listy.  
- Nagrywanie strumienia na kartę SD w formacie AAC.  
- Sterowanie głośnością.  
- Wybór ustawień equalizera graficznego.  
  
Prace trwają!  
  
## **Wymagania sprzętowe:**  
#### Płytka deweloperska **AI Thinker ESP32-A1S**  
Dane techniczne:  
- ESP32-D0WD rev. 3.1  
- 4 MB Flash  
- 8 MB PSRAM (4 MB dostępne dla systemu)
- Audio CODEC es8388 z przedwzmacniaczem  
- Wzmacniacze audio NS4150 2x 3W (4Ω)   
- Złącze dla akumulatora   
- Złącze USB UART  
- Gniazdo dla kart microSD  
Przykładowa oferta: [App: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_ooTic0A), [Web: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_onbBPzW) (afiliacja)

![AI Thinker ESP32-A1S](img/ESP32A1S.jpeg)
  
#### Karta mikroSD (do 64 GB)  

#### Zalecany wyświetlacz

OLED SSD1306 128x64 z magistralą i2c. Dobrze, jeżeli ma wbudowane 4 przyciski lub zapewnimy takie przyciski osobno dla wygodnieszego użytkowania np. [OLED SSD1306 128x64 z czterema przyciskami](https://s.click.aliexpress.com/e/_oFKo8XC)

Jeżeli projekt jest kompilowany w idf.py menuconfig można wybrać wyświetlacz SH1107 również w konfiguracji i2c.

Podłączenie:
- SDA: **GPIO18**
- SCL: **GPIO5**

Podłączenie przycisków zewnątrznych:
- KEY4 [Up] GPIO23
- KEY3 [Down] GPIO19
- KEY2 [Eq/Rec] GPIO13/MTCK (uwaga: zmień ustawienai dip-switch)
- KEY1 [Stations] GPIO22
  
## Użycie gotowego skompilowanego pliku:  
Wgrać dowolnym narzędziem do flashowania ESP32 plik `RadioJKK_v0.bin` lub `RadioJKK_LCD_v0.bin` dla wersji z OLED SSD1306 z folderu `bin` od adresu 0x0. np. komendą:   
```
esptool.py -p /dev/cu.usbserial-0001 write_flash 0x0 bin/RadioJKK_v0.bin  
```
  
### Przygotowanie karty microSD:  
- Format FAT32 (MS-DOS).  
- Plik tekstowy (plain text) `settings.txt` z nazwą sieci WiFi i hasłem oddzielonym średnikiem (jedna linia tekstu):  
```
myssid;mypassword  
```

Po restarcie ustawienia WiFi są zapisywane w pamięci flash NVS i obecność karty SD w czytniku nie jest już konieczna.
  
- Plik tekstowy (plain text) `stations.txt` z listą stacji radiowych (maksymalnie 20) w formacie csv (pola rozdzielone średnikiem).  
`url stacji;nazwa krótka;opis;1 lub 0 (ulubione);rodzaj` np.:  
```
http://stream2.nadaje.com:9248/prw.aac;RW;Radio Wrocław - Public Radio;0;5  
http://stream2.nadaje.com:9228/ram.aac;RAM;Radio RAM;0;1  
http://stream2.nadaje.com:9238/rwkultura.aac;RWK;Radio Wrocław Kultura;1;1  
http://mp3.polskieradio.pl:8904/;Trójka;Polskie Radio Program Trzeci;0;1  
```

- Plik tekstowy (plain text) `eq.txt` z listą ustawień equalizerów graficznych (maksymalnie 10) w formacie csv (pola rozdzielone przecinkiem lub średnikiem).  
`nazwa;0;0;0;0;0;0;0;0;0;0` (zawsze 10 ustawień) np.:  
```
flat;0;0;0;0;0;0;0;0;0;0
music;0;4;3;1;0;-1;0;1;3;6
rock;0;6;6;4;0;-1;-1;0;6;10
```
  
Pliku należy umieścić w katalogu głównym karty microSD.  

Lista stacji oraz equalizery są zapamiętywane w pamięci flash NVS. Aby zmienić jedną lub więcej stacji (eqyalizerów), wgraj nową listę na kartę SD i umieść ją w czytniku. Po aktualizacji listy, która trwa do 3 sekund, możesz wyjąć kartę SD, jeżeli nie chcesz nagrywać strumieni.
  
Przykładowe pliki znajdują się w repozytorium.  
  
## Kompilacja z plików źródłowych:  
Oprogramowanie jest sprawdzane z ESP-IDF 5.4.1 lub 5.4.2 oraz ESP-ADF (najnowszą wersją).  
  
Opis instalacji [ESP-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#quick-start). Repozytorium [ESP-ADF on GitHub](https://github.com/espressif/esp-adf).  
  
## Używanie:  
- **Klawisz 6** [krótko] głośniej.  
- **Klawisz 5** [krótko] ciszej, [długo] mute.  
- **Klawisz 4** [krótko] następna stacja z listy, [długo] powrót do pozycji 1. Dioda miga zgodnie z numerem na liście.  
- **Klawisz 3** [krótko] poprzednia stacja z listy, [długo] ulubiona (pierwsza ulubiona z listy). Dioda miga zgodnie z numerem na liście.  
- **Klawisz 2** [krótko] rozpoczęcie nagrywania (dioda co 3 sekundy miga dwukrotnie), [długo] zakończenie nagrywania (dioda mika 3 razy).  
- **Klawisz 1** [krótko] przejście do kolejnego ustawienia equalizera, [długo] powrót do ustawień bez korekcji.  

### Sterowanie w wersji z wyświetlaczem:
- **Klawisz 4** [krótko] głośniej. Gdy wyświetlana jest lista stacji lub equalizerów - przewijanie listy.
- **Klawisz 3** [krótko] cieszej. Gdy wyświetlana jest lista stacji lub equalizerów - przewijanie listy. [długo] wyciszenie.  
- **Klawisz 2** [krótko] wywołanie menu equalizerów, ponownie wciśnięcie zatwierdzenie wyboru. [długo] rozpoczęcie nagrywania, ponowne wciśnięcie: zakończenie nagrywania.  
- **Klawisz 1** [krótko] wywołanie menu stacji radiowych, ponownie wciśnięcie zatwierdzenie wyboru. [długo] wyjście z listy bez zmiany stacji.  
    
Pliki audio zapisywane są w folderze `rec/data_nagrania` z dodatkowym wspólnym plikiem tekstowym zawierającym ścieżkę pliku audio, nazwę stacji i czas startu zapisu.  

