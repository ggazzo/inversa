// test_mocks.h — Mock Arduino types for native unit tests
#pragma once

#ifdef NATIVE_BUILD

#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>

// Mock Arduino String class
class String {
public:
    String() : _str("") {}
    String(const char* s) : _str(s ? s : "") {}
    String(const String& s) : _str(s._str) {}
    String(int val) : _str(std::to_string(val)) {}
    String(float val) : _str(std::to_string(val)) {}
    
    const char* c_str() const { return _str.c_str(); }
    size_t length() const { return _str.length(); }
    bool isEmpty() const { return _str.empty(); }
    
    String& operator=(const char* s) { _str = s ? s : ""; return *this; }
    String& operator=(const String& s) { _str = s._str; return *this; }
    String& operator+=(const char* s) { _str += (s ? s : ""); return *this; }
    String& operator+=(const String& s) { _str += s._str; return *this; }
    String operator+(const char* s) const { return String((_str + (s ? s : "")).c_str()); }
    String operator+(const String& s) const { return String((_str + s._str).c_str()); }
    
    bool operator==(const char* s) const { return _str == (s ? s : ""); }
    bool operator==(const String& s) const { return _str == s._str; }
    bool operator!=(const char* s) const { return !(*this == s); }
    bool operator!=(const String& s) const { return !(*this == s); }
    
    char charAt(unsigned int i) const { return i < _str.length() ? _str[i] : 0; }
    int indexOf(char c, int from = 0) const { 
        auto pos = _str.find(c, from);
        return pos == std::string::npos ? -1 : (int)pos;
    }
    int indexOf(const char* s, int from = 0) const { 
        auto pos = _str.find(s, from);
        return pos == std::string::npos ? -1 : (int)pos;
    }
    
    String substring(int from) const { return String(_str.substr(from).c_str()); }
    String substring(int from, int to) const { return String(_str.substr(from, to - from).c_str()); }
    
    void trim() {
        size_t start = _str.find_first_not_of(" \t\r\n");
        size_t end = _str.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) _str = "";
        else _str = _str.substr(start, end - start + 1);
    }
    
    void toUpperCase() {
        for (auto& c : _str) c = toupper(c);
    }
    
    bool startsWith(const char* prefix) const {
        return _str.find(prefix) == 0;
    }
    
    bool endsWith(const char* suffix) const {
        size_t slen = strlen(suffix);
        return _str.length() >= slen && _str.substr(_str.length() - slen) == suffix;
    }
    
    float toFloat() const { 
        try { return std::stof(_str); } 
        catch (...) { return 0.0f; }
    }
    int toInt() const { 
        try { return std::stoi(_str); }
        catch (...) { return 0; }
    }

private:
    std::string _str;
};

// Mock millis()
inline uint32_t millis() {
    static uint32_t t = 0;
    return t += 100;  // Simulate time passing
}

// Mock delay()
inline void delay(uint32_t) {}

// Mock constrain
template<typename T>
inline T constrain(T x, T a, T b) {
    return (x < a) ? a : ((x > b) ? b : x);
}

// Mock Serial
class MockSerial {
public:
    void begin(int) {}
    void println(const char*) {}
    void println() {}
    void print(const char*) {}
    void printf(const char*, ...) {}
};
extern MockSerial Serial;

#endif // NATIVE_BUILD
