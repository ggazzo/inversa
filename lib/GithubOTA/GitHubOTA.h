#ifndef GITHUB_OTA_H
#define GITHUB_OTA_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

class GitHubOTA {
public:
    GitHubOTA(const char* repoOwner, const char* repoName, const char* currentVersion);
    bool checkForUpdate();
    bool performUpdate();
    String getLatestVersion();
    String getUpdateUrl();
    size_t getUpdateSize();
    void setUpdateCallback(void (*callback)(bool success));
    void setCheckInterval(unsigned long interval = 3600000); // Default 1 hour
    void loop();

private:
    String _repoOwner;
    String _repoName;
    String _currentVersion;
    String _latestVersion;
    String _updateUrl;
    size_t _updateSize;
    bool _updateAvailable;
    unsigned long _lastCheck;
    unsigned long _checkInterval;
    void (*_updateCallback)(bool success);
    
    bool _getLatestReleaseInfo();
    bool _downloadAndInstallUpdate();
};

#endif 