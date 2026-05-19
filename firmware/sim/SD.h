#pragma once
// SD.h stub — in-memory FAT-like filesystem for the simulator.
// Backs `firmware/sim/sim_fs` so recipe files can be pre-loaded from disk
// at startup (see main_sim.cpp::loadHostFile).

#include "Arduino.h"
#include "SPI.h"
#include <unordered_map>
#include <vector>
#include <cstring>

constexpr int FILE_READ   = 1;
constexpr int FILE_WRITE  = 2;
constexpr int FILE_APPEND = 3;

class File {
public:
    File() = default;
    File(std::vector<uint8_t>* data, int mode)
        : _data(data), _mode(mode), _valid(true) {
        if (mode == FILE_WRITE && data) data->clear();
    }
    // A File is "truthy" if it is either a regular file with a backing
    // buffer OR a directory iterator. Testing `_data` alone made
    // `SD.open("/recipes")` look closed to SDCardPlugin::listRecipes,
    // which short-circuited and returned an empty list in the sim.
    explicit operator bool() const { return _valid && (_data || _isDir); }

    size_t size() const { return _data ? _data->size() : 0; }

    String readString() {
        if (!_data) return String("");
        std::string s(_data->begin(), _data->end());
        return String(s);
    }
    size_t read(uint8_t* out, size_t n) {
        if (!_data) return 0;
        size_t left = _data->size() > _pos ? _data->size() - _pos : 0;
        size_t take = n < left ? n : left;
        std::memcpy(out, _data->data() + _pos, take);
        _pos += take;
        return take;
    }
    size_t write(const uint8_t* buf, size_t n) {
        if (!_data) return 0;
        _data->insert(_data->end(), buf, buf + n);
        return n;
    }
    size_t print(const String& s) {
        return write(reinterpret_cast<const uint8_t*>(s.c_str()), s.length());
    }
    void close() { _data = nullptr; _valid = false; }

    // Directory traversal — SDCardPlugin uses this to list /recipes/*.txt.
    bool isDirectory() const { return _isDir; }
    File openNextFile() {
        if (!_isDir || _entries.empty() || _entryIdx >= _entries.size()) return File();
        File f(_entries[_entryIdx].second, FILE_READ);
        f._name = _entries[_entryIdx].first;
        _entryIdx++;
        return f;
    }
    const char* name() const { return _name.c_str(); }

    // Internals exposed for the SDClass below:
    void setDirectoryEntries(std::vector<std::pair<std::string, std::vector<uint8_t>*>> e) {
        // _valid must flip with the directory marker. The previous
        // default-constructed File had _valid=false, so listRecipes'
        // `if (!dir || !dir.isDirectory())` short-circuited even when
        // entries were present.
        _valid = true; _isDir = true; _entries = std::move(e);
    }

private:
    std::vector<uint8_t>* _data = nullptr;
    int    _mode  = FILE_READ;
    size_t _pos   = 0;
    bool   _valid = false;
    bool   _isDir = false;
    std::string _name;
    std::vector<std::pair<std::string, std::vector<uint8_t>*>> _entries;
    size_t _entryIdx = 0;
};

class SDClass {
public:
    bool begin(int /*cs*/) { return true; }

    bool exists(const char* path)    { return _files.count(path) > 0 || isDir(path); }
    bool exists(const String& path)  { return exists(path.c_str()); }
    bool mkdir(const char* /*path*/) { return true; }    // dirs are implicit
    bool remove(const char* path)    { return _files.erase(path) > 0; }
    bool remove(const String& path)  { return remove(path.c_str()); }

    File open(const char* path, int mode = FILE_READ) {
        if (isDir(path)) {
            File dir;
            std::vector<std::pair<std::string, std::vector<uint8_t>*>> entries;
            std::string prefix = std::string(path) + "/";
            for (auto& kv : _files) {
                if (kv.first.rfind(prefix, 0) == 0 &&
                    kv.first.find('/', prefix.length()) == std::string::npos) {
                    entries.push_back({kv.first.substr(prefix.length()), &kv.second});
                }
            }
            dir.setDirectoryEntries(std::move(entries));
            return dir;
        }
        if (mode == FILE_READ) {
            auto it = _files.find(path);
            if (it == _files.end()) return File();
            return File(&it->second, FILE_READ);
        }
        // WRITE / APPEND — create if needed.
        auto& buf = _files[path];
        return File(&buf, mode);
    }
    File open(const String& path, int mode = FILE_READ) { return open(path.c_str(), mode); }

    // Sim-specific helper to seed a file from the host filesystem.
    void simInject(const std::string& path, const std::vector<uint8_t>& data) {
        _files[path] = data;
    }
    void simInject(const std::string& path, const std::string& data) {
        _files[path] = std::vector<uint8_t>(data.begin(), data.end());
    }

private:
    bool isDir(const char* path) {
        std::string p = path; if (p.empty()) return false;
        if (p.back() == '/') p.pop_back();
        std::string prefix = p + "/";
        for (auto& kv : _files) if (kv.first.rfind(prefix, 0) == 0) return true;
        return false;
    }
    std::unordered_map<std::string, std::vector<uint8_t>> _files;
};
extern SDClass SD;
