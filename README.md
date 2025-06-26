# JkkRadio – Internet Radio for **AI Thinker ESP32-A1S**

## **Features:**
- Playback of internet radio stations from a list  
- Stream recording to SD card in AAC format  
- Volume control  
- Selection of one of four equalizer settings  

Work in progress!

## **Hardware Requirements:**
Development board: **AI Thinker ESP32-A1S**  
Specifications:
- ESP32-D0WD rev. 3.1  
- 4 MB Flash  
- 8 MB PSRAM  
- ES8388 audio codec with preamp  
- NS4150 audio amplifiers 2x 3W (4Ω)  
- Battery connector  
- USB UART interface  
- microSD card slot  
Example listing: [App: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_ooTic0A), [Web: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_onbBPzW) (affiliate)

![AI Thinker ESP32-A1S](img/ESP32A1S.jpeg)

microSD card (up to 64 GB)

## Using a Precompiled Binary:
Flash the `RadioJKK_v0.bin` file from the `bin` folder to address 0x0 using any ESP32 flashing tool, e.g.:
```
esptool.py -p /dev/cu.usbserial-0001 write_flash 0x0 bin/RadioJKK_v0.bin
```

### Preparing the microSD card:
- Format as FAT32 (MS-DOS)  
- Plain text file `settings.txt` containing WiFi SSID and password separated by a semicolon (single line):  
```
myssid;mypassword
```

After restart, WiFi settings are saved to NVS flash memory, so the SD card is no longer required.

- Plain text file `stations.txt` containing a list of up to 20 stations in CSV format (semicolon-separated):  
`station_url;short_name;description;1 or 0 (favorite);type`, for example:
```
http://stream2.nadaje.com:9248/prw.aac;RW;Radio Wrocław - Public Radio;0;5  
http://stream2.nadaje.com:9228/ram.aac;RAM;Radio RAM;0;1  
http://stream2.nadaje.com:9238/rwkultura.aac;RWK;Radio Wrocław Kultura;1;1  
http://mp3.polskieradio.pl:8904/;Trójka;Polskie Radio Program Trzeci;0;1  
```

Place the file in the root directory of the microSD card.

The station list is saved to NVS flash memory. To change it, copy a new version to the SD card and insert it into the reader. The update takes up to 3 seconds. You can then remove the card if you don't plan to record streams.

Sample files are included in the repository.

## Initial OLED Display Support (I2C)
Support for I2C displays 128x64 SSD1306 or SH1107 (selected via `idf.py menuconfig`). The binary `RadioJKK_LCD_v0.bin` is prepared for SSD1306.

Connection:
- SDA: GPIO 18  
- SCL: GPIO 5  
Note: Buttons 5 and 6 will no longer function. Their roles are reassigned.

## Building from Source:
Tested with ESP-IDF 5.4 / 5.4.1 and the latest version of ESP-ADF.

Quick start guide for installing [ESP-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#quick-start)  
ESP-ADF GitHub repo: [ESP-ADF on GitHub](https://github.com/espressif/esp-adf)

## Usage:
- **Button 6** [short press] volume up  
- **Button 5** [short press] volume down, [long press] mute  
- **Button 4** [short press] next station, [long press] go to first station (LED blinks number of times matching index)  
- **Button 3** [short press] previous station, [long press] favorite station (first favorite from list; LED blinks accordingly)  
- **Button 2** [short press] start recording (LED double-blinks every 3 seconds), [long press] stop recording (LED blinks 3 times)  
- **Button 1** [short press] switch to next equalizer setting, [long press] reset to flat EQ  

### Temporary I2C Display Controls:
- **Button 4** [short press] volume up, [long press] next station  
- **Button 3** [short press] volume down, [long press] previous station  
- **Button 1** [short press] next equalizer setting, [long press] toggle recording on/off  

Inserting the SD card reloads the station list. If missing, a default list is used.

Audio recordings are saved in `rec/data_nagrania` along with a common text file containing the path, station name, and start time.
