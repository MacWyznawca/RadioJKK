# JkkRadio - Internet Radio for **AI Thinker ESP32-A1S**

## **Features:**
- Playback of internet radio stations from a list.
- Recording of streams to an SD card in AAC format.
- Volume control.
- Selection of one of four equalizer settings.

Work in progress!

## **Hardware Requirements:**
Development board: **AI Thinker ESP32-A1S**  
Technical specifications:  
- ESP32-D0WD rev. 3.1  
- 4 MB Flash  
- 8 MB PSRAM  
- ES8388 Audio CODEC with preamplifier  
- NS4150 audio amplifiers, 2x 3W (4Ω)  
- Battery connector  
- USB UART connector  
- MicroSD card slot  
Example offer: [**AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_onbBPzW) (affiliate)

![AI Thinker ESP32-A1S](img/ESP32A1S.jpeg)

MicroSD card (up to 64 GB)

## Using the Precompiled Binary:
Flash the `RadioJKK_v0.bin` file from the `bin` folder to address 0x0 using any ESP32 flashing tool. Example command:  
```
esptool.py -p /dev/cu.usbserial-0001 write_flash 0x0 bin/RadioJKK_v0.bin
```

### Preparing the MicroSD Card:
- Format: FAT32 (MS-DOS).  
- Create a plain text file `settings.txt` with the WiFi network name and password, separated by a semicolon (single line):  
```
myssid;mypassword
```

- Create a plain text file `stations.txt` with a list of radio stations (up to 20) in CSV format (fields separated by semicolons) `url;short_name;description;favorite(1 or 0);type`.  
Example format:  
```
http://stream2.nadaje.com:9248/prw.aac;RW;Radio Wrocław - Public Radio;0;5
http://stream2.nadaje.com:9228/ram.aac;RAM;Radio RAM;0;1
http://stream2.nadaje.com:9238/rwkultura.aac;RWK;Radio Wrocław Kultura;1;1
http://mp3.polskieradio.pl:8904/;Trójka;Polskie Radio Program Trzeci;0;1
```

Place these files in the root directory of the microSD card.  

Example files are included in the repository.

## Compiling from Source:
The software has been tested with ESP-IDF 5.4 or 5.4.1 and the latest version of ESP-ADF.  

Installation guide: [ESP-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#quick-start)  
Repository: [ESP-ADF on GitHub](https://github.com/espressif/esp-adf)

## Usage:
- **Button 6** [short press]: Volume up.  
- **Button 5** [short press]: Volume down, [long press]: Mute.  
- **Button 4** [short press]: Next station on the list, [long press]: Return to station 1. LED flashes according to the station number.  
- **Button 3** [short press]: Previous station on the list, [long press]: Favorite station (first favorite on the list). LED flashes according to the station number.  
- **Button 2** [short press]: Start recording (LED flashes twice every 3 seconds), [long press]: Stop recording (LED flashes 3 times).
- **Button 1** [short press]: move to the next equalizer setting, [long press] return to the setting without equalizer.

Inserting an SD card reloads the station list. If no list is found, the default list is used.

Audio files are saved in the `rec/data_nagrania` folder, along with a text file containing the station name.