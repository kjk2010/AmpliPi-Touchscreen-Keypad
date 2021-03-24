// Touchscreen Keypad Controller for AmpliPi
// Designed to run on an ESP32

#include <Arduino.h>
#include <stdint.h>
#include "FS.h"
#include "SPIFFS.h"
#include "Free_Fonts.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
WiFiMulti wifiMulti;

#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <HTTPClient.h>
#include <ArduinoJson.h>


/* Debug options */
#define DEBUGAPIREQ false


/**************************************/
/* Configure screen colors and layout */
/**************************************/
// Colors
#define GREY 0x5AEB
#define BLUE 0x9DFF

// Source bar location
#define SRCBAR_X 0
#define SRCBAR_Y 0
#define SRCBAR_W 240
#define SRCBAR_H 36

// Main area, normally where metadata is shown
#define MAINZONE_X 0
#define MAINZONE_Y 36
#define MAINZONE_W 240
#define MAINZONE_H 220

// Album art location
#define ALBUMART_X 60
#define ALBUMART_Y 36

// Warning zone
#define WARNZONE_X 0
#define WARNZONE_Y 260
#define WARNZONE_W 240
#define WARNZONE_H 20

// Mute button location
#define MUTE_X 0
#define MUTE_Y 280
#define MUTE_W 36
#define MUTE_H 36

// Volume control bar position and size
#define VOLBAR_X 45
#define VOLBAR_Y 295
#define VOLBAR_W 150
#define VOLBAR_H 6

// Volume control bar zone size
#define VOLBARZONE_X 40
#define VOLBARZONE_Y 280
#define VOLBARZONE_W 160
#define VOLBARZONE_H 40


/******************************/
/* Configure system variables */
/******************************/
TFT_eSPI tft = TFT_eSPI(); // Invoke TFT display library

// This is the file name used to store the touch coordinate
// calibration data. Cahnge the name to start a new calibration.
#define CALIBRATION_FILE "/TouchCalData"

// Set REPEAT_CAL to true instead of false to run calibration
// again, otherwise it will only be done once.
// Repeat calibration if you change the screen rotation.
#define REPEAT_CAL false


/******************************/
/* Configure WiFiManager */
/******************************/
// Must have ESPAsync_WiFiManager v1.6.0 or greater
#define ESP_ASYNC_WIFIMANAGER_VERSION_MIN_TARGET     "ESPAsync_WiFiManager v1.6.0"

// Use from 0 to 4. Higher number, more debugging messages and memory usage.
#define _ESPASYNC_WIFIMGR_LOGLEVEL_    3

// For ESPAsync_WiFiManager:
#define USE_LITTLEFS      false
#define USE_SPIFFS        true
#define FileFS            SPIFFS
#define FS_Name           "SPIFFS"
#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())
#define LED_BUILTIN       2
#define LED_ON            HIGH
#define LED_OFF           LOW
#define PIN_LED           LED_BUILTIN

char configFileName[] = "/config.json";

String Router_SSID;
String Router_Pass;

#define MIN_AP_PASSWORD_SIZE    8
#define SSID_MAX_LEN            32
#define PASS_MAX_LEN            64

typedef struct
{
  char wifi_ssid[SSID_MAX_LEN];
  char wifi_pw  [PASS_MAX_LEN];
}  WiFi_Credentials;

typedef struct
{
  String wifi_ssid;
  String wifi_pw;
}  WiFi_Credentials_String;

#define NUM_WIFI_CREDENTIALS      2

typedef struct
{
  WiFi_Credentials  WiFi_Creds [NUM_WIFI_CREDENTIALS];
} WM_Config;

WM_Config         WM_config;

#define  CONFIG_FILENAME              F("/wifi_cred.dat")

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;

// SSID and PW for Config Portal
String AP_SSID;
String AP_PASS;

// Use false to disable NTP config. Advisable when using Cellphone, Tablet to access Config Portal.
// See Issue 23: On Android phone ConfigPortal is unresponsive (https://github.com/khoih-prog/ESP_WiFiManager/issues/23)
#define USE_ESP_WIFIMANAGER_NTP     false

// Use true to enable CloudFlare NTP service. System can hang if you don't have Internet access while accessing CloudFlare
// See Issue #21: CloudFlare link in the default portal (https://github.com/khoih-prog/ESP_WiFiManager/issues/21)
#define USE_CLOUDFLARE_NTP          false

// New in v1.0.11
#define USING_CORS_FEATURE          true

#include <ESPAsync_WiFiManager.h>              //https://github.com/khoih-prog/ESPAsync_WiFiManager

#define HTTP_PORT           80

#define AMPLIPIHOST_LEN     64
#define AMPLIPIZONE_LEN     6
char amplipiHost [AMPLIPIHOST_LEN] = "amplipi.local"; // Default settings
char amplipiZone1 [AMPLIPIZONE_LEN] = "0";
char amplipiZone2 [AMPLIPIZONE_LEN] = "null";


