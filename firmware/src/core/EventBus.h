#pragma once

#ifdef NATIVE_BUILD
  // Native unit tests provide an Arduino `String` stub via test_mocks.h.
#else
  #include <Arduino.h>
#endif

#include <functional>
#include <vector>
#include <algorithm>

// ─── Event Types ────────────────────────────────────────────
enum class EventType : uint8_t {
    // Temperature
    TemperatureRead,        // float: current temperature
    TemperatureError,       // sensor read error

    // PID / Heater
    SetpointChanged,        // float: new setpoint
    PIDOutputChanged,       // float: PID output (0-255)
    HeaterStateChanged,     // bool: on/off
    PIDParamsChanged,       // PID Kp, Ki, Kd changed

    // Ramp Mode
    RampConfigChanged,      // float: rate in °C/min (0 = disabled)
    RampedSetpointChanged,  // float: current ramped setpoint
    RampCompleted,          // float: final temperature reached

    // Boil Timer
    BoilStarted,
    BoilPaused,
    BoilResumed,
    BoilCompleted,
    BoilAdditionAlert,      // string: addition name

    // Pump
    PumpStateChanged,       // bool: on/off

    // Recipe
    RecipeStarted,          // string: recipe filename
    RecipeStopped,
    RecipePaused,
    RecipeResumed,
    RecipeStepChanged,      // int: step index
    RecipeCompleted,
    RecipeWaitConfirm,      // string: confirm message
    RecipeConfirmed,
    RecipeRecoveryAvailable, // string: recipe name (recovery data found on boot)

    // SD Card
    SDCardMounted,
    SDCardError,

    // BLE
    BLEClientConnected,
    BLEClientDisconnected,
    BLECommandReceived,     // string: JSON command
    BLESend,                // string: JSON message to send to client

    // WiFi
    WiFiConnected,
    WiFiDisconnected,

    // RTC
    RTCTimeUpdated,         // float: unix timestamp

    // Timer (generic countdown timer)
    TimerStartRequest,      // int: duration in seconds (or string "HH:MM" for absolute)
    TimerStarted,           // int: duration in seconds
    TimerTick,              // int: remaining seconds
    TimerPaused,
    TimerResumed,
    TimerCompleted,
    TimerCancelled,

    // Auto-Tune
    AutoTuneStarted,
    AutoTuneProgress,       // int: progress percentage
    AutoTuneCompleted,      // string: JSON with Kp, Ki, Kd
    AutoTuneFailed,         // string: error message

    // Scheduler ("be ready at HH:MM")
    SchedulerSet,           // Scheduler configured with target time
    SchedulerStarting,      // Heating started (calculated start time reached)
    SchedulerReady,         // Target temperature reached at target time
    SchedulerCancelled,     // Scheduler cancelled

    // System
    SystemReady,
    SystemError,
    SettingsChanged,
};

// ─── Event Data ─────────────────────────────────────────────
struct Event {
    EventType type;

    union {
        float   floatValue;
        int     intValue;
        bool    boolValue;
    };

    String stringValue;    // for strings (JSON commands, filenames, messages)

    // Convenience constructors
    Event(EventType t) : type(t), floatValue(0) {}
    Event(EventType t, float v) : type(t), floatValue(v) {}
    Event(EventType t, int v) : type(t), intValue(v) {}
    Event(EventType t, bool v) : type(t), boolValue(v) {}
    Event(EventType t, const String& s) : type(t), floatValue(0), stringValue(s) {}
    Event(EventType t, float v, const String& s) : type(t), floatValue(v), stringValue(s) {}
};

// ─── Event Callback ─────────────────────────────────────────
using EventCallback = std::function<void(const Event&)>;

struct Subscription {
    EventType   type;
    EventCallback callback;
    uint16_t    id;
};

// ─── Event Bus ──────────────────────────────────────────────
class EventBus {
public:
    static EventBus& instance() {
        static EventBus bus;
        return bus;
    }

    // Subscribe to a specific event type. Returns subscription ID.
    uint16_t subscribe(EventType type, EventCallback callback) {
        uint16_t id = _nextId++;
        _subscriptions.push_back({type, callback, id});
        return id;
    }

    // Unsubscribe by ID
    void unsubscribe(uint16_t id) {
        _subscriptions.erase(
            std::remove_if(_subscriptions.begin(), _subscriptions.end(),
                [id](const Subscription& s) { return s.id == id; }),
            _subscriptions.end()
        );
    }

#ifdef NATIVE_BUILD
    // Test-only helpers — the singleton is shared across the Unity binary
    // so each test resets state explicitly.
    void   reset()           { _subscriptions.clear(); _nextId = 1; }
    size_t subscriberCount() { return _subscriptions.size(); }
#endif

    // Publish an event to all subscribers.
    // P12 fix: copy _subscriptions before iterating so that subscribers may
    // safely call subscribe()/unsubscribe() inside their own callback without
    // invalidating the iterator. Cost: ~50 elements × ~16 bytes per publish
    // (irrelevant on ESP32 at typical publish rates).
    void publish(const Event& event) {
        auto subs_copy = _subscriptions;
        for (auto& sub : subs_copy) {
            if (sub.type == event.type) {
                sub.callback(event);
            }
        }
    }

    // Convenience: publish with just type
    void publish(EventType type) {
        publish(Event(type));
    }

    // Convenience: publish with float
    void publish(EventType type, float value) {
        publish(Event(type, value));
    }

    // Convenience: publish with int
    void publish(EventType type, int value) {
        publish(Event(type, value));
    }

    // Convenience: publish with bool
    void publish(EventType type, bool value) {
        publish(Event(type, value));
    }

    // Convenience: publish with string
    void publish(EventType type, const String& value) {
        publish(Event(type, value));
    }

    // Convenience: publish with float and string
    void publish(EventType type, float value, const String& str) {
        publish(Event(type, value, str));
    }

private:
    EventBus() = default;
    std::vector<Subscription> _subscriptions;
    uint16_t _nextId = 1;
};
