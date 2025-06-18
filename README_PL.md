# JkkRadio internetowe radio dla **AI Thinker ESP32-A1S**  
  
## **Możliwości:**  
- Odtwarzanie radiowych stacji internetowych z listy.  
- Nagrywanie strumienia na kartę SD w formacie AAC.  
- Sterowanie głośnością.  
- Wybór jednego z czterech ustawień equalizera.  
  
Prace trwają!  
  
## **Wymagania sprzętowe:**  
Płytka deweloperska **AI Thinker ESP32-A1S**  
Dane techniczne:  
- ESP32-D0WD rev. 3.1  
- 4 MB Flash  
- 8 MB PSRAM  
- Audio CODEC es8388 z przedwzmacniaczem  
- Wzmacniacze audio NS4150 2x 3W (4Ω)   
- Złącze dla akumulatora   
- Złącze USB UART  
- Gniazdo dla kart microSD  
Przykładowa oferta: [**AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_oB0P9NU) (afiliacja)
  
Karta mikroSD (do 64 GB)  
  
## Użycie gotowego skompilowanego pliku:  
Wgrać dowolnym narzędziem do flashowania ESP32 plik `RadioJKK_v0.bin` z folderu bin od adresu 0x0. np. komendą:   
```
esptool.py -p /dev/cu.usbserial-0001 write_flash 0x0 bin/RadioJKK_v0.bin  
```
  
### Przygotowanie karty microSD:  
- Format FAT32 (MS-DOS).  
- Plik tekstowy (plain text) `settings.txt` z nazwą sieci WiFi i hasłem oddzielonym średnikiem (jedna linia tekstu):  
```
myssid;mypassword  
```
  
- Plik tekstowy (plain text) `stations.txt` z listą stacji radiowych (maksymalnie 20) w formacie csv (pola rozdzielone średnikiem).  
`url stacji;nazwa krótka;opis;1 lub 0 (ulubione);rodzaj` np.:  
```
http://stream2.nadaje.com:9248/prw.aac;RW;Radio Wrocław - Public Radio;0;5  
http://stream2.nadaje.com:9228/ram.aac;RAM;Radio RAM;0;1  
http://stream2.nadaje.com:9238/rwkultura.aac;RWK;Radio Wrocław Kultura;1;1  
http://mp3.polskieradio.pl:8904/;Trójka;Polskie Radio Program Trzeci;0;1  
```
  
Pliku należy umieścić w katalogu głównym karty microSD.  
  
Przykładowe pliki znajdują się w repozytorium.  
  
## Kompilacja z plików źródłowych:  
Oprogramowanie jest sprawdzane z ESP-IDF 5.4 lub 5.4.1 oraz ESP-ADF (najnowszą wersją).  
  
Opis instalacji [ESP-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#quick-start). Repozytorium [ESP-ADF on GitHub](https://github.com/espressif/esp-adf).  
  
## Używanie:  
- **Klawisz** 6 [krótko] głośniej.  
- **Klawisz 5** [krótko] ciszej, [długo] mute.  
- **Klawisz 4** [krótko] następna stacja z listy, [długo] powrót do pozycji 1. Dioda miga zgodnie z numerem na liście.  
- **Klawisz 3** [krótko] poprzednia stacja z listy, [długo] ulubiona (pierwsza ulubiona z listy). Dioda miga zgodnie z numerem na liście.  
- **Klawisz 2** [krótko] rozpoczęcie nagrywania (dioda co 3 sekundy miga dwukrotnie), [długo] zakończenie nagrywania (dioda mika 3 razy).  
  
Włożenie karty SD powoduje ponowne załadowanie listy stacji. W przypadku braku listy stacji używana jest domyślna.  
  
Pliki audio zapisywane są w folderze rec/data_nagrania z dodatkowym plikiem tekstowym zawierającym nazwę stacji.  