/******************************/
/* Configure system variables */
/******************************/
String sourceName = "";
String currentArtist = "";
String currentSong = "";
String currentStatus = "";
String currentAlbumArt = "";
bool inWarning = false;
int amplipiSource = 0;
int amplipiZone = 0;
bool updateAlbumart = true;
bool updateSource = false;
bool updateMute = true;
bool mute = false;
float volPercent = 100;
bool updateVol = true;
bool metadata_refresh = true;


/***********************/
/* Configure functions */
/***********************/

// WiFiManager functions

///////////////////////////////////////////

uint8_t connectMultiWiFi()
{
#define WIFI_MULTI_1ST_CONNECT_WAITING_MS           0L
#define WIFI_MULTI_CONNECT_WAITING_MS               100L
  
  uint8_t status;

  LOGERROR(F("ConnectMultiWiFi with :"));
  
  if ( (Router_SSID != "") && (Router_Pass != "") )
  {
    LOGERROR3(F("* Flash-stored Router_SSID = "), Router_SSID, F(", Router_Pass = "), Router_Pass );
  }

  for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
  {
    // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
    if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
    {
      LOGERROR3(F("* Additional SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
    }
  }
  
  LOGERROR(F("Connecting MultiWifi..."));

  WiFi.mode(WIFI_STA);

  int i = 0;
  status = wifiMulti.run();
  delay(WIFI_MULTI_1ST_CONNECT_WAITING_MS);

  while ( ( i++ < 20 ) && ( status != WL_CONNECTED ) )
  {
    status = wifiMulti.run();

    if ( status == WL_CONNECTED )
      break;
    else
      delay(WIFI_MULTI_CONNECT_WAITING_MS);
  }

  if ( status == WL_CONNECTED )
  {
    LOGERROR1(F("WiFi connected after time: "), i);
    LOGERROR3(F("SSID:"), WiFi.SSID(), F(",RSSI="), WiFi.RSSI());
    LOGERROR3(F("Channel:"), WiFi.channel(), F(",IP address:"), WiFi.localIP() );
  }
  else
    LOGERROR(F("WiFi not connected"));

  return status;
}

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println(F("Should save config"));
  shouldSaveConfig = true;
}

bool loadFileFSConfigFile()
{
  //clean FS, for testing
  //FileFS.format();

  //read configuration from FS json
  Serial.println(F("Mounting FS..."));

  if (FileFS.begin())
  {
    Serial.println(F("Mounted file system"));

    if (FileFS.exists(configFileName))
    {
      //file exists, reading and loading
      Serial.println(F("Reading config file"));
      File configFile = FileFS.open(configFileName, "r");

      if (configFile)
      {
        Serial.print(F("Opened config file, size = "));
        size_t configFileSize = configFile.size();
        Serial.println(configFileSize);

        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[configFileSize + 1]);

        configFile.readBytes(buf.get(), configFileSize);

        Serial.print(F("\nJSON parseObject() result : "));

        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get(), configFileSize);

        if ( deserializeError )
        {
          Serial.println(F("failed"));
          return false;
        }
        else
        {
          Serial.println(F("OK"));

          if (json["amplipiHost"])
            strncpy(amplipiHost,  json["amplipiHost"],  sizeof(amplipiHost));
         
          if (json["amplipiZone1"])
            strncpy(amplipiZone1, json["amplipiZone1"], sizeof(amplipiZone1));
 
          if (json["amplipiZone2"])
            strncpy(amplipiZone2, json["amplipiZone2"], sizeof(amplipiZone2));
        }

        //serializeJson(json, Serial);
        serializeJsonPretty(json, Serial);

        configFile.close();
      }
    }
  }
  else
  {
    Serial.println(F("failed to mount FS"));
    return false;
  }
  return true;
}

bool saveFileFSConfigFile()
{
  Serial.println(F("Saving config"));

  DynamicJsonDocument json(1024);

  json["amplipiHost"]  = amplipiHost;
  json["amplipiZone1"] = amplipiZone1;
  json["amplipiZone2"] = amplipiZone2;

  File configFile = FileFS.open(configFileName, "w");

  if (!configFile)
  {
    Serial.println(F("Failed to open config file for writing"));

    return false;
  }

  serializeJsonPretty(json, Serial);
  // Write data to file and close it
  serializeJson(json, configFile);

  configFile.close();
  //end save

  return true;
}

void toggleLED()
{
  //toggle state
  digitalWrite(PIN_LED, !digitalRead(PIN_LED));
}

void heartBeatPrint()
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED)
    Serial.print(F("H"));        // H means connected to WiFi
  else
    Serial.print(F("F"));        // F means not connected to WiFi

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(F(" "));
  }
}

