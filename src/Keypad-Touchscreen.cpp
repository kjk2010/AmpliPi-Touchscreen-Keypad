// Touchscreen Keypad Controller for AmpliPi
// Designed to run on an ESP32

#include <Arduino.h>
#include <stdint.h>
#include "FS.h"
#include "SPIFFS.h" // For ESP32 only
#include "Free_Fonts.h"
#include <WiFi.h>
#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <HTTPClient.h>
#include <ArduinoJson.h>


// Wifi connection configuration
const char *ssid = "";
const char *password = "";

// AmpliPi connection configuration
const String amplipiHost = "192.168.2.136";
const int amplipiSource = 0;
const int amplipiZone = 0;


TFT_eSPI tft = TFT_eSPI(); // Invoke TFT display library

// This is the file name used to store the touch coordinate
// calibration data. Cahnge the name to start a new calibration.
#define CALIBRATION_FILE "/TouchCalData"

// Set REPEAT_CAL to true instead of false to run calibration
// again, otherwise it will only be done once.
// Repeat calibration if you change the screen rotation.
#define REPEAT_CAL false

// Colors
#define GREY 0x5AEB
#define BLUE 0x9DFF

// Album art location
#define ALBUMART_X 60
#define ALBUMART_Y 30

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

String sourceName = "";
String currentArtist = "";
String currentSong = "";
String currentStatus = "";
String currentAlbumArt = "";
bool updateAlbumart = true;
bool updateSource = false;
bool updateMute = true;
bool mute = false;
float volPercent = 100;
bool updateVol = true;


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

uint16_t read16(fs::File &f)
{
    uint16_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read(); // MSB
    return result;
}

uint32_t read32(fs::File &f)
{
    uint32_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read();
    ((uint8_t *)&result)[2] = f.read();
    ((uint8_t *)&result)[3] = f.read(); // MSB
    return result;
}

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
    String url = "http://" + amplipiHost + "/api/streams/image/" + streamID;
    String filename = "/albumart.bmp";

    // configure server and url
    http.begin(url);

    // start connection and send HTTP header
    int httpCode = http.GET();

    Serial.println(("[HTTP] GET DONE with code " + String(httpCode)));

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

        // HTTP header has been sent and Server response header has been handled
        Serial.printf("-[HTTP] GET... code: %d\n", httpCode);
        Serial.println("Free Heap: " + String(ESP.getFreeHeap()));

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            // get lenght of document (is -1 when Server sends no Content-Length header)
            int total = http.getSize();
            int len = total;

            Serial.println("HTTP SIZE IS " + String(total));

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

            Serial.println("[HTTP] connection closed or file end.");
        }
        f.close();
    }
    else
    {
        Serial.println("[HTTP] GET... failed, error: " + http.errorToString(httpCode));
        outcome = false;
    }
    http.end();
    return outcome;
}


void drawAlbumart()
{
    if (!updateAlbumart)
    {
        return;
    }; // Only update album art on screen if we need to
    drawBmp("/albumart.bmp", ALBUMART_X, ALBUMART_Y);
    updateAlbumart = false;
}


void drawSource()
{
    if (!updateSource)
    {
        return;
    }; // Only update source on screen if we need to

    tft.fillRect(0, 0, 320, ALBUMART_Y, TFT_BLACK); // Clear metadata area first
    tft.setFreeFont(FSS9);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(sourceName, 2, 3, GFXFF); // Top Left

    updateSource = false;
}


// API Request to Amplipi
String requestAPI(String request)
{
    HTTPClient http;

    String url = "http://" + amplipiHost + "/api/" + request;
    String payload;

    Serial.print("[HTTP] begin...\n");
    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    http.begin(url); //HTTP

    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0)
    {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            payload = http.getString();
            Serial.println(payload);
        }
    }
    else
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();

    return payload;
}


// Send PATCH to API
bool patchAPI(String request, String payload)
{
    HTTPClient http;

    bool result = false;

    String url = "http://" + amplipiHost + "/api/" + request;

    Serial.print("[HTTP] begin...\n");
    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    http.begin(url); //HTTP
    http.addHeader("Accept", "application/json");
    http.addHeader("Content-Type", "application/json");

    Serial.print("[HTTP] PATCH...\n");
    // start connection and send HTTP header
    int httpCode = http.PATCH(payload);

    // httpCode will be negative on error
    if (httpCode > 0)
    {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] PATCH... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            result = true;
        }
        String resultPayload = http.getString();
        Serial.println("[HTTP] PATCH result:");
        Serial.println(resultPayload);
    }
    else
    {
        Serial.printf("[HTTP] PATCH... failed, error: %s\n", http.errorToString(httpCode).c_str());
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
        tft.setCursor(3, 120, 2);
        tft.fillRect(0, 120, 320, 130, TFT_BLACK); // Clear metadata area first
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

    if (albumArt != currentAlbumArt)
    {
        currentAlbumArt = albumArt;
        updateAlbumart = true;
        downloadAlbumart(streamID);
        drawAlbumart();
    }

    
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
    Serial.println("Startup");
    tft.init();

    // Set the rotation before we calibrate
    tft.setRotation(0);

    // Wrap test at right and bottom of screen
    tft.setTextWrap(true, true);

    tft.setFreeFont(FSS12);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // call screen calibration
    touch_calibrate();

    // clear screen
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(0, 20, 2);

    Serial.print("Connecting to ");
    Serial.println(ssid);
    tft.println("Welcome to AmpliPi");
    tft.println("");
    tft.println("Connecting to WiFi...");

    // Wifi setup
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(1000);
    }

    // Clear screen
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 20, 2);

    tft.println("WiFi connected");

    // Clear screen
    tft.fillScreen(TFT_BLACK);
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    drawMuteBtn();
    drawVolume(200);
    //getStream(amplipiSource);
}
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
void loop()
{
    uint16_t x, y;

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
        Serial.println("Refreshing metadata");
        lastRefreshTime += REFRESH_INTERVAL;
        getStream(amplipiSource);
        getZone();
    }
}
//------------------------------------------------------------------------------------------
