#pragma once
// Preferences.h stub — in-memory NVS for the simulator.

#include <unordered_map>
#include <variant>
#include "Arduino.h"

class Preferences {
public:
    bool begin(const char* /*ns*/, bool /*ro*/ = false) { return true; }
    void end() {}
    void clear() { _store.clear(); }

    void remove(const char* key) { _store.erase(key ? key : ""); }
    bool isKey (const char* key) { return _store.find(key ? key : "") != _store.end(); }

    void putFloat (const char* k, float v)        { _store[k] = Value{.f = v, .kind = K::F}; }
    void putInt   (const char* k, int32_t v)      { _store[k] = Value{.i = v, .kind = K::I}; }
    void putUInt  (const char* k, uint32_t v)     { _store[k] = Value{.u = v, .kind = K::U}; }
    void putUChar (const char* k, uint8_t v)      { _store[k] = Value{.uc = v, .kind = K::UC}; }
    void putBool  (const char* k, bool v)         { _store[k] = Value{.b = v, .kind = K::B}; }
    void putString(const char* k, const String& v){ _store[k] = Value{.s = std::string(v.c_str()), .kind = K::S}; }

    float    getFloat (const char* k, float    def) { auto it=_store.find(k); return it!=_store.end()&&it->second.kind==K::F? it->second.f : def; }
    int32_t  getInt   (const char* k, int32_t  def) { auto it=_store.find(k); return it!=_store.end()&&it->second.kind==K::I? it->second.i : def; }
    uint32_t getUInt  (const char* k, uint32_t def) { auto it=_store.find(k); return it!=_store.end()&&it->second.kind==K::U? it->second.u : def; }
    uint8_t  getUChar (const char* k, uint8_t  def) { auto it=_store.find(k); return it!=_store.end()&&it->second.kind==K::UC? it->second.uc : def; }
    bool     getBool  (const char* k, bool     def) { auto it=_store.find(k); return it!=_store.end()&&it->second.kind==K::B? it->second.b : def; }
    String  getString(const char* k, const String& def) {
        auto it = _store.find(k);
        return it != _store.end() && it->second.kind == K::S ? String(it->second.s) : def;
    }

private:
    enum class K { F, I, U, UC, B, S };
    struct Value {
        float    f  = 0;
        int32_t  i  = 0;
        uint32_t u  = 0;
        uint8_t  uc = 0;
        bool     b  = false;
        std::string s;
        K kind = K::F;
    };
    std::unordered_map<std::string, Value> _store;
};
