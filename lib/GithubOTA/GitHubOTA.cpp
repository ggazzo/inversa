#include "GitHubOTA.h"
#include "semver.h"

GitHubOTA::GitHubOTA(const char* url, const char* currentVersion, const char* firmwareName) {
    _url = url;
    _currentVersion = currentVersion;
    _firmwareName = firmwareName;
    _updateAvailable = false;
    _updateSize = 0;
}

bool GitHubOTA::checkForUpdate() {
    Serial.println("Checking for update");
    if (_getLatestReleaseInfo()) {
        return _updateAvailable;
    }
    return false;
}

bool GitHubOTA::performUpdate() {
    if (!_updateAvailable) {
        return false;
    }
    return _downloadAndInstallUpdate();
}

String GitHubOTA::getLatestVersion() {
    return _latestVersion;
}

String GitHubOTA::getUpdateUrl() {
    return _updateUrl;
}

size_t GitHubOTA::getUpdateSize() {
    return _updateSize;
}

bool GitHubOTA::_getLatestReleaseInfo() {
    HTTPClient http;
    Serial.println("Getting latest release info");
    http.begin(_url);
    int httpCode = http.GET();
    Serial.println("HTTP code: " + String(httpCode));
    if (httpCode == HTTP_CODE_OK) {
        Serial.println("Getting payload");
        String payload = http.getString();
        Serial.println("Payload: " + payload);
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload);
        
        _latestVersion = doc["tag_name"].as<String>();
        
        // Remove 'v' prefix if exists for version comparison
        String currentVer = _currentVersion;
        String latestVer = _latestVersion;
        if (currentVer.startsWith("v")) currentVer = currentVer.substring(1);
        if (latestVer.startsWith("v")) latestVer = latestVer.substring(1);

        bool versionIsGreater = compareSemVer(currentVer.c_str(), latestVer.c_str()) == -1;

        _updateAvailable = false;
        if(versionIsGreater) {

            for (JsonObject asset : doc["assets"].as<JsonArray>()) {
                if (asset["name"].as<String>() == _firmwareName) {
                    _updateUrl = asset["browser_download_url"].as<String>();
                    _updateSize = asset["size"].as<size_t>();
                    _updateAvailable = true;
                    break;
                }
            }

        }

        http.end();
        return _updateAvailable;
    }
    
    http.end();
    return false;
}

bool GitHubOTA::_downloadAndInstallUpdate() {
    HTTPClient http;
    http.begin(_updateUrl);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        http.end();
        return false;
    }
    
    if (!Update.begin(contentLength)) {
        http.end();
        return false;
    }
    
    WiFiClient * stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    
    if (written == contentLength && Update.end()) {
        http.end();
        return true;
    }
    
    http.end();
    return false;
} 