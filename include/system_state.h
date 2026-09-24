#pragma once

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

DisplayMode AdvanceDisplayMode(DisplayMode current, bool clockwise);
const char *DisplayModeName(DisplayMode mode);