void check_WiFi()
{
  if ( (WiFi.status() != WL_CONNECTED) )
  {
    Serial.println(F("\nWiFi lost. Calling connectMultiWiFi in loop"));
    connectMultiWiFi();
  }
}  

void check_status()
{
  static ulong checkstatus_timeout  = 0;
  static ulong LEDstatus_timeout    = 0;
  static ulong checkwifi_timeout    = 0;

  static ulong current_millis;

#define WIFICHECK_INTERVAL    1000L
#define LED_INTERVAL          2000L
#define HEARTBEAT_INTERVAL    10000L

  current_millis = millis();
  
  // Check WiFi every WIFICHECK_INTERVAL (1) seconds.
  if ((current_millis > checkwifi_timeout) || (checkwifi_timeout == 0))
  {
    check_WiFi();
    checkwifi_timeout = current_millis + WIFICHECK_INTERVAL;
  }

  if ((current_millis > LEDstatus_timeout) || (LEDstatus_timeout == 0))
  {
    // Toggle LED at LED_INTERVAL = 2s
    toggleLED();
    LEDstatus_timeout = current_millis + LED_INTERVAL;
  }

  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((current_millis > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = current_millis + HEARTBEAT_INTERVAL;
  }
}

bool loadConfigData()
{

  if (FileFS.exists(CONFIG_FILENAME))
  {
    File file = FileFS.open(CONFIG_FILENAME, "r");
    LOGERROR(F("LoadWiFiCfgFile "));

    memset(&WM_config,       0, sizeof(WM_config));

    if (file)
    {
        file.readBytes((char *) &WM_config,   sizeof(WM_config));
        file.close();
        LOGERROR(F("OK"));
        return true;
    }
    else {
        return false;
    }
  }
  else
  {
    LOGERROR(F("failed"));

    return false;
  }
}
    
void saveConfigData()
{
  File file = FileFS.open(CONFIG_FILENAME, "w");
  LOGERROR(F("SaveWiFiCfgFile "));

  if (file)
  {
    file.write((uint8_t*) &WM_config,   sizeof(WM_config));
    file.close();
    LOGERROR(F("OK"));
  }
  else
  {
    LOGERROR(F("failed"));
  }
}


// Function to handle touchscreen calibration. Calibration only runs once, unless 
//  TouchCalData file is removed or REPEAT_CAL is set to true
void touch_calibrate()
{
    uint16_t calData[5];
    uint8_t calDataOK = 0;

    // check file system exists
    if (!SPIFFS.begin())
    {
        Serial.println("Formatting file system");
        SPIFFS.format();
        SPIFFS.begin();
    }

    // check if calibration file exists and size is correct
    if (SPIFFS.exists(CALIBRATION_FILE))
    {
        if (REPEAT_CAL)
        {
            // Delete if we want to re-calibrate
            SPIFFS.remove(CALIBRATION_FILE);
        }
        else
        {
            File f = SPIFFS.open(CALIBRATION_FILE, "r");
            if (f)
            {
                if (f.readBytes((char *)calData, 14) == 14)
                    calDataOK = 1;
                f.close();
            }
        }
    }

    if (calDataOK && !REPEAT_CAL)
    {
        // calibration data valid
        tft.setTouch(calData);
    }
    else
    {
        // data not valid so recalibrate
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(20, 20);
        tft.setFreeFont(FSS12);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        tft.println("Touch corners as");
        tft.println("indicated");

        //tft.setTextFont(1);
        tft.println();

        if (REPEAT_CAL)
        {
            //tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setFreeFont(FSS9);
            tft.println("Set REPEAT_CAL to false to stop this running again!");
        }

        tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

        //tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("Calibration complete!");

        // store data
        File f = SPIFFS.open(CALIBRATION_FILE, "w");
        if (f)
        {
            f.write((const unsigned char *)calData, 14);
            f.close();
        }
    }
}

// Function used for processing BMP images
uint16_t read16(fs::File &f)
{
    uint16_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read(); // MSB
    return result;
}

// Function used for processing BMP images
uint32_t read32(fs::File &f)
{
    uint32_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read();
    ((uint8_t *)&result)[2] = f.read();
    ((uint8_t *)&result)[3] = f.read(); // MSB
    return result;
}


// Clear the main area of the screen. Generally metadata is shown here, but also source select and settings
void clearMainArea()
{
    tft.fillRect(MAINZONE_X, MAINZONE_Y, MAINZONE_W, MAINZONE_H, TFT_BLACK); // Clear metadata area
}


// Show a warning near bottom of screen. Primarily used if we can't access AmpliPi API
void drawWarning(String message)
{
    tft.fillRect(WARNZONE_X, WARNZONE_Y, WARNZONE_W, WARNZONE_H, TFT_BLACK); // Clear warning area
    tft.setCursor(5, WARNZONE_Y, 2);
    Serial.print("Warning: ");
    Serial.println(message);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setFreeFont(FSS9);
    tft.println(message);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont(FSS12);
    inWarning = true;
}

void clearWarning()
{
    tft.fillRect(WARNZONE_X, WARNZONE_Y, WARNZONE_W, WARNZONE_H, TFT_BLACK); // Clear warning area
    inWarning = false;
}


// Show the album art file on screen
void drawBmp(const char *filename, int16_t x, int16_t y)
{

    if ((x >= tft.width()) || (y >= tft.height()))
        return;

    fs::File bmpFS;

    // Open requested file on SD card
    bmpFS = SPIFFS.open(filename, "r");

    if (!bmpFS)
    {
        Serial.print("File not found");
        return;
    }

    uint32_t seekOffset;
    uint16_t w, h, row;
    uint8_t r, g, b;

    uint32_t startTime = millis();

    if (read16(bmpFS) == 0x4D42)
    {
        read32(bmpFS);
        read32(bmpFS);
        seekOffset = read32(bmpFS);
        read32(bmpFS);
        w = read32(bmpFS);
        h = read32(bmpFS);

        if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0))
        {
            y += h - 1;

            bool oldSwapBytes = tft.getSwapBytes();
            tft.setSwapBytes(true);
            bmpFS.seek(seekOffset);

            uint16_t padding = (4 - ((w * 3) & 3)) & 3;
            uint8_t lineBuffer[w * 3 + padding];

            for (row = 0; row < h; row++)
            {

                bmpFS.read(lineBuffer, sizeof(lineBuffer));
                uint8_t *bptr = lineBuffer;
                uint16_t *tptr = (uint16_t *)lineBuffer;
                // Convert 24 to 16 bit colours
                for (uint16_t col = 0; col < w; col++)
                {
                    b = *bptr++;
                    g = *bptr++;
                    r = *bptr++;
                    *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                }

                // Push the pixel row to screen, pushImage will crop the line if needed
                // y is decremented as the BMP image is drawn bottom up
                tft.pushImage(x, y--, w, 1, (uint16_t *)lineBuffer);
            }
            tft.setSwapBytes(oldSwapBytes);
            Serial.print("Loaded in ");
            Serial.print(millis() - startTime);
            Serial.println(" ms");
        }
        else
            Serial.println("BMP format not recognized.");
    }
    bmpFS.close();
}


// Download album art or logo from AmpliPi API. Requires code update to AmpliPi API
bool downloadAlbumart(String streamID)
{
    HTTPClient http;
    bool outcome = true;
    String url = "http://" + String(amplipiHost) + "/api/streams/image/" + streamID;
    String filename = "/albumart.bmp";

    // configure server and url
    http.begin(url);

    // start connection and send HTTP header
    int httpCode = http.GET();

#if DEBUGAPIREQ
    Serial.println(("[HTTP] GET DONE with code " + String(httpCode)));
#endif

    if (httpCode > 0)
    {
        Serial.println(F("-- >> OPENING FILE..."));

        SPIFFS.remove(filename);
        fs::File f = SPIFFS.open(filename, "w+");
        if (!f)
        {
            Serial.println(F("file open failed"));
            return false;
        }

#if DEBUGAPIREQ
        // HTTP header has been sent and Server response header has been handled
        Serial.printf("-[HTTP] GET... code: %d\n", httpCode);
        Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
#endif

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            // get lenght of document (is -1 when Server sends no Content-Length header)
            int total = http.getSize();
            int len = total;

#if DEBUGAPIREQ
            Serial.println("HTTP SIZE IS " + String(total));
#endif

            // create buffer for read
            uint8_t buff[128] = {0};

            // get tcp stream
            WiFiClient *stream = http.getStreamPtr();

            // read all data from server
            while (http.connected() && (len > 0 || len == -1))
            {
                // get available data size
                size_t size = stream->available();

                if (size)
                {
                    // read up to 128 byte
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

                    // write it to SPIFFS
                    f.write(buff, c);

                    if (len > 0)
                    {
                        len -= c;
                    }
                }
                delay(1);
            }

#if DEBUGAPIREQ
            Serial.println("[HTTP] connection closed or file end.");
#endif
        }
        f.close();
    }
    else
    {
        Serial.println("[HTTP] GET... failed, error: " + http.errorToString(httpCode));

        drawWarning("Unable to access AmpliPi");
        outcome = false;
    }
    http.end();
    return outcome;
}


// Show downloaded album art of screen
void drawAlbumart()
{
    // Only update album art on screen if we need
    if (!updateAlbumart)
    {
        return;
    };
    tft.fillRect(ALBUMART_X, ALBUMART_Y, 120, 120, TFT_BLACK); // Clear album art first
    drawBmp("/albumart.bmp", ALBUMART_X, ALBUMART_Y);
    updateAlbumart = false;
}


// Show the current source on screen, top left (by default)
void drawSource()
{
    // Only update source on screen if we need
    if (!updateSource)
    {
        return;
    };

    tft.setFreeFont(FSS9);
    tft.setTextDatum(TL_DATUM);
    tft.fillRect(SRCBAR_X, SRCBAR_Y, SRCBAR_W, SRCBAR_H, TFT_BLACK); // Clear source bar first
    tft.drawString(sourceName, 2, 5, GFXFF); // Top Left
    drawBmp("/source.bmp", (SRCBAR_W - 36), SRCBAR_Y);

    updateSource = false;
}


void drawSourceSelection()
{
    // Download source options


    // Stop metadata refresh
    metadata_refresh = false;

    // Clear screen
    clearMainArea();

    // Show options


}


void selectSource()
{
    // Send source selection (or cancel?)


    // Clear screen
    clearMainArea();

    // Restart metadata refresh
    metadata_refresh = true;
}


void drawSettings()
{
    // Available settings:
    // - Select zone, zones, or group to manage
    // - Reset WiFi settings (delete 'wifi.json' config file and reboot, or enable web server to select AP)
    // - Restart
    // - Show version, Wifi AP, IP address

    // Stop metadata refresh
    metadata_refresh = false;

    // Clear screen
    clearMainArea();
    
    // Show settings


}


void saveSettings()
{
    // Save settings


    // Clear screen
    clearMainArea();

    // Reboot if requested


    // Restart metadata refresh
    metadata_refresh = true;
}


// API Request to Amplipi
String requestAPI(String request)
{
    HTTPClient http;

    String url = "http://" + String(amplipiHost) + "/api/" + request;
    String payload;

#if DEBUGAPIREQ
    Serial.print("[HTTP] begin...\n");
    Serial.print("[HTTP] GET...\n");
#endif
    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    http.begin(url); //HTTP

    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0)
    {
#if DEBUGAPIREQ
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
#endif

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            payload = http.getString();

#if DEBUGAPIREQ
            Serial.println(payload);
#endif

            // Clear the warning since we jsut received a successful API request
            if (inWarning)
            {
                clearWarning();
            }
        }
    }
    else
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        drawWarning("Unable to access AmpliPi");
    }

    http.end();

    return payload;
}


// Send PATCH to API
bool patchAPI(String request, String payload)
{
    HTTPClient http;

    bool result = false;

    String url = "http://" + String(amplipiHost) + "/api/" + request;

#if DEBUGAPIREQ
    Serial.print("[HTTP] begin...\n");
    Serial.print("[HTTP] PATCH...\n");
#endif

    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    http.begin(url); //HTTP
    http.addHeader("Accept", "application/json");
    http.addHeader("Content-Type", "application/json");

    // start connection and send HTTP header
    int httpCode = http.PATCH(payload);

    // httpCode will be negative on error
    if (httpCode > 0)
    {
#if DEBUGAPIREQ
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] PATCH... code: %d\n", httpCode);
#endif

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            result = true;

            // Clear the warning since we jsut received a successful API request
            if (inWarning)
            {
                clearWarning();
            }
        }
        String resultPayload = http.getString();

#if DEBUGAPIREQ
        Serial.println("[HTTP] PATCH result:");
        Serial.println(resultPayload);
#endif
    }
    else
    {
        Serial.printf("[HTTP] PATCH... failed, error: %s\n", http.errorToString(httpCode).c_str());
        drawWarning("Unable to access AmpliPi");
    }

    http.end();

    return result;
}


void sendVolUpdate()
{
    // Send volume update to API, but only one update per second so we don't spam it
    int volDb = (int)(volPercent * 0.79 - 79); // Convert to proper AmpliPi number (-79 to 0)
    String payload = "{\"vol\": " + String(volDb) + "}";
    bool result = patchAPI("zones/" + String(amplipiZone), payload);
    Serial.print("sendVolUpdate result: ");
    Serial.println(result);
}

void drawVolume(int x)
{
    if (!updateVol)
    {
        return;
    }; // Only update volume on screen if we need to

    if (x > 185)
    {
        x = 200;
    } // Bring to 100% if it's close

    tft.fillRect(VOLBARZONE_X, VOLBARZONE_Y, 320, VOLBARZONE_H, TFT_BLACK); // Clear area first

    volPercent = (x - 40) / 1.6;
    Serial.print("New Volume Level: ");
    Serial.println(volPercent);

    int barWidth = VOLBAR_W * (volPercent / 100);
    Serial.print("New barWidth: ");
    Serial.println(barWidth);
    Serial.print("New x: ");
    Serial.println(x);

    // Volume control bar
    tft.fillRect(VOLBAR_X, VOLBAR_Y, VOLBAR_W, VOLBAR_H, GREY);       // Grey bar
    tft.fillRect(VOLBAR_X, VOLBAR_Y, (x - VOLBAR_X), VOLBAR_H, BLUE); // Blue active bar
    tft.fillCircle(x, (VOLBAR_Y + 2), 8, BLUE);                       // Circle marker

    updateVol = false;
}


void sendMuteUpdate()
{
    // Send volume update to API, but only one update per second so we don't spam it
    String payload;

    if (mute) {
        payload = "{\"mute\": true}";
    }
    else {
        payload = "{\"mute\": false}";
    }
    bool result = patchAPI("zones/" + String(amplipiZone), payload);
    Serial.print("sendMuteUpdate result: ");
    Serial.println(result);
}

void drawMuteBtn()
{
    if (!updateMute)
    {
        return;
    }; // Only update mute button on screen if we need to

    tft.fillRect(MUTE_X, MUTE_Y, MUTE_W, MUTE_H, TFT_BLACK); // Clear area first

    // Mute/unmute button
    if (mute)
    {
        drawBmp("/volume_off.bmp", MUTE_X, MUTE_Y);
    }
    else
    {
        drawBmp("/volume_up.bmp", MUTE_X, MUTE_Y);
    }

    updateMute = false;
}


// Split a string by a separator
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data.charAt(i) == separator || i == maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


void getZone()
{
    String json = requestAPI("zones/" + String(amplipiZone));

    // DynamicJsonDocument<N> allocates memory on the heap
    DynamicJsonDocument ampSourceStatus(1000);

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(ampSourceStatus, json);

    // Test if parsing succeeds.
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
    }

    bool currentMute = ampSourceStatus["mute"];
    mute = currentMute;
    drawMuteBtn();

    int currentVol = ampSourceStatus["mute"];
    if (currentVol < 0) {
        volPercent = currentVol / 0.79 + 100; // Convert from AmpliPi number (-79 to 0) to percent
    }
    else {
        volPercent = 100;
    }
    drawVolume(int(volPercent * 1.6)); // Multiply by 1.6 to give it the x coord

}


String getSource(int sourceID)
{
    String json = requestAPI("sources/" + String(sourceID));

    // DynamicJsonDocument<N> allocates memory on the heap
    DynamicJsonDocument ampSourceStatus(1000);

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(ampSourceStatus, json);

    // Test if parsing succeeds.
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());

        String errormsg = "Error parsing results from Amplipi API";
        //tft.println(errormsg);
        return errormsg;
    }

    //String newSourceName = ampSourceStatus["name"];
    String sourceInput = ampSourceStatus["input"];
    Serial.print("sourceInput: ");
    Serial.println(sourceInput);
    String streamID = "";

    if (sourceInput == "local")
    {
        streamID = "0";
    }
    else
    {
        streamID = getValue(sourceInput, '=', 1);
    }

    Serial.print("streamID: ");
    Serial.println(streamID);
    return streamID;
}


void getStream(int sourceID)
{
    String displaySong;
    String displayArtist;
    String streamID = getSource(sourceID);

    String json = requestAPI("streams/" + streamID);

    // DynamicJsonDocument<N> allocates memory on the heap
    DynamicJsonDocument ampSourceStatus(2000);

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(ampSourceStatus, json);

    // Test if parsing succeeds.
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());

        String errormsg = "Error parsing results from Amplipi API";
        //tft.println(errormsg);
        return;
    }

    String streamArtist = ampSourceStatus["info"]["artist"];
    if (streamArtist == "null")
    {
        streamArtist = "";
    }
    String streamAlbum = ampSourceStatus["info"]["album"];
    if (streamAlbum == "null")
    {
        streamAlbum = "";
    }
    String streamSong = ampSourceStatus["info"]["song"];
    if (streamSong == "null")
    {
        streamSong = "";
    }
    String streamStatus = ampSourceStatus["status"];
    if (streamStatus == "null")
    {
        streamStatus = "";
    }
    String streamName = ampSourceStatus["name"];
    if (streamName == "null")
    {
        streamName = "";
    }
    String albumArt = ampSourceStatus["info"]["img_url"];
    if (albumArt == "null")
    {
        albumArt = "";
    }

    // Only refresh screen if we have new data
    if (currentArtist != streamArtist || currentSong != streamSong || currentStatus != streamStatus)
    {
        if (streamSong.length() >= 19) {
            displaySong = streamSong.substring(0,18) + "...";
        }
        else {
            displaySong = streamSong;
        }
        
        if (streamArtist.length() >= 19) {
            displayArtist = streamArtist.substring(0,18) + "...";
        }
        else {
            displayArtist = streamArtist;
        }

        Serial.println("Refreshing metadata on screen");

        tft.setTextDatum(TC_DATUM);
        tft.setFreeFont(FSS12);
        //tft.setCursor(3, 120, 2);
        tft.fillRect(0, 157, 320, 125, TFT_BLACK); // Clear metadata area first
        tft.drawString(displaySong, 120, 165, GFXFF); // Center Middle
        tft.fillRect(20, 192, 200, 1, GREY);         // Seperator between song and artist
        tft.drawString(displayArtist, 120, 200, GFXFF);
        
        //TODO:
        //tft.setFreeFont(FSS9);
        //tft.println(streamAlbum);

        currentArtist = streamArtist;
        currentSong = streamSong;
        currentStatus = streamStatus;
    }

    // Download and refresh album art if it has changed
    if (albumArt != currentAlbumArt)
    {
        currentAlbumArt = albumArt;
        updateAlbumart = true;
        downloadAlbumart(streamID);
        drawAlbumart();
    }

    // Update source name if it has changed
    if (streamName != sourceName)
    {
        sourceName = streamName;
        updateSource = true;
        drawSource();
    }
}

