#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "../core/Plugin.h"
#include "../core/constants.h"
#include "../models/MachineState.h"

class SDCardPlugin : public Plugin {
public:
    const char* getName() const override { return "SDCard"; }

    bool setup() override {
        #if HAS_CUSTOM_SPI_PINS
            SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
        #endif

        if (!SD.begin(PIN_SD_CS)) {
            DEBUG_PRINTLN("[SDCard] Mount FAILED");
            _mounted = false;
            bus().publish(EventType::SDCardError);
            return false;
        }
        
        _mounted = true;

        // Ensure recipes directory exists
        if (!SD.exists(SD_RECIPES_DIR)) {
            SD.mkdir(SD_RECIPES_DIR);
        }

        DEBUG_PRINTLN("[SDCard] Mounted successfully");
        bus().publish(EventType::SDCardMounted);
        return true;
    }

    void loop() override {
        // No periodic work
    }

    // ── Recipe File Operations ──────────────────────────────

    // List all .txt files in /recipes
    std::vector<String> listRecipes() {
        std::vector<String> recipes;
        File dir = SD.open(SD_RECIPES_DIR);
        if (!dir || !dir.isDirectory()) return recipes;

        File entry;
        while ((entry = dir.openNextFile())) {
            String name = entry.name();
            if (!entry.isDirectory() && name.endsWith(".txt")) {
                recipes.push_back(name);
            }
            entry.close();
        }
        dir.close();
        return recipes;
    }

    // Read recipe file content
    String readRecipe(const String& filename) {
        String path = String(SD_RECIPES_DIR) + "/" + filename;
        File file = SD.open(path, FILE_READ);
        if (!file) return "";

        String content = file.readString();
        file.close();
        return content;
    }

    // Write recipe file
    bool writeRecipe(const String& filename, const String& content) {
        String path = String(SD_RECIPES_DIR) + "/" + filename;
        File file = SD.open(path, FILE_WRITE);
        if (!file) return false;

        file.print(content);
        file.close();
        return true;
    }

    // Delete recipe file
    bool deleteRecipe(const String& filename) {
        String path = String(SD_RECIPES_DIR) + "/" + filename;
        return SD.remove(path);
    }

    // ── Recovery File Operations ────────────────────────────

    bool saveRecoveryData(const uint8_t* data, size_t len) {
        File file = SD.open(SD_RECOVERY_FILE, FILE_WRITE);
        if (!file) return false;
        file.write(data, len);
        file.close();
        return true;
    }

    size_t loadRecoveryData(uint8_t* data, size_t maxLen) {
        File file = SD.open(SD_RECOVERY_FILE, FILE_READ);
        if (!file) return 0;
        size_t read = file.read(data, maxLen);
        file.close();
        return read;
    }

    bool deleteRecoveryData() {
        return SD.remove(SD_RECOVERY_FILE);
    }

    bool hasRecoveryData() {
        return SD.exists(SD_RECOVERY_FILE);
    }

    bool isMounted() const { return _mounted; }

private:
    bool _mounted = false;
};
