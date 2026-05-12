#pragma once
// WiFi.h stub — the simulator does not pretend to be networked. All API
// calls succeed superficially but disconnected==true; nothing connects.

#include "Arduino.h"

enum WiFiEvent_t      { ARDUINO_EVENT_WIFI_STA_GOT_IP, ARDUINO_EVENT_WIFI_STA_DISCONNECTED, ARDUINO_EVENT_WIFI_STA_CONNECTED };
typedef int           WiFiEventInfo_t;
constexpr int         WIFI_STA      = 1;
constexpr int         WL_CONNECTED  = 3;

class IPAddress {
public:
    IPAddress() = default;
    String toString() const { return String("0.0.0.0"); }
};

class WiFiClass {
public:
    void mode(int)                                 {}
    void setAutoReconnect(bool)                    {}
    void begin(const char*, const char*)           {}
    void disconnect(bool /*eraseCreds*/ = false)   {}
    int  status()                                  { return 0; }  // never connected
    IPAddress localIP()                            { return {}; }
    String SSID()                                  { return String(""); }
    template <typename F> void onEvent(F&&)        {}
};
extern WiFiClass WiFi;
