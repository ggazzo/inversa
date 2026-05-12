#pragma once
#include "Arduino.h"
#include "WiFiClientSecure.h"

constexpr int HTTP_CODE_OK              = 200;
constexpr int HTTPC_STRICT_FOLLOW_REDIRECTS = 0;

class HTTPClient {
public:
    bool begin(WiFiClientSecure&, const String&) { return false; }
    void addHeader(const char*, const char*) {}
    void setTimeout(int) {}
    void setFollowRedirects(int) {}
    void setRedirectLimit(int) {}
    int  GET()         { return 0; }
    int  getSize()     { return 0; }
    String getString() { return String(""); }
    WiFiClient* getStreamPtr() { return nullptr; }
    bool connected()   { return false; }
    void end()         {}
};
