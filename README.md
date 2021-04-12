# AmpliPi-Touchscreen-Keypad (WiFi)
A touchscreen keypad designed to control AmpliPi, using an ESP32 and WiFi.

For the POE-powered version, go to this repo: https://github.com/kjk2010/AmpliPi-POE-Touchscreen-Keypad

Currently a work in progress.

Built using VS Code with Platform.IO.

#### Currently supported hardware
- ESP32 dev board
- ILI9341 compatible 320x240 TFT touchscreen

#### Basic setup instructions
1. Wire the TFT touchscreen to the ESP32.
2. Clone this Github project
3. Edit the default pin selection in libdeps/esp32dev/TFT_eSPI/User_Setup.h

###### For an Olimex ESP32-PoE board, use these settings in User_Setup.h:
- TFT_MISO 16
- TFT_MOSI  5
- TFT_SCLK 14
- TFT_CS   15
- TFT_DC    2
- TFT_RST   4
- TOUCH_CS 13

![alt text](https://github.com/kjk2010/AmpliPi-Touchscreen-Keypad/blob/main/docs/ESP32-to-TFT-pin-assignment.jpg?raw=true)

4. Upload to ESP32
5. Upload Filesystem Image to ESP32 (this uploads the files in the data folder to the ESP32's file system)

Note: Some screens can't be reliably powered via the board's 3.3v pins and instead should be powered from 5v or an external power source.

#### To do items
- [x] Add WifiManager to configure Wifi AP settings, and move Wifi settings to config file
- [x] Add source selection screen
- [x] Add local source to source selection screen
- [x] Add settings screen: Change Zone, Change Source, Reset WiFi & AmpliPi Host, Reset Touchscreen
- [x] Move zone selection to settings screen and save to config file
- [x] Show album art for local inputs
- [x] Support coontrolling one or two zones
- [ ] Split display functions from update data functions. Display functions should be drawing everything from memory, and update functions should be updated the data and triggering a draw if data has changed.
- [ ] Add mDNS resolution support so touchscreen can find amplipi.local
- [ ] Research storing album art in RAM instead of file system
- [ ] Add support for stream commands: Play/Pause, Next, Stop, Like
- [ ] Add local inputs to source selection screen
- [x] Add POE and Ethernet capabilities, utilizing Olimex's ESP32-POE board - Repo for POE version: https://github.com/kjk2010/AmpliPi-POE-Touchscreen-Keypad
- [ ] Show configured names for local inputs
- [ ] Switch to using SSE or WebSockets, whichever AmpliPi server offers, instead of spamming API
- [ ] Screen time out options (PIR, touch, screensaver showing full screen only metadata?)
- [ ] Add AmpliPi preset functionality
- [ ] Put metadata into sprites and scroll long titles


#### Future features
- Add OTA upgrades
- Integrate Home Assistant control options on a second screen
- Add support for expansion units (more than 6 output zones)
