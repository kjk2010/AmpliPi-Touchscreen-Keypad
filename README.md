# AmpliPi-Touchscreen-Keypad
A touchscreen keypad designed to control AmpliPi.

Currently a work in progress.

Built using VS Code with Platform.IO.

#### Currently supported hardware
- ESP32 dev board
- ILI9341 compatible 320x240 TFT touchscreen

#### Basic setup instructions
1. Wire the TFT touchscreen to the ESP32. Default pin selection below, but can be changed in libdeps/esp32dev/TFT_eSPI/User_Setup.h
###### For a standard ESP32 dev board, use these settings:
- TFT_MISO 19
- TFT_MOSI 23
- TFT_SCLK 18
- TFT_CS   15
- TFT_DC    2
- TFT_RST   4
- TOUCH_CS 21
![alt text](https://github.com/kjk2010/AmpliPi-Touchscreen-Keypad/blob/main/docs/ESP32-to-TFT-pin-assignment.jpg?raw=true)

###### For an Olimex ESP32-POE board, use these settings:
- TFT_MISO 16
- TFT_MOSI 36
- TFT_SCLK 14
- TFT_CS   15
- TFT_DC    2
- TFT_RST   4
- TOUCH_CS 13

2. Clone this Github project
3. Configure Wifi settings and AmpliPi host IP in src/Keypad-Touchscreen.cpp
4. Upload to ESP32
5. Upload Filesystem Image to ESP32 (this uploads files in the data folder to the ESP32's file system)


#### To do items
- [x] Add WifiManager to configure Wifi AP settings, and move Wifi settings to config file
- [x] Add source selection screen
- [x] Add local source to source selection screen
- [ ] Add settings screen
- [ ] Move zone selection to settings screen and save to config file. Include support for expansion units (more than 6 output zones)
- [ ] Add POE and Ethernet capabilities, utilizing Olimex's ESP32-POE board
- [ ] Show configured names and add album art for local inputs
- [ ] Switch to using SSE or WebSockets, whichever AmpliPi server offers, instead of spamming API
- [ ] Screen time out options (PIR, touch, screensaver showing full screen only metadata?)
- [ ] Add AmpliPi preset functionality
- [ ] Put metadata into sprites and scroll them left and right


#### Future features
- Add OTA upgrades
- Integrate Home Assistant control options on a second screen
