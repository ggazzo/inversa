#include <unity.h>
#include "definitions.h"
#include "state.h"
#include "command.h"
#include "media.h"

void setUp(void) {
    // Setup code that runs before each test
}

void tearDown(void) {
    // Cleanup code that runs after each test
}

void test_state_initialization() {
    MachineState state;
    TEST_ASSERT_EQUAL(0, state.current_temperature_c);
    TEST_ASSERT_EQUAL(0, state.target_temperature_c);
    TEST_ASSERT_EQUAL(0, state.power_watts);
    TEST_ASSERT_EQUAL(0, state.volume_liters);
}

void test_command_parsing() {
    const char* cmd = "{\"command\":\"set_temperature\",\"value\":25.5}";
    CommandResult result = parseCommand(cmd);
    TEST_ASSERT_EQUAL(CommandType::SET_TEMPERATURE, result.type);
    TEST_ASSERT_EQUAL_FLOAT(25.5, result.value);
}

void test_media_operations() {
    #ifdef HAS_MEDIA
    TEST_ASSERT_FALSE(sdCardState.isMounted);
    initializeSDCard();
    // Note: This test might fail if no SD card is present
    // TEST_ASSERT_TRUE(sdCardState.isMounted);
    #endif
}

void RUN_UNITY_TESTS() {
    UNITY_BEGIN();
    RUN_TEST(test_state_initialization);
    RUN_TEST(test_command_parsing);
    RUN_TEST(test_media_operations);
    UNITY_END();
}

int main(int argc, char **argv) {
    RUN_UNITY_TESTS();
    return 0;
} 