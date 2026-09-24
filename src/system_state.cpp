#include "system_state.h"

DisplayMode nextDisplayMode(DisplayMode mode)
{
    switch (mode)
    {
        case DisplayMode::TEMPERATURE: return DisplayMode::HUMIDITY;
        case DisplayMode::HUMIDITY:    return DisplayMode::LIGHT;
        case DisplayMode::LIGHT:       return DisplayMode::MOTION;
        case DisplayMode::MOTION:      return DisplayMode::TEMPERATURE;
    }

    return DisplayMode::TEMPERATURE;
}

DisplayMode previousDisplayMode(DisplayMode mode)
{
    switch (mode)
    {
        case DisplayMode::TEMPERATURE: return DisplayMode::MOTION;
        case DisplayMode::HUMIDITY:    return DisplayMode::TEMPERATURE;
        case DisplayMode::LIGHT:       return DisplayMode::HUMIDITY;
        case DisplayMode::MOTION:      return DisplayMode::LIGHT;
    }

    return DisplayMode::TEMPERATURE;
}

MotionState evaluateSystemState(MotionState current_state,
                                bool motion_detected,
                                uint32_t inactivity_ms,
                                uint32_t timeout_ms)
{
    if (motion_detected)
    {
        return MotionState::ACTIVE;
    }

    if (current_state == MotionState::ACTIVE && inactivity_ms >= timeout_ms)
    {
        return MotionState::INACTIVE;
    }

    return current_state;
}

const char *DisplayModeName(DisplayMode mode)
{
    switch (mode)
    {
        case DisplayMode::TEMPERATURE: return "Temperature";
        case DisplayMode::HUMIDITY:    return "Humidity";
        case DisplayMode::LIGHT:       return "Light";
        case DisplayMode::MOTION:      return "Motion";
    }

    return "Unknown";
}
