#pragma once
// NimBLEDevice.h stub — provides enough surface area for BLEPlugin.h to
// compile against. The simulator overrides BLEPlugin's `send()` to go to
// stdout instead of a notify characteristic (see main_sim.cpp). The real
// callback chain inside this header is no-op.

#include "Arduino.h"

class NimBLEServer;
class NimBLECharacteristic;
class NimBLEAdvertising;
class NimBLEService;

struct NimBLEAddress { String toString() const { return String("00:00:00:00:00:00"); } };
struct NimBLEConnInfo { NimBLEAddress getAddress() const { return {}; } };

class NimBLECharacteristicCallbacks {
public:
    virtual ~NimBLECharacteristicCallbacks() = default;
    virtual void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) {}
};
class NimBLEServerCallbacks {
public:
    virtual ~NimBLEServerCallbacks() = default;
    virtual void onConnect(NimBLEServer*, NimBLEConnInfo&) {}
    virtual void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int /*reason*/) {}
};

namespace NIMBLE_PROPERTY {
    constexpr int NOTIFY   = 1;
    constexpr int WRITE    = 2;
    constexpr int WRITE_NR = 4;
}

class NimBLECharacteristic {
public:
    void setValue(const uint8_t*, size_t) {}
    void notify() {}
    void setCallbacks(NimBLECharacteristicCallbacks*) {}
    std::string getValue() { return std::string(); }
};

class NimBLEService {
public:
    NimBLECharacteristic* createCharacteristic(const char* /*uuid*/, int /*props*/) {
        return &_dummy;
    }
    void start() {}
private:
    NimBLECharacteristic _dummy;
};

class NimBLEAdvertising {
public:
    void addServiceUUID(const char*) {}
    void setName(const char*) {}
    void start() {}
};

class NimBLEServer {
public:
    void setCallbacks(NimBLEServerCallbacks*) {}
    NimBLEService* createService(const char* /*uuid*/) { return &_svc; }
    int  getConnectedCount() { return _connected ? 1 : 0; }
    void simConnect()    { _connected = true; }
    void simDisconnect() { _connected = false; }
private:
    NimBLEService _svc;
    bool _connected = false;
};

class NimBLEDevice {
public:
    static void init(const char*) {}
    static void setMTU(int) {}
    static int  getMTU()    { return 512; }
    static NimBLEServer*      createServer()    { static NimBLEServer s;     return &s; }
    static NimBLEAdvertising* getAdvertising()  { static NimBLEAdvertising a; return &a; }
};