//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
void setup(void)
{
    Serial.begin(115200);
    Serial.println("System Startup");

    // Initialize screen
    tft.init();

    // Set the rotation before we calibrate
    tft.setRotation(0);

    // Wrap test at right and bottom of screen
    tft.setTextWrap(true, true);

    tft.setFreeFont(FSS18);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Call screen calibration
    //  This also handles formatting the filesystem if it hasn't been formatted yet
    touch_calibrate();

    // clear screen
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(60, 40, 2);
    tft.setFreeFont(FSS18);

    tft.print("Ampli");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Pi");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont(FSS12);
    tft.setCursor(70, 100, 2);
    tft.println("Welcome");
    tft.println("");

    // Load config file from WifiManager. This gives up our Wifi settings and AmpliPi settings (if they've been set).
    //  Otherwise, it starts the web server so the settings can be configured.
    loadFileFSConfigFile();
    ESPAsync_WMParameter custom_amplipiHost ("amplipiHost",  "amplipiHost",  amplipiHost,  AMPLIPIHOST_LEN + 1);
    ESPAsync_WMParameter custom_amplipiZone1("amplipiZone1", "amplipiZone1", amplipiZone1, AMPLIPIZONE_LEN + 1);
    ESPAsync_WMParameter custom_amplipiZone2("amplipiZone2", "amplipiZone2", amplipiZone2, AMPLIPIZONE_LEN + 1 );
    unsigned long startedAt = millis();
    
    //Local intialization. Once its business is done, there is no need to keep it around
    // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
    //ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer);
    // Use this to personalize DHCP hostname (RFC952 conformed)
    AsyncWebServer webServer(HTTP_PORT);

    DNSServer dnsServer;
  
    ESPAsync_WiFiManager ESPAsync_wifiManager(&webServer, &dnsServer, "AutoConnect-FSParams");
    
    //set config save notify callback
    ESPAsync_wifiManager.setSaveConfigCallback(saveConfigCallback);

    //add all your parameters here
    ESPAsync_wifiManager.addParameter(&custom_amplipiHost);
    ESPAsync_wifiManager.addParameter(&custom_amplipiZone1);
    ESPAsync_wifiManager.addParameter(&custom_amplipiZone2);

    //reset settings - for testing
    //ESPAsync_wifiManager.resetSettings();

    ESPAsync_wifiManager.setDebugOutput(true);

    //set minimum quality of signal so it ignores AP's under that quality
    //defaults to 8%
    //ESPAsync_wifiManager.setMinimumSignalQuality();
    ESPAsync_wifiManager.setMinimumSignalQuality(-1);

    // From v1.0.10 only
    // Set config portal channel, default = 1. Use 0 => random channel from 1-13
    ESPAsync_wifiManager.setConfigPortalChannel(0);
    //////

#if USING_CORS_FEATURE
    ESPAsync_wifiManager.setCORSHeader("Access-Control-Allow-Origin *");
#endif

    // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
    // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
    // Have to create a new function to store in EEPROM/SPIFFS/LittleFS for this purpose
    Router_SSID = ESPAsync_wifiManager.WiFi_SSID();
    Router_Pass = ESPAsync_wifiManager.WiFi_Pass();

    //Remove this line if you do not want to see WiFi password printed
    Serial.println("Connecting to self-stored SSID: " + Router_SSID);

    bool configDataLoaded = false;

    String chipID = String(ESP_getChipId(), HEX);
    chipID.toUpperCase();

    // SSID and PW for Config Portal
    AP_SSID = chipID + "_AutoConnectAP";
    AP_PASS = "amplipi";
    
    // Don't permit NULL password
    if ( (Router_SSID != "") && (Router_Pass != "") )
    {
        LOGERROR3(F("* Add SSID = "), Router_SSID, F(", PW = "), Router_Pass);
        wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());

        ESPAsync_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
        Serial.println(F("Got ESP Self-Stored Credentials. Timeout 120s for Config Portal"));
    }
    else if (loadConfigData())
    {
        configDataLoaded = true;

        ESPAsync_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
        Serial.println(F("Got stored Credentials. Timeout 120s for Config Portal")); 
    }
    else
    {
        // Enter CP only if no stored SSID on flash and file 
        Serial.println(F("Open Config Portal without Timeout: No stored Credentials."));
        initialConfig = true;
        tft.println("Configure WiFi by");
        tft.println("connecting to:");
        tft.setFreeFont(FSS9);
        tft.println("");
        tft.print("SSID: ");
        tft.println(AP_SSID.c_str());
        tft.print("Pass: ");
        tft.println(AP_PASS.c_str());
    }

    if (initialConfig)
    {
        Serial.println(F("We don't have any access point credentials, so get them now"));

        // Starts an access point
        //if (!ESPAsync_wifiManager.startConfigPortal((const char *) ssid.c_str(), password))
        if ( !ESPAsync_wifiManager.startConfigPortal(AP_SSID.c_str(), AP_PASS.c_str()) )
        {
            Serial.println(F("Not connected to WiFi but continuing anyway."));
        }
        else
        {
            Serial.println(F("WiFi connected...yeey :)"));
            tft.println("Connecting to WiFi...");
        }

        // Stored  for later usage, from v1.1.0, but clear first
        memset(&WM_config, 0, sizeof(WM_config));
        
        for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
        {
            String tempSSID = ESPAsync_wifiManager.getSSID(i);
            String tempPW   = ESPAsync_wifiManager.getPW(i);
        
            if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1)
                strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
            else
                strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);

            if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1)
                strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
            else
                strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);  

            // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
            if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
            {
                LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
                wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
            }
        }

        saveConfigData();
    }
    else
    {
        wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());
    }

    startedAt = millis();

    if (!initialConfig)
    {
        // Load stored data, the addAP ready for MultiWiFi reconnection
        if (!configDataLoaded)
        loadConfigData();

        for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
        {
            // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
            if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
            {
                LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
                wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
            }
        }

        if ( WiFi.status() != WL_CONNECTED ) 
        {
            Serial.println(F("ConnectMultiWiFi in setup"));
        
            connectMultiWiFi();
        }
    }

    Serial.print(F("After waiting "));
    Serial.print((float) (millis() - startedAt) / 1000L);
    Serial.print(F(" secs more in setup(), connection result is "));

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print(F("connected. Local IP: "));
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println(ESPAsync_wifiManager.getStatus(WiFi.status()));
    }

    //read updated parameters
    strncpy(amplipiHost, custom_amplipiHost.getValue(), sizeof(amplipiHost));
    strncpy(amplipiZone1, custom_amplipiZone1.getValue(), sizeof(amplipiZone1));
    strncpy(amplipiZone2, custom_amplipiZone2.getValue(), sizeof(amplipiZone2));

    //save the custom parameters to FS
    if (shouldSaveConfig)
    {
        saveFileFSConfigFile();
    }


    // Clear screen
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 20, 2);


    // Show AmpliPi keypad screen
    drawMuteBtn();
    drawVolume(200);
    //getStream(amplipiSource);
}
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
void loop()
{
    uint16_t x, y;
    
    // WiFiManager status check
    check_status();

    // See if there's any touch data for us
    if (tft.getTouch(&x, &y))
    {
        // Draw a block spot to show where touch was calculated to be
        //tft.fillCircle(x, y, 2, TFT_BLUE);

        // Touch screen control

        // Mute button
        if ((x > MUTE_X) && (x < (MUTE_X + MUTE_W)))
        {
            if ((y > MUTE_Y) && (y <= (MUTE_Y + MUTE_H)))
            {
                if (mute)
                {
                    mute = false;
                }
                else
                {
                    mute = true;
                }
                updateMute = true;
                drawMuteBtn();
                sendMuteUpdate();
                Serial.print("Mute button hit.");
                delay(100); // Debounce
            }
        }

        // Volume control
        if ((x > VOLBARZONE_X) && (x < (VOLBARZONE_X + VOLBARZONE_W)))
        {
            if ((y > VOLBARZONE_Y) && (y <= (VOLBARZONE_Y + VOLBARZONE_H)))
            {
                updateVol = true;
                drawVolume(x);
                sendVolUpdate();
                Serial.print("Volume control hit.");
                delay(100); // Debounce
            }
        }
    }

    static const unsigned long REFRESH_INTERVAL = 5000; // ms
    static unsigned long lastRefreshTime = 0; // Start now, then reset to 0

    if (millis() - lastRefreshTime >= REFRESH_INTERVAL)
    {
        if (metadata_refresh) {
            Serial.println("Refreshing metadata");
            lastRefreshTime += REFRESH_INTERVAL;
            getStream(amplipiSource);
            getZone();
        }
    }

}
//------------------------------------------------------------------------------------------
