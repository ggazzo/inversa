#include <ArduinoOTA.h>
#include <GitHubOTA.h>
#include "definitions.h"
#include "OTA.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "semver.h"

#define RELEASE_URL "https://api.github.com/repos/ggazzo/inversa/releases/latest"
#define FIRMWARE_FILE_NAME FIRMWARE_NAME ".bin"

GitHubOTA OsOta(RELEASE_URL, BUILD_GIT_VERSION, FIRMWARE_FILE_NAME);

const unsigned long CHECK_INTERVAL = 1000 * 60 * 10; // Check every 10 minutes
void handleOTA() {
    static unsigned long lastCheck = millis() + CHECK_INTERVAL;
    
    if (millis() - lastCheck < CHECK_INTERVAL) {
        ArduinoOTA.handle();
        return;
    }
    
    Serial.println("[OTA] Checking for updates...");
    lastCheck = millis();
    
    HTTPClient http;
    http.begin("https://api.github.com/repos/ggazzo/inversa/releases/latest");
    http.addHeader("User-Agent", "ESP32");
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA] Failed to get release info. HTTP code: %d\n", httpCode);
        http.end();
        return;
    }
    
    String payload = http.getString();
    http.end();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.printf("[OTA] JSON parsing failed: %s\n", error.c_str());
        return;
    }
    
    const char* latestVersion = doc["tag_name"];
    if (!latestVersion) {
        Serial.println("[OTA] No version tag found in release");
        return;
    }

    Serial.printf("[OTA] Current version: %s, Latest version: %s\n", BUILD_GIT_VERSION, latestVersion);
    
    // Compare versions
    if (compareSemVer(latestVersion, BUILD_GIT_VERSION) <= 0) {
        Serial.println("[OTA] Already on latest version");
        return;
    }
    
    Serial.println("[OTA] New version available, checking assets...");
    
    // Find firmware asset
    String firmwareUrl;
    size_t firmwareSize = 0;
    
    JsonArray assets = doc["assets"];
    for (JsonObject asset : assets) {
        if (asset["name"].as<String>() == FIRMWARE_FILE_NAME) {
            firmwareUrl = asset["browser_download_url"].as<String>();
            firmwareSize = asset["size"].as<size_t>();
            Serial.printf("[OTA] Found firmware asset: %s (%d bytes)\n", firmwareUrl.c_str(), firmwareSize);
            break;
        }
    }
    
    if (firmwareUrl.length() == 0) {
        Serial.println("[OTA] No firmware asset found");
        return;
    }
    
    // Download and update firmware
    Serial.println("[OTA] Starting firmware download...");
    // handle 302 redirects
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setRedirectLimit(5);
    http.begin(firmwareUrl);
    httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA] Failed to download firmware. HTTP code: %d\n", httpCode);
        http.end();
        return;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("[OTA] Invalid content length");
        http.end();
        return;
    }
    
    Serial.printf("[OTA] Firmware size: %d bytes\n", contentLength);
    
    if (!Update.begin(contentLength)) {
        Serial.printf("[OTA] Not enough space for update. Free: %d, Required: %d\n", ESP.getFreeSketchSpace(), contentLength);
        http.end();
        return;
    }
    
    Serial.println("[OTA] Starting firmware update...");
    WiFiClient *stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    
    if (written == contentLength && Update.end()) {
        Serial.println("[OTA] Update successful, restarting...");
        ESP.restart();
    } else {
        Serial.printf("[OTA] Update failed. Written: %d/%d\n", written, contentLength);
    }
    
    http.end();
}

void setupOTA() {
    ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    // Serial.printf("Error[%u]: ", error);
    // if (error == OTA_AUTH_ERROR) {
    //   Serial.println("Auth Failed");
    // } else if (error == OTA_BEGIN_ERROR) {
    //   Serial.println("Begin Failed");
    // } else if (error == OTA_CONNECT_ERROR) {
    //   Serial.println("Connect Failed");
    // } else if (error == OTA_RECEIVE_ERROR) {
    //   Serial.println("Receive Failed");
    // } else if (error == OTA_END_ERROR) {
    //   Serial.println("End Failed");
    // }
  });

  #ifdef OTA_HOSTNAME
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  #endif
  #ifdef ESP32
  ArduinoOTA.begin();
  #else
  ArduinoOTA.begin(true);
  #endif
}