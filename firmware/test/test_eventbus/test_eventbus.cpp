// test_eventbus.cpp — Unit tests for EventBus (real src/core/EventBus.h).
//
// Previously this file copied the EventBus implementation into the test, so
// drift between test and prod was silent. It now includes the production
// header directly, gated on `NATIVE_BUILD`.

#include <unity.h>
#include "test_mocks.h"
#include "../../src/core/EventBus.h"

void setUp(void) {}
void tearDown(void) {}

// ─── Test state ─────────────────────────────────────────────

static int callCount = 0;
static float lastFloatValue = 0;
static bool lastBoolValue = false;

void resetTestState() {
    callCount = 0;
    lastFloatValue = 0;
    lastBoolValue = false;
    EventBus::instance().reset();
}

// ─── Basic publish/subscribe ────────────────────────────────

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
    EventBus::instance().subscribe(EventType::SystemReady, [](const Event&) { callCount++; });
    EventBus::instance().subscribe(EventType::SystemReady, [](const Event&) { callCount++; });
    EventBus::instance().publish(EventType::SystemReady);
    TEST_ASSERT_EQUAL(2, callCount);
}

void test_subscriber_filtering() {
    resetTestState();
    EventBus::instance().subscribe(EventType::TemperatureRead, [](const Event&) { callCount++; });
    EventBus::instance().publish(EventType::HeaterStateChanged, true);
    TEST_ASSERT_EQUAL(0, callCount);
    EventBus::instance().publish(EventType::TemperatureRead, 50.0f);
    TEST_ASSERT_EQUAL(1, callCount);
}

void test_unsubscribe() {
    resetTestState();
    uint16_t id = EventBus::instance().subscribe(EventType::PumpStateChanged,
        [](const Event&) { callCount++; });
    TEST_ASSERT_EQUAL(1, EventBus::instance().subscriberCount());
    EventBus::instance().publish(EventType::PumpStateChanged, true);
    TEST_ASSERT_EQUAL(1, callCount);
    EventBus::instance().unsubscribe(id);
    TEST_ASSERT_EQUAL(0, EventBus::instance().subscriberCount());
    EventBus::instance().publish(EventType::PumpStateChanged, false);
    TEST_ASSERT_EQUAL(1, callCount);  // not re-fired
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

// ─── P12 regression: subscribers can subscribe/unsubscribe inside their
// own callback without invalidating the iterator. The pre-P12 EventBus
// iterated `_subscriptions` in place; if a callback mutated the vector,
// the iterator pointed at moved memory and we either crashed or skipped
// neighbors. The fix copies the vector before iterating.

void test_p12_subscribe_during_publish_is_safe() {
    resetTestState();
    int newSubFires = 0;
    EventBus::instance().subscribe(EventType::SystemReady, [&newSubFires](const Event&) {
        EventBus::instance().subscribe(EventType::SystemReady, [&newSubFires](const Event&) {
            newSubFires++;
        });
        callCount++;
    });
    EventBus::instance().publish(EventType::SystemReady);
    TEST_ASSERT_EQUAL(1, callCount);
    // The newly-added subscriber MUST NOT fire on this same publish — it was
    // added during iteration of the snapshot.
    TEST_ASSERT_EQUAL(0, newSubFires);

    // ...but it does fire on the next publish.
    EventBus::instance().publish(EventType::SystemReady);
    TEST_ASSERT_EQUAL(1, newSubFires);
}

void test_p12_unsubscribe_during_publish_is_safe() {
    resetTestState();
    uint16_t toRemove = 0;
    EventBus::instance().subscribe(EventType::SystemReady, [&toRemove](const Event&) {
        EventBus::instance().unsubscribe(toRemove);
        callCount++;
    });
    toRemove = EventBus::instance().subscribe(EventType::SystemReady,
        [](const Event&) { callCount++; });
    // First publish: both fire (the snapshot was taken before the unsubscribe).
    EventBus::instance().publish(EventType::SystemReady);
    TEST_ASSERT_EQUAL(2, callCount);
    TEST_ASSERT_EQUAL(1, EventBus::instance().subscriberCount());
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
    RUN_TEST(test_p12_subscribe_during_publish_is_safe);
    RUN_TEST(test_p12_unsubscribe_during_publish_is_safe);
    return UNITY_END();
}
