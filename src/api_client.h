#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "state.h"
#include "Components/Settings.h"
#include "Controller/MainController.h"

class APIClient {
public:
    APIClient(const char* host, int port);
    ~APIClient();

    // Temperature endpoints
    bool getTemperature(float& current, float& target);
    bool setTemperature(float temperature);

    // State endpoints
    bool getState(JsonDocument& doc);
    bool start();
    bool abort();
    bool confirm();

    // Preferences endpoints
    bool getPreferences(JsonDocument& doc);
    bool setPreferences(const JsonDocument& doc);

    // Timer endpoints
    bool waitTimer(unsigned long duration);
    bool waitTemperature(float temperature);
    bool prepareRelative(float temperature, unsigned long minutes);
    bool prepareAbsolute(float temperature, const char* time);

    // PID endpoints
    bool setPID(float kp, float ki, float kd, float pOn, float sampleTime);

    // File endpoints
    bool getFile(const char* filename, String& content);

    // Beep endpoint
    bool beep();

private:
    HTTPClient http;
    String baseUrl;
    bool sendRequest(const char* method, const char* endpoint, const JsonDocument* body = nullptr, JsonDocument* response = nullptr);
};

#endif 