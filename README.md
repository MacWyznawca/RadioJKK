# RadioJKK32 - Multifunction Internet Radio Player

**RadioJKK32** is an advanced internet radio project based on ESP32-A1S (ESP32-A1S Audio Kit), built using ESP-ADF libraries and designed to provide a seamless listening experience with a wide range of features and control capabilities.

## 🌟 Key Features

### 🌐 **Local Web Server - NEW!**
- **Remote control** via web browser
- **Real-time radio station list modification**
- **Automatic network discovery** with mDNS/Bonjour/NetBIOS
- **Responsive interface** working on all devices
- **Local access** without internet connection requirement

### 📻 Audio Playback
- Multiple audio format support: **MP3, AAC, OGG, WAV, FLAC, OPUS, M4A, AMR**
- Internet streaming with automatic playlist parsing
- High-quality decoding and playback
- Automatic reconnect on connection issues

### 🔧 Audio Processing
- **10-band equalizer** with predefined settings
- Real-time audio level indicator
- Audio processing (equalizer) enable/disable capability

### 💾 Recording
- **SD card recording** in AAC format
- Automatic folder structure creation by date
- Information files with recording metadata
- Support for high-capacity SD cards

### 📱 User Interfaces
- **Local web server** - primary control method
- **I2C OLED (SSD1306/SH1107)** with LVGL graphical interface
- **GPIO keypad** with long press support
- **QR codes** (display option) for easy WiFi configuration

### 🔗 Connectivity
- **WiFi** with automatic provisioning via ESP SoftAP Prov app
- **mDNS/Bonjour, NetBIOS** for easy network discovery
- **SNTP** for time synchronization
- Configuration support through ESP SoftAP app

### ⚙️ Configuration and Management
- **SD card configuration** (stations, equalizer, WiFi)
- **NVS memory** for persistent settings storage
- **Automatic loading** of configuration at startup

## 🚀 Getting Started

### Hardware Requirements
- **ESP32-A1S Audio Kit**
- **MicroSD card** (optional)
- **I2C OLED display** (optional)

Example listings: [App: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_ooTic0A), [Web: **AI Thinker ESP32-A1S**](https://s.click.aliexpress.com/e/_onbBPzW) (affiliate)

![AI Thinker ESP32-A1S](img/ESP32A1S.jpeg)

#### Recommended Display

OLED SSD1306 128x64 with I2C bus. Ideally with built-in 4 buttons or provide separate buttons for more convenient use, e.g., [OLED SSD1306 128x64 with four buttons](https://s.click.aliexpress.com/e/_oFKo8XC)

[![Example display](img/OLED-i2c.jpeg)](https://s.click.aliexpress.com/e/_oFKo8XC)

#### Display Connection:
- SDA: **GPIO18**
- SCL: **GPIO5**

#### Optional External Button Connections:
- KEY4 [Up] **GPIO23**
- KEY3 [Down] **GPIO19**
- KEY2 [Eq/Rec] **GPIO13/MTCK** (note: change DIP switch settings)
- KEY1 [Stations] **GPIO22**

![I2C display and external keypad connection](img/ESP32A1S-OLED-connections.jpeg)

### Installation
1. **Clone repository:**
   ```bash
   git clone --recurse-submodules https://github.com/MacWyznawca/RadioJKK.git
   cd RadioJKK32
   ```

2. **ESP-IDF and ESP-ADF configuration:**
[ESP-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#quick-start) installation guide. [ESP-ADF repository on GitHub](https://github.com/espressif/esp-adf).

**Note**: for ESP-IDF 5.4.x and 5.5.x use:
   ```bash
   cd $IDF_PATH 
   git apply $ADF_PATH/idf_patches/idf_v5.4_freertos.patch
   ```

3. **Build and flash:**
   ```bash
   idf.py build
   idf.py -p [COM port name/path] flash monitor
   ```
   Exit monitor: Control+] (CTRL + right square bracket)

4. **Using precompiled binary:**  
   Flash any ESP32 flashing tool with selected file from `bin` folder at address 0x0. e.g., with command:   
   ```bash
   esptool.py -p /dev/cu.usbserial-0001 write_flash 0x0 bin/RadioJKK_v0.bin
   ```

## 📋 Configuration

### WiFi Configuration
If scanning **QR code**, proceed to step 3.
1. **On first boot** device will create access point "JKK..."
2. **Connect** to this access point and use ESP SoftAP app
3. **Scan QR code** displayed on OLED or enter data manually. PIN: jkk
4. **Enter WiFi credentials** for your network

**Note**: after initial configuration, if web server doesn't respond, restart the device is recommended.

**Alternative using SD card**:

Create `settings.txt` file with WiFi network name and password separated by semicolon (single line of text):  
```
mySSID;myPassword
```
If you don't want to start the web server, add at the end after semicolon: wwwoff
```
mySSID;myPassword;wwwoff
```

### Radio Station List
Via web interface or SD card

Create `stations.txt` file on SD card in format:
```
http://stream.url;ShortName;Long station name;0;1;audio_description
```

**Example:**
```
http://mp3.polskieradio.pl:8904/;PR3;Polskie Radio Program Trzeci;0;1;
http://stream2.nadaje.com:9248/prw.aac;RW;Radio Wrocław;0;5;
```

### Predefined Equalizer Settings
Create `eq.txt` file on SD card:
```
flat;0;0;0;0;0;0;0;0;0;0
music;2;3;1;0;-1;-2;0;1;2;0
rock;4;5;3;1;-1;-3;-1;3;4;0
```
Always 10 correction settings in dB

**Note**: All configuration files should be saved in the root directory of the SD card.

## 🌐 Web Server

### Server Access
- **Automatic discovery:** `http://radiojkk32.local` (via mDNS/Bonjour). NetBIOS: RadioJKK
- **Direct IP:** `http://[device-ip-address]`
- **Port:** 80 (default)

### Web Server Features
- 📻 **Playback control** (play/pause/stop)
- 🔊 **Real-time volume control**
- 📝 **Station switching** with full list of available options
- 📋 **Station list editing** without physical access requirement

## 🎛️ Button Operation (without OLED mode)

| Button | Short Press | Long Press |
|--------|-------------|------------|
| **PLAY** KEY3 | Previous station | Favorite station |
| **SET** KEY4 | Next station | First station |
| **MODE** KEY2 | Next equalizer | Reset equalizer |
| **REC** KEY1 | Start recording | Stop recording |
| **VOL+** KEY6| Increase volume | - |
| **VOL-** KEY5 | Decrease volume | Mute |

## 🖥️ OLED Operation (when enabled)

| Button | Short Press | Long Press |
|--------|-------------|------------|
| **MODE** KEY1 | Station list/Confirm | Close [ESC] |
| **SET** KEY2 | Equalizer list | Recording start/stop |
| **VOL+/-** KEY4/KEY3 | Menu navigation / Volume | Mute / Favorite |

## 🔧 Configuration Options

The project offers extensive configuration possibilities through `menuconfig`:

```bash
idf.py menuconfig
```

### Available options:
- **Display type:** SSD1306/SH1107
- **Resolution:** 128x64 
- **Button type:** GPIO 
- **Board variant:** ESP32-A1S 
- **SD card:** enable/disable
- **External keys:** optional

## 🌍 Internationalization

The project supports Polish diacritical marks with automatic UTF-8 to ASCII conversion for monochromatic displays.

## 🤝 Contributing

We welcome contributions to the project! 

1. **Fork** the repository
2. **Create a branch** for your feature
3. **Add changes** with descriptive commits
4. **Submit Pull Request**

## 📄 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **Espressif Systems** for ESP-IDF and ESP-ADF
- **LVGL** for the graphics library
- **Open-source community** for support and inspiration

## 📞 Contact

- **Author:** Jaromir K. Kopp (JKK)
- **GitHub:** [MacWyznawca](https://github.com/MacWyznawca)

---

**RadioJKK32** - Internet Radio with Browser Control 🎵