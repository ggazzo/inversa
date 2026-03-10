// test_eventbus.cpp — Unit tests for EventBus
#include <unity.h>
#include "test_mocks.h"
#include <functional>
#include <vector>
#include <algorithm>

// Minimal EventBus implementation for testing (copied from core/EventBus.h)
enum class EventType : uint8_t {
    TemperatureRead,
    SetpointChanged,
    HeaterStateChanged,
    PumpStateChanged,
    SystemReady,
};

struct Event {
    EventType type;
    union {
        float floatValue;
        int intValue;
        bool boolValue;
    };
    String stringValue;

    Event(EventType t) : type(t), floatValue(0) {}
    Event(EventType t, float v) : type(t), floatValue(v) {}
    Event(EventType t, int v) : type(t), intValue(v) {}
    Event(EventType t, bool v) : type(t), boolValue(v) {}
    Event(EventType t, const String& s) : type(t), floatValue(0), stringValue(s) {}
};

using EventCallback = std::function<void(const Event&)>;

struct Subscription {
    EventType type;
    EventCallback callback;
    uint16_t id;
};

class EventBus {
public:
    static EventBus& instance() {
        static EventBus bus;
        return bus;
    }

    void reset() {
        _subscriptions.clear();
        _nextId = 1;
    }

    uint16_t subscribe(EventType type, EventCallback callback) {
        uint16_t id = _nextId++;
        _subscriptions.push_back({type, callback, id});
        return id;
    }

    void unsubscribe(uint16_t id) {
        _subscriptions.erase(
            std::remove_if(_subscriptions.begin(), _subscriptions.end(),
                [id](const Subscription& s) { return s.id == id; }),
            _subscriptions.end()
        );
    }

    void publish(const Event& event) {
        for (auto& sub : _subscriptions) {
            if (sub.type == event.type) {
                sub.callback(event);
            }
        }
    }

    void publish(EventType type) { publish(Event(type)); }
    void publish(EventType type, float value) { publish(Event(type, value)); }
    void publish(EventType type, int value) { publish(Event(type, value)); }
    void publish(EventType type, bool value) { publish(Event(type, value)); }
    void publish(EventType type, const String& value) { publish(Event(type, value)); }

    size_t subscriberCount() const { return _subscriptions.size(); }

private:
    EventBus() = default;
    std::vector<Subscription> _subscriptions;
    uint16_t _nextId = 1;
};

// ─── Test Variables ─────────────────────────────────────────
static int callCount = 0;
static float lastFloatValue = 0;
static bool lastBoolValue = false;

void resetTestState() {
    callCount = 0;
    lastFloatValue = 0;
    lastBoolValue = false;
    EventBus::instance().reset();
}

// ─── Tests ──────────────────────────────────────────────────

void test_subscribe_and_publish_float() {
    resetTestState();
    
    EventBus::instance().subscribe(EventType::TemperatureRead, [](const Event& e) {
        callCount++;
        lastFloatValue = e.floatValue;
    });
    
    EventBus::instance().publish(EventType::TemperatureRead, 65.5f);
    
    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 65.5, lastFloatValue);
}

void test_subscribe_and_publish_bool() {
    resetTestState();
    
    EventBus::instance().subscribe(EventType::HeaterStateChanged, [](const Event& e) {
        callCount++;
        lastBoolValue = e.boolValue;
    });
    
    EventBus::instance().publish(EventType::HeaterStateChanged, true);
    
    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_TRUE(lastBoolValue);
}

void test_multiple_subscribers() {
    resetTestState();
    
    EventBus::instance().subscribe(EventType::SystemReady, [](const Event&) {
        callCount++;
    });
    EventBus::instance().subscribe(EventType::SystemReady, [](const Event&) {
        callCount++;
    });
    
    EventBus::instance().publish(EventType::SystemReady);
    
    TEST_ASSERT_EQUAL(2, callCount);
}

void test_subscriber_filtering() {
    resetTestState();
    
    EventBus::instance().subscribe(EventType::TemperatureRead, [](const Event&) {
        callCount++;
    });
    
    // Publish different event type
    EventBus::instance().publish(EventType::HeaterStateChanged, true);
    
    // Should not trigger
    TEST_ASSERT_EQUAL(0, callCount);
    
    // Publish matching event type
    EventBus::instance().publish(EventType::TemperatureRead, 50.0f);
    
    TEST_ASSERT_EQUAL(1, callCount);
}

void test_unsubscribe() {
    resetTestState();
    
    uint16_t id = EventBus::instance().subscribe(EventType::PumpStateChanged, [](const Event&) {
        callCount++;
    });
    
    TEST_ASSERT_EQUAL(1, EventBus::instance().subscriberCount());
    
    EventBus::instance().publish(EventType::PumpStateChanged, true);
    TEST_ASSERT_EQUAL(1, callCount);
    
    EventBus::instance().unsubscribe(id);
    TEST_ASSERT_EQUAL(0, EventBus::instance().subscriberCount());
    
    EventBus::instance().publish(EventType::PumpStateChanged, false);
    TEST_ASSERT_EQUAL(1, callCount);  // Still 1, not incremented
}

void test_publish_with_string() {
    resetTestState();
    String received;
    
    EventBus::instance().subscribe(EventType::SystemReady, [&received](const Event& e) {
        received = e.stringValue;
        callCount++;
    });
    
    EventBus::instance().publish(EventType::SystemReady, String("test message"));
    
    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_TRUE(received == "test message");
}

// ─── Main ───────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_subscribe_and_publish_float);
    RUN_TEST(test_subscribe_and_publish_bool);
    RUN_TEST(test_multiple_subscribers);
    RUN_TEST(test_subscriber_filtering);
    RUN_TEST(test_unsubscribe);
    RUN_TEST(test_publish_with_string);
    
    return UNITY_END();
}
