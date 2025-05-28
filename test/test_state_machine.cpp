#include <unity.h>
#include "States/stateMachine.h"
#include "state.h"

extern MachineState state;
extern StateMachine mainTaskMachine;

void test_state_machine_initialization() {
    TEST_ASSERT_EQUAL(StateType::IDLE, state.current);
    TEST_ASSERT_EQUAL(0, state.current_temperature_c);
    TEST_ASSERT_EQUAL(0, state.target_temperature_c);
}

void test_state_transition_to_wait_temperature() {
    waitForTemperatureState.enter();
    TEST_ASSERT_EQUAL(StateType::WAIT_TEMPERATURE, state.current);
}

void test_state_transition_to_preparing() {
    preparingState.enter();
    TEST_ASSERT_EQUAL(StateType::PREPARING, state.current);
}

void test_state_transition_to_timer() {
    timerState.enter();
    TEST_ASSERT_EQUAL(StateType::WAIT_TIMER, state.current);
}

void test_state_transition_to_confirm() {
    confirmState.enter();
    TEST_ASSERT_EQUAL(StateType::WAIT_CONFIRM, state.current);
}

void test_state_transition_to_idle() {
    idleState.enter();
    TEST_ASSERT_EQUAL(StateType::IDLE, state.current);
}

void RUN_STATE_MACHINE_TESTS() {
    UNITY_BEGIN();
    RUN_TEST(test_state_machine_initialization);
    RUN_TEST(test_state_transition_to_wait_temperature);
    RUN_TEST(test_state_transition_to_preparing);
    RUN_TEST(test_state_transition_to_timer);
    RUN_TEST(test_state_transition_to_confirm);
    RUN_TEST(test_state_transition_to_idle);
    UNITY_END();
} 