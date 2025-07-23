# Changelog

All notable changes to RadioJKK32 project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2025-07-23

### Added
- Web interface for station management with drag & drop reordering
- Equalizer selection through web interface
- Station backup and export functionality
- Automatic station backup system
- Remote station backup download via web interface
- Support for station reordering with NVS persistence
- Enhanced web UI with responsive design for mobile devices
- Drop zone for easier station list management
- Bin files based on ESP-IDF 5.5 and current ESP-ADF

### Changed
- Improved web interface layout with better spacing and alignment
- Enhanced error handling in web requests
- Cleaned up JavaScript console logging for production use
- Updated comments in code to English for better internationalization
- Maximum number of stations increased by 50
- Maximum number of equalizers increased to 20
- Optymized for ESP-IDF 5.5

### Fixed
- Station reordering functionality now properly updates indices
- Equalizer list loading in web interface
- Mobile responsiveness for iPhone and Android devices
- Proper synchronization between LCD display and web interface

## [0.9.0] - 2025-07-01

### Added
- ESP32-A1S audio kit support with custom board definition
- Multi-format audio decoding (MP3, AAC, OGG, WAV, FLAC, OPUS, M4A)
- Internet radio streaming with HTTP/HTTPS support
- I2C OLED display support (SSD1306/SH1107)
- 10-band equalizer with preset management
- SD card recording in AAC format
- Station management with favorites support
- NVS (Non-Volatile Storage) for persistent settings
- WiFi provisioning with SoftAP fallback
- Volume control with mute functionality
- Real-time audio level meter
- SNTP time synchronization
- mDNS service discovery
- Web server for remote control
- Button controls with short/long press support
- Drag & drop station reordering in web interface
- Audio pipeline with resampling and processing
- Station backup and restore functionality

### Hardware Support
- AI Thinker ESP32-A1S Audio Kit (variants 5 and 7)
- ES8388 audio codec
- I2C OLED displays (128x64 SSD1306, 64x128 SH1107)
- SD card support (1-line mode)
- GPIO buttons or ADC buttons
- External amplifier control
- Headphone detection support

### Audio Features
- Sample rates: 8kHz to 48kHz
- Bit depths: 16-bit, 24-bit
- Channels: Mono, Stereo
- Audio formats: MP3, AAC, OGG, WAV, FLAC, OPUS, M4A, AMR-NB, AMR-WB
- Real-time equalizer with 10 bands
- Volume control (0-100%)
- Audio recording to SD card in AAC format
- Automatic gain control
- Audio level monitoring

### Network Features
- WiFi STA mode with WPA/WPA2 support
- WiFi provisioning via ESP SoftAP
- HTTP/HTTPS streaming support
- mDNS hostname: RadioJKK.local
- Web interface on port 80
- SNTP time synchronization
- Playlist support (M3U, PLS)
- Automatic reconnection handling

### Storage Features
- NVS for configuration persistence
- SD card for audio recording
- Station list import/export
- Equalizer preset management
- Automatic backup system
- Configuration file support (settings.txt, stations.txt, eq.txt)

### User Interface
- Physical buttons for basic control
- I2C OLED display with real-time information
- Web interface for advanced management
- Station drag & drop reordering
- Real-time audio level display
- Responsive design for mobile devices

### Configuration Files
- `settings.txt`: WiFi credentials and web server settings
- `stations.txt`: Radio station list in CSV format
- `eq.txt`: Equalizer presets configuration

### API Endpoints
- `GET /`: Main web interface
- `GET /status`: Current status (volume, station, equalizer)
- `GET /station_list`: List of all stations
- `GET /eq_list`: List of equalizer presets
- `POST /volume`: Set volume level
- `POST /station_select`: Select radio station
- `POST /station_edit`: Add/edit station
- `POST /station_delete`: Delete station
- `POST /station_reorder`: Reorder stations
- `POST /eq_select`: Select equalizer preset
- `GET /backup_stations`: Download station backup

---

## Version Numbering

This project uses [Semantic Versioning](https://semver.org/):
- **MAJOR**: Incompatible API changes or major feature overhauls
- **MINOR**: New functionality added in backwards compatible manner
- **PATCH**: Backwards compatible bug fixes

## Contributing

When contributing to this project, please:
1. Update the CHANGELOG.md with your changes
2. Follow the existing format and categories
3. Add entries to the "Unreleased" section
4. Move entries to a version section when releasing

## Categories

- **Added**: New features
- **Changed**: Changes in existing functionality  
- **Deprecated**: Soon-to-be removed features
- **Removed**: Now removed features
- **Fixed**: Bug fixes
- **Security**: Vulnerability fixes