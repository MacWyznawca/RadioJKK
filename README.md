# JkkRadio Internet Radio for **AI Thinker ESP32-A1S**  
  
## **Features:**  
- Playing internet radio stations from a list.  
- Recording stream to SD card in AAC format.  
- Volume control.  
- Graphic equalizer settings selection.  
- Storing equalizer and radio station settings.
- Playback error handling.
  
Work in progress!  
  
## **Hardware Requirements:**  
#### Development board **AI Thinker ESP32-A1S**  
Technical specifications:  
- ESP32-D0WD rev. 3.1  
- 4 MB Flash  
- 8 MB PSRAM (4 MB available for system)
- Audio CODEC es8388 with preamplifier  
- Audio amplifiers NS4150 2x 3W (4Ω)   
- Battery connector   
- USB UART connector  
- microSD card slot  
Sample offer: [App: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_ooTic0A), [Web: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_onbBPzW) (affiliate)

![AI Thinker ESP32-A1S](img/ESP32A1S.jpeg)
  
#### MicroSD card (up to 64 GB)  

#### Recommended display

OLED SSD1306 128x64 with i2c bus. Good if it has built-in 4 buttons or provide such buttons separately for more convenient use e.g. [OLED SSD1306 128x64 with four buttons](https://s.click.aliexpress.com/e/_oFKo8XC)

[![Sample display](img/OLED-i2c.jpeg)](https://s.click.aliexpress.com/e/_oFKo8XC)

If the project is compiled in idf.py menuconfig, you can also select SH1107 display in i2c configuration.

Connection:
- SDA: **GPIO18**
- SCL: **GPIO5**

External button connections:
- KEY4 [Up] GPIO23
- KEY3 [Down] GPIO19
- KEY2 [Eq/Rec] GPIO13/MTCK (note: change dip-switch settings)
- KEY1 [Stations] GPIO22

![i2c display and external keyboard connection](img/ESP32A1S-OLED-connections.jpeg)
  
## Using pre-compiled file:  
Flash any ESP32 flashing tool with selected file from `bin` folder from address 0x0. e.g. with command:   
```
esptool.py -p /dev/cu.usbserial-0001 write_flash 0x0 bin/RadioJKK_v0.bin  
```
- `RadioJKK_v0.bin` - version without display, AI Thinker ESP32-A1S development board only
- `RadioJKK_LCD_board_keys_v0.bin` - version for SSD1306 128x64 i2c display
- `RadioJKK_LCD_ext_keys_v0.bin` - version for SSD1306 128x64 i2c display with possibility to connect external buttons

  
### Preparing microSD card:  
- Format FAT32 (MS-DOS).  
- Text file (plain text) `settings.txt` with WiFi network name and password separated by semicolon (one line of text):  
```
mySSID;myPassword
```

After restart, WiFi settings are saved in NVS flash memory and the presence of SD card in the reader is no longer necessary.
  
- Text file (plain text) `stations.txt` with list of radio stations (maximum **40**) in csv format (fields separated by semicolon).  
`station url;short name;description;1 or 0 (favorites);type` e.g.:  
```
http://stream2.nadaje.com:9248/prw.aac;RW;Radio Wrocław - Public Radio;0;5  
http://stream2.nadaje.com:9228/ram.aac;RAM;Radio RAM;0;1  
http://stream2.nadaje.com:9238/rwkultura.aac;RWK;Radio Wrocław Kultura;1;1  
http://mp3.polskieradio.pl:8904/;Trójka;Polskie Radio Program Trzeci;0;1  
```

- Text file (plain text) `eq.txt` with list of graphic equalizer settings (maximum 10) in csv format (fields separated by comma or semicolon).  
`name;0;0;0;0;0;0;0;0;0;0` (always 10 correction settings in dB) e.g.:  
```
flat;0;0;0;0;0;0;0;0;0;0
music;0;4;3;1;0;-1;0;1;3;6
rock;0;6;6;4;0;-1;-1;0;6;10
```
  
Files should be placed in the root directory of the microSD card.  

Station list and equalizers are stored in NVS flash memory. To change one or more stations (equalizers), upload a new list to SD card and place it in the reader. After updating the list, which takes up to 3 seconds, you can remove the SD card if you don't want to record streams. New WiFi network settings will be used after restart.
  
Sample files are in the repository.  
  
## Compilation from source files:  
Software is tested with ESP-IDF 5.4.1 or 5.4.2 and ESP-ADF v2.7 or newer.  
  
Installation description [ESP-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#quick-start). Repository [ESP-ADF on GitHub](https://github.com/espressif/esp-adf).  

### Installation from GitHub
```bash
git clone --recurse-submodules https://github.com/MacWyznawca/RadioJKK.git
```
In case of downloading as .zip or without submodules:
```bash
git submodule update --init --recursive
```
  
## Usage:  
- **Key 6** [short] louder.  
- **Key 5** [short] quieter, [long] mute.  
- **Key 4** [short] next station from list, [long] return to position 1. LED blinks according to number on the list.  
- **Key 3** [short] previous station from list, [long] favorite (first favorite from list). LED blinks according to number on the list.  
- **Key 2** [short] start recording (LED blinks twice every 3 seconds), [long] stop recording (LED blinks 3 times).  
- **Key 1** [short] go to next equalizer setting, [long] return to settings without correction.  

### Display Version Controls:
- **Key 4** [short press] volume up. When station list or equalizer is displayed - scroll through the list.
- **Key 3** [short press] volume down. When station list or equalizer is displayed - scroll through the list. [long press] mute.
- **Key 2** [short press] call equalizer menu, [long press] start recording, press again: stop recording.
- **Key 1** [short press] call radio station menu. When Roller is displayed (stations or equalizers) confirm selection. [long press] exit list without confirming selection. 

The selected station and equalizer are stored in NVS memory and restored at startup. The writing occurs 10 seconds after the change to limit the number of writes.

If an error occurs while changing stations, it reverts to the previously played station.
    
Audio files are saved in `rec/recording_date` folder with additional common text file containing audio file path, station name and recording start time.
