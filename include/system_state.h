#pragma once

#include <stdint.h>

enum class DisplayMode
{
    TEMPERATURE,
    HUMIDITY,
    LIGHT,
    MOTION
};

enum class MotionState
{
    ACTIVE,
    INACTIVE
};

DisplayMode nextDisplayMode(DisplayMode current);
DisplayMode previousDisplayMode(DisplayMode current);
const char *DisplayModeName(DisplayMode mode);

// Pure state decision: motion activates the system; inactivity timeout sleeps it.
MotionState evaluateSystemState(MotionState current_state,
                                bool motion_detected,
                                uint32_t inactivity_ms,
                                uint32_t timeout_ms);
