#include "api_client.h"

APIClient::APIClient(const char* host, int port) {
    baseUrl = String("http://") + host + ":" + String(port);
}

APIClient::~APIClient() {
    http.end();
}

bool APIClient::sendRequest(const char* method, const char* endpoint, const JsonDocument* body, JsonDocument* response) {
    String url = baseUrl + endpoint;
    http.begin(url);
    
    if (body) {
        String bodyStr;
        serializeJson(*body, bodyStr);
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.sendRequest(method, bodyStr);
        
        if (httpCode == HTTP_CODE_OK && response) {
            String payload = http.getString();
            deserializeJson(*response, payload);
        }
        
        http.end();
        return httpCode == HTTP_CODE_OK;
    } else {
        int httpCode = http.sendRequest(method);
        
        if (httpCode == HTTP_CODE_OK && response) {
            String payload = http.getString();
            deserializeJson(*response, payload);
        }
        
        http.end();
        return httpCode == HTTP_CODE_OK;
    }
}

bool APIClient::getTemperature(float& current, float& target) {
    JsonDocument doc;
    if (sendRequest("GET", "/api/temperature", nullptr, &doc)) {
        current = doc["temperature"];
        target = doc["target_temperature"];
        return true;
    }
    return false;
}

bool APIClient::setTemperature(float temperature) {
    JsonDocument doc;
    doc["temperature"] = temperature;
    return sendRequest("POST", "/api/temperature", &doc);
}

bool APIClient::getState(JsonDocument& doc) {
    return sendRequest("GET", "/api/state", nullptr, &doc);
}

bool APIClient::start() {
    return sendRequest("POST", "/api/start");
}

bool APIClient::abort() {
    return sendRequest("POST", "/api/abort");
}

bool APIClient::confirm() {
    return sendRequest("POST", "/api/confirm");
}

bool APIClient::getPreferences(JsonDocument& doc) {
    return sendRequest("GET", "/api/preferences", nullptr, &doc);
}

bool APIClient::setPreferences(const JsonDocument& doc) {
    return sendRequest("POST", "/api/preferences", &doc);
}

bool APIClient::waitTimer(unsigned long duration) {
    JsonDocument doc;
    doc["duration"] = duration;
    return sendRequest("POST", "/api/wait_timer", &doc);
}

bool APIClient::waitTemperature(float temperature) {
    JsonDocument doc;
    doc["temperature"] = temperature;
    return sendRequest("POST", "/api/wait_temperature", &doc);
}

bool APIClient::prepareRelative(float temperature, unsigned long minutes) {
    JsonDocument doc;
    doc["temperature"] = temperature;
    doc["minutes"] = minutes;
    return sendRequest("POST", "/api/prepare_relative", &doc);
}

bool APIClient::prepareAbsolute(float temperature, const char* time) {
    JsonDocument doc;
    doc["temperature"] = temperature;
    doc["time"] = time;
    return sendRequest("POST", "/api/prepare_absolute", &doc);
}

bool APIClient::setPID(float kp, float ki, float kd, float pOn, float sampleTime) {
    JsonDocument doc;
    doc["kp"] = kp;
    doc["ki"] = ki;
    doc["kd"] = kd;
    doc["pOn"] = pOn;
    doc["sampleTime"] = sampleTime;
    return sendRequest("POST", "/api/pid", &doc);
}

bool APIClient::getFile(const char* filename, String& content) {
    String endpoint = String("/api/file?filename=") + filename;
    JsonDocument doc;
    if (sendRequest("GET", endpoint.c_str(), nullptr, &doc)) {
        content = doc["content"].as<String>();
        return true;
    }
    return false;
}

bool APIClient::beep() {
    return sendRequest("POST", "/api/beep");
} 