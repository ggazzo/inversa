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

void checkForUpdatesGithub() {
  HTTPClient http;
    http.begin("https://api.github.com/repos/ggazzo/inversa/releases/latest");
    http.addHeader("User-Agent", "ESP32");
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return;
    }
    
    String payload = http.getString();
    http.end();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        return;
    }
    
    const char* latestVersion = doc["tag_name"];
    if (!latestVersion) {
        return;
    }

    
    // Compare versions
    if (compareSemVer(latestVersion, BUILD_GIT_VERSION) <= 0) {
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
        return;
    }
    
    // Download and update firmware
    // handle 302 redirects
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setRedirectLimit(5);
    http.begin(firmwareUrl);
    httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        http.end();
        return;
    }
    
    
    if (!Update.begin(contentLength)) {
        http.end();
        return;
    }
    
    WiFiClient *stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    
    http.end();
    if (written == contentLength && Update.end()) {
        ESP.restart();
    }
    
}
void handleOTA(bool checkForUpdates) {
    static unsigned long lastCheck = millis() + CHECK_INTERVAL;
    ArduinoOTA.handle();
    
    if (millis() - lastCheck < CHECK_INTERVAL) {
        return;
    }
    
    lastCheck = millis();
    checkForUpdatesGithub();
}

void setupOTA() {
  #ifdef OTA_HOSTNAME
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  #endif
  #ifdef ESP32
  ArduinoOTA.begin();
  #else
  ArduinoOTA.begin(true);
  #endif
}