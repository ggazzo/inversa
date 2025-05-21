#ifndef EVENT_EMITTER_H
#define EVENT_EMITTER_H

#include <Arduino.h> // For String
#include <map>       // For std::map
#include <vector>    // For std::vector
#include <functional> // For std::function

// Forward declaration if we decide to use a more complex EventData struct/class
// struct EventData;

class EventEmitter {
public:
    // Callback function type. Takes an optional void pointer for arguments.
    using EventHandler = std::function<void(void* data)>;

    EventEmitter() = default; // Default constructor

    /**
     * @brief Registers an event listener for the given event name.
     * @param eventName The name of the event to listen for.
     * @param handler The callback function to execute when the event is emitted.
     */
    void on(const String& eventName, EventHandler handler) {
        if (handler) { // Only add valid handlers
            listeners[eventName].push_back(handler);
        }
    }

    /**
     * @brief Emits an event, calling all registered listeners for that event.
     * @param eventName The name of the event to emit.
     * @param data Optional data to pass to the event handlers.
     */
    void emit(const String& eventName, void* data = nullptr) {
        auto it = listeners.find(eventName);
        if (it != listeners.end()) {
            // Iterate over a copy of the handlers vector.
            // This prevents issues if a handler modifies the listeners list
            // (e.g., by calling on() or removeAllListeners() for the same event).
            std::vector<EventHandler> handlersToCall = it->second;
            for (const auto& handler : handlersToCall) {
                if (handler) { // Double check handler validity
                    handler(data);
                }
            }
        }
    }

    /**
     * @brief Removes all listeners for a specific event.
     * @param eventName The name of the event whose listeners should be removed.
     */
    void removeAllListeners(const String& eventName) {
        auto it = listeners.find(eventName);
        if (it != listeners.end()) {
            it->second.clear();
            // Optionally, remove the event name from the map if it has no more listeners
            // to save a bit of memory, though map lookups for empty vectors are fast.
            // if (it->second.empty()) {
            //     listeners.erase(it); 
            // }
        }
    }

    /**
     * @brief Removes all listeners for all events.
     */
    void clear() {
        listeners.clear();
    }

    /**
     * @brief Alias for on(). Registers an event listener.
     * @param eventName The name of the event to listen for.
     * @param handler The callback function to execute when the event is emitted.
     */
    void addListener(const String& eventName, EventHandler handler) {
        on(eventName, handler);
    }

private:
    // Using String as key for map might have performance/memory implications
    // on very constrained Arduinos, but is fine for ESP32/ESP8266.
    // Consider const char* or integer IDs for events in extreme cases.
    std::map<String, std::vector<EventHandler>> listeners;
};

#endif // EVENT_EMITTER_H 