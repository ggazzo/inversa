#include <ArduinoOTA.h>
#include "definitions.h"
#include "OTA.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "semver.h"

#define RELEASE_URL "https://api.github.com/repos/ggazzo/inversa/releases/latest"
#define FIRMWARE_FILE_NAME FIRMWARE_NAME ".bin"

const unsigned long CHECK_INTERVAL = 1000 * 60 * 30; // Check every 5 minutes

OTA::OTA() : lastCheck(millis() + CHECK_INTERVAL) {}

void OTA::setup() {
  #ifdef OTA_HOSTNAME
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  #endif
  #ifdef ESP32
  ArduinoOTA.begin();
  #else
  ArduinoOTA.begin(true);
  #endif
}

void OTA::loop() {
    ArduinoOTA.handle();
    
    if (millis() - lastCheck < CHECK_INTERVAL) {
        return;
    }
    
    lastCheck = millis();
    checkForUpdatesGithub();
}

void OTA::checkForUpdatesGithub() {

    if(WiFi.status() != WL_CONNECTED){
        Serial.println("Not connected to WiFi");
        return;
    }

    WiFiClientSecure client;
    HTTPClient http;
    http.begin(client, RELEASE_URL);
    client.setInsecure();
    http.addHeader("User-Agent", "ESP32");

        int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("Failed to get release info. HTTP code: %d\n", httpCode);
        http.end();
        return;
    }
    
    String payload = http.getString();
    http.end();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.printf("JSON parsing failed: %s\n", error.c_str());
        return;
    }
    
    const char* latestVersion = doc["tag_name"];
    if (!latestVersion) {
        Serial.println("No version tag found in release");
        return;
    }


    Serial.printf("Latest version: %s, Current version: %s\n", latestVersion, BUILD_GIT_VERSION);
    
    // Compare versions
    if (compareSemVer(latestVersion, BUILD_GIT_VERSION) <= 0) {
        Serial.println("Already on latest version");
        return;
    }
    
    
    // Find firmware asset
    String firmwareUrl;
    size_t firmwareSize = 0;
    
    JsonArray assets = doc["assets"];
    for (JsonObject asset : assets) {
        if (asset["name"].as<String>() == FIRMWARE_FILE_NAME) {
            firmwareUrl = asset["browser_download_url"].as<String>();
            firmwareSize = asset["size"].as<size_t>();
            break;
        }
    }
    
    if (firmwareUrl.length() == 0) {
        Serial.println("No firmware asset found");
        return;
    }
    
    // Download and update firmware
    // handle 302 redirects
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setRedirectLimit(5);
    http.begin(client, firmwareUrl);
    client.setInsecure();
    httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("Failed to download firmware. HTTP code: %d\n", httpCode);
        http.end();
        return;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("Invalid content length");
        http.end();
        return;
    }
    
    
    if (!Update.begin(contentLength)) {
        Serial.println("Failed to begin update");
        http.end();
        return;
    }
    
    WiFiClient *stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    Serial.printf("Written: %d, Content Length: %d\n", written, contentLength);
    if (written == contentLength && Update.end()) {
        Serial.println("Update successful, restarting...");
        ESP.restart();
    }
    http.end();
}