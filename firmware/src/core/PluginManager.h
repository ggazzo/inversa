#pragma once
#include <Arduino.h>
#include <vector>
#include <memory>
#include "Plugin.h"

class PluginManager {
public:
    static PluginManager& instance() {
        static PluginManager mgr;
        return mgr;
    }

    // Register a plugin (takes ownership)
    template<typename T, typename... Args>
    T* add(Args&&... args) {
        auto plugin = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = plugin.get();
        _plugins.push_back(std::move(plugin));
        return ptr;
    }

    // Initialize all plugins
    void setup() {
        Serial.println("[PluginManager] Initializing plugins...");
        for (auto& p : _plugins) {
            Serial.printf("[PluginManager] Setting up: %s\n", p->getName());
            if (!p->setup()) {
                Serial.printf("[PluginManager] DISABLED: %s\n", p->getName());
                p->setEnabled(false);
            }
        }
        Serial.printf("[PluginManager] %d plugins ready\n", _plugins.size());
    }

    // Run all enabled plugins
    void loop() {
        for (auto& p : _plugins) {
            if (p->isEnabled()) {
                p->loop();
            }
        }
    }

    // Get plugin by type
    template<typename T>
    T* get() {
        for (auto& p : _plugins) {
            T* cast = dynamic_cast<T*>(p.get());
            if (cast) return cast;
        }
        return nullptr;
    }

    size_t count() const { return _plugins.size(); }

private:
    PluginManager() = default;
    std::vector<std::unique_ptr<Plugin>> _plugins;
};
