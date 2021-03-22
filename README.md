# AmpliPi-Touchscreen-Keypad
A touchscreen keypad designed to control AmpliPi.

Currently a work in progress.

Built using VS Code with Platform.IO.

#### Currently supported hardware
- ESP32 dev board
- ILI9341 compatible 320x240 TFT touchscreen

#### Basic setup instructions
1. Wire the TFT touchscreen to the ESP32. Default pin selection below, but can be changed in libdeps/esp32dev/TFT_eSPI/User_Setup.h
- TFT_MISO 19
- TFT_MOSI 23
- TFT_SCLK 18
- TFT_CS   15  // Chip select control pin
- TFT_DC    2  // Data Command control pin
- TFT_RST   4  // Reset pin
- TOUCH_CS 21  // Chip select pin (T_CS) of touch screen
![alt text](https://github.com/kjk2010/AmpliPi-Touchscreen-Keypad/blob/main/docs/ESP32-to-TFT-pin-assignment.jpg?raw=true)

2. Clone this Github project
3. Configure Wifi settings and AmpliPi host IP in src/Keypad-Touchscreen.cpp
4. Upload to ESP32
5. Upload Filesystem Image to ESP32 (this uploads files in the data folder to the ESP32's file system)


#### To do items
- [ ] Add WifiManager to configure Wifi AP settings, and move Wifi settings to config file
- [ ] Add source selection screen
- [ ] Add settings screen
- [ ] Move zone selection to settings screen and save to config file. Include support for expansion units (more than 6 output zones)
- [ ] Add preset functionality


#### Future features
- Integrate Home Assistant control options
