#include <unity.h>

#include "alarm_logic.h"
#include "system_state.h"

void setUp(void) {}
void tearDown(void) {}

void test_temperature_below_lower_threshold_is_low()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::LOW_TEMPERATURE),
        static_cast<int>(evaluateTemperature(17.9f)));
}

void test_temperature_at_lower_threshold_is_normal()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::NORMAL),
        static_cast<int>(evaluateTemperature(18.0f)));
}

void test_temperature_inside_thresholds_is_normal()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::NORMAL),
        static_cast<int>(evaluateTemperature(24.0f)));
}

void test_temperature_at_upper_threshold_is_normal()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::NORMAL),
        static_cast<int>(evaluateTemperature(30.0f)));
}

void test_temperature_above_upper_threshold_is_high()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::HIGH_TEMPERATURE),
        static_cast<int>(evaluateTemperature(30.1f)));
}

void test_next_mode_moves_forward()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::HUMIDITY),
        static_cast<int>(nextDisplayMode(DisplayMode::TEMPERATURE)));
}

void test_next_mode_wraps_from_motion_to_temperature()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::TEMPERATURE),
        static_cast<int>(nextDisplayMode(DisplayMode::MOTION)));
}

void test_previous_mode_moves_backward()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::TEMPERATURE),
        static_cast<int>(previousDisplayMode(DisplayMode::HUMIDITY)));
}

void test_previous_mode_wraps_from_temperature_to_motion()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::MOTION),
        static_cast<int>(previousDisplayMode(DisplayMode::TEMPERATURE)));
}

void test_active_state_remains_active_before_timeout()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MotionState::ACTIVE),
        static_cast<int>(evaluateSystemState(MotionState::ACTIVE, false, 14999, 15000)));
}

void test_active_state_becomes_inactive_at_timeout()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MotionState::INACTIVE),
        static_cast<int>(evaluateSystemState(MotionState::ACTIVE, false, 15000, 15000)));
}

void test_inactive_state_stays_inactive_without_motion()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MotionState::INACTIVE),
        static_cast<int>(evaluateSystemState(MotionState::INACTIVE, false, 0, 15000)));
}

void test_motion_activates_inactive_state()
{
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MotionState::ACTIVE),
        static_cast<int>(evaluateSystemState(MotionState::INACTIVE, true, 0, 15000)));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_temperature_below_lower_threshold_is_low);
    RUN_TEST(test_temperature_at_lower_threshold_is_normal);
    RUN_TEST(test_temperature_inside_thresholds_is_normal);
    RUN_TEST(test_temperature_at_upper_threshold_is_normal);
    RUN_TEST(test_temperature_above_upper_threshold_is_high);
    RUN_TEST(test_next_mode_moves_forward);
    RUN_TEST(test_next_mode_wraps_from_motion_to_temperature);
    RUN_TEST(test_previous_mode_moves_backward);
    RUN_TEST(test_previous_mode_wraps_from_temperature_to_motion);
    RUN_TEST(test_active_state_remains_active_before_timeout);
    RUN_TEST(test_active_state_becomes_inactive_at_timeout);
    RUN_TEST(test_inactive_state_stays_inactive_without_motion);
    RUN_TEST(test_motion_activates_inactive_state);
    return UNITY_END();
}
