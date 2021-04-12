#include <Arduino.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <ESPmDNS.h>

String findMDNS(String mDnsHost) {
    // the input mDnsHost is e.g. "mqtt-broker" from "mqtt-broker.local"
    Serial.println("Finding the mDNS details...");
    // Need to make sure that we're connected to the wifi first
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
    }

    uint64_t chipid = ESP.getEfuseMac();
    char chipid_char[16];
    int n = sprintf(chipid_char, "%16X", chipid);

    if (!MDNS.begin(chipid_char)) {
        Serial.println("Error setting up MDNS responder!");
    } else {
        Serial.println("Finished intitializing the MDNS client...");
    }

    Serial.println("mDNS responder started");
    IPAddress serverIp = MDNS.queryHost(mDnsHost);
    while (serverIp.toString() == "0.0.0.0") {
        Serial.println("Trying again to resolve mDNS");
        delay(250);
        serverIp = MDNS.queryHost(mDnsHost);
    }
    Serial.print("IP address of server: ");
    Serial.println(serverIp.toString());
    Serial.println("Finished finding the mDNS details...");
    return serverIp.toString();
}