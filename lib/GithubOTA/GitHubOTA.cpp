#include "GitHubOTA.h"

GitHubOTA::GitHubOTA(const char* repoOwner, const char* repoName, const char* currentVersion) {
    _repoOwner = repoOwner;
    _repoName = repoName;
    _currentVersion = currentVersion;
    _updateAvailable = false;
    _updateSize = 0;
}

bool GitHubOTA::checkForUpdate() {
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
    String url = "https://api.github.com/repos/" + _repoOwner + "/" + _repoName + "/releases/latest";
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload);
        
        _latestVersion = doc["tag_name"].as<String>();
        _updateUrl = doc["assets"][0]["browser_download_url"].as<String>();
        _updateSize = doc["assets"][0]["size"].as<size_t>();
        
        // Remove 'v' prefix if exists for version comparison
        String currentVer = _currentVersion;
        String latestVer = _latestVersion;
        if (currentVer.startsWith("v")) currentVer = currentVer.substring(1);
        if (latestVer.startsWith("v")) latestVer = latestVer.substring(1);
        
        _updateAvailable = (currentVer != latestVer);
        http.end();
        return true;
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