# JkkRadio Internet Radio for **AI Thinker ESP32-A1S**

## **Features:**
- Playback of internet radio stations from a list.
- Stream recording to SD card in AAC format.
- Volume control.
- Graphic equalizer settings selection.

Work in progress!

## **Hardware Requirements:**
#### Development Board **AI Thinker ESP32-A1S**
Technical specifications:
- ESP32-D0WD rev. 3.1
- 4 MB Flash
- 8 MB PSRAM (4 MB available for system)
- Audio CODEC es8388 with preamplifier
- Audio amplifiers NS4150 2x 3W (4Ω)
- Battery connector
- USB UART connector
- MicroSD card slot
Sample offer: [App: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_ooTic0A), [Web: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_onbBPzW) (affiliate)

![AI Thinker ESP32-A1S](img/ESP32A1S.jpeg)

#### MicroSD Card (up to 64 GB)

#### Recommended Display

OLED SSD1306 128x64 with i2c bus. Preferably with 4 built-in buttons or provide such buttons separately for more convenient use, e.g. [OLED SSD1306 128x64 with four buttons](https://s.click.aliexpress.com/e/_oFKo8XC)

If the project is compiled in idf.py menuconfig, you can also select SH1107 display in i2c configuration.

Connection:
- SDA: **GPIO18**
- SCL: **GPIO5**

External button connections:
- KEY4 [Up] GPIO23
- KEY3 [Down] GPIO19
- KEY2 [Eq/Rec] GPIO13/MTCK (note: change dip-switch settings see picture)
- KEY1 [Stations] GPIO22

## Using Pre-compiled Binary:
Flash the `RadioJKK_v0.bin` or `RadioJKK_LCD_v0.bin` file for the OLED SSD1306 version from the `bin` folder to address 0x0 using any ESP32 flashing tool. For example:
```
esptool.py -p /dev/cu.usbserial-0001 write_flash 0x0 bin/RadioJKK_v0.bin
```

### MicroSD Card Preparation:
- Format: FAT32 (MS-DOS).
- Text file (plain text) `settings.txt` with WiFi network name and password separated by semicolon (one line of text):
```
mySSID;myPassword
```

After restart, WiFi settings are saved in NVS flash memory and the presence of SD card in the reader is no longer necessary.

- Text file (plain text) `stations.txt` with radio station list (maximum 20) in CSV format (fields separated by semicolon).
`station url;short name;description;1 or 0 (favorite);type` e.g.:
```
http://stream2.nadaje.com:9248/prw.aac;RW;Radio Wrocław - Public Radio;0;5
http://stream2.nadaje.com:9228/ram.aac;RAM;Radio RAM;0;1
http://stream2.nadaje.com:9238/rwkultura.aac;RWK;Radio Wrocław Kultura;1;1
http://mp3.polskieradio.pl:8904/;Trójka;Polskie Radio Program Trzeci;0;1
```

- Text file (plain text) `eq.txt` with graphic equalizer settings list (maximum 10) in CSV format (fields separated by comma or semicolon).
`name;0;0;0;0;0;0;0;0;0;0` (always 10 settings) e.g.:
```
flat;0;0;0;0;0;0;0;0;0;0
music;0;4;3;1;0;-1;0;1;3;6
rock;0;6;6;4;0;-1;-1;0;6;10
```

Files should be placed in the root directory of the microSD card.

Station list and equalizers are stored in NVS flash memory. To change one or more stations (equalizers), upload a new list to the SD card and insert it into the reader. After updating the list, which takes up to 3 seconds, you can remove the SD card if you don't want to record streams.

Example files are available in the repository.

## Compilation from Source Code:
The software is tested with ESP-IDF 5.4.1 or 5.4.2 and ESP-ADF (latest version).

Installation description [ESP-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#quick-start). Repository [ESP-ADF on GitHub](https://github.com/espressif/esp-adf).

## Usage:
- **Key 6** [short] volume up.
- **Key 5** [short] volume down, [long] mute.
- **Key 4** [short] next station from list, [long] return to position 1. LED blinks according to list number.
- **Key 3** [short] previous station from list, [long] favorite (first favorite from list). LED blinks according to list number.
- **Key 2** [short] start recording (LED blinks twice every 3 seconds), [long] stop recording (LED blinks 3 times).
- **Key 1** [short] switch to next equalizer setting, [long] return to settings without correction.

### Control in Display Version:
- **Key 4** [short] volume up. When station or equalizer list is displayed - scroll list.
- **Key 3** [short] volume down. When station or equalizer list is displayed - scroll list. [long] mute.
- **Key 2** [short] call equalizer menu, press again to confirm selection. [long] start recording, press again: stop recording.
- **Key 1** [short] call radio station menu, press again to confirm selection. [long] exit list without changing station.

Audio files are saved in the `rec/recording_date` folder with an additional common text file containing the audio file path, station name and recording start time.