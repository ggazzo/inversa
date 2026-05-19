#pragma once

#ifdef NATIVE_BUILD
  // Native unit tests provide an Arduino `String` stub via test_mocks.h.
#else
  #include <Arduino.h>
#endif

#include <functional>
#include <utility>
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

    // Thermal Watchdog (001-thermal-watchdog)
    WatchdogTripped,        // int: WatchdogCause enum value
    WatchdogReset,          // bool: true = auto-reset by cooldown, false = manual
    WatchdogConfigChanged,  // —
    WatchdogKick,           // — (debug; ISR sentinel)
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
        if (_dispatchDepth > 0) {
            _pendingAdds.push_back({type, std::move(callback), id});
        } else {
            _subscriptions.push_back({type, std::move(callback), id});
        }
        return id;
    }

    // Unsubscribe by ID
    void unsubscribe(uint16_t id) {
        if (_dispatchDepth > 0) {
            // Defer removal: keep the live subscription firing for the rest of
            // the current dispatch (snapshot semantics matching the legacy
            // copy-on-publish behavior). Pruned once dispatch unwinds.
            _pendingRemoves.push_back(id);
            return;
        }
        _subscriptions.erase(
            std::remove_if(_subscriptions.begin(), _subscriptions.end(),
                [id](const Subscription& s) { return s.id == id; }),
            _subscriptions.end()
        );
    }

#ifdef NATIVE_BUILD
    // Test-only helpers — the singleton is shared across the Unity binary
    // so each test resets state explicitly.
    void   reset()           {
        _subscriptions.clear();
        _pendingAdds.clear();
        _pendingRemoves.clear();
        _nextId = 1;
        _dispatchDepth = 0;
    }
    size_t subscriberCount() { return _subscriptions.size(); }
#endif

    // Publish an event to all subscribers.
    // P12 fix: subscribers may safely subscribe()/unsubscribe() inside their
    // own callback. Instead of copying the whole vector per publish, we iterate
    // by index against the live vector and queue mutations into pending lists,
    // flushing once the outermost dispatch finishes. Reentrant publish() is
    // supported via _dispatchDepth.
    void publish(const Event& event) {
        _dispatchDepth++;
        // Snapshot size at entry so newly-subscribed handlers don't fire for
        // the current event (matches previous copy-on-publish semantics).
        const size_t n = _subscriptions.size();
        for (size_t i = 0; i < n; ++i) {
            const Subscription& sub = _subscriptions[i];
            if (sub.type == event.type) {
                sub.callback(event);
            }
        }
        _dispatchDepth--;
        if (_dispatchDepth == 0) {
            if (!_pendingRemoves.empty()) {
                for (uint16_t id : _pendingRemoves) {
                    _subscriptions.erase(
                        std::remove_if(_subscriptions.begin(), _subscriptions.end(),
                            [id](const Subscription& s) { return s.id == id; }),
                        _subscriptions.end()
                    );
                }
                _pendingRemoves.clear();
            }
            if (!_pendingAdds.empty()) {
                for (auto& a : _pendingAdds) _subscriptions.push_back(std::move(a));
                _pendingAdds.clear();
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
    std::vector<Subscription> _pendingAdds;
    std::vector<uint16_t>     _pendingRemoves;
    uint16_t _nextId = 1;
    uint16_t _dispatchDepth = 0;
};
