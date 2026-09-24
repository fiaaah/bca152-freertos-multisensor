#include "system_state.h"

DisplayMode AdvanceDisplayMode(DisplayMode mode, bool clockwise)
{
    if (clockwise)
    {
        switch (mode)
        {
            case DisplayMode::TEMPERATURE: return DisplayMode::HUMIDITY;
            case DisplayMode::HUMIDITY:    return DisplayMode::LIGHT;
            case DisplayMode::LIGHT:       return DisplayMode::MOTION;
            case DisplayMode::MOTION:      return DisplayMode::TEMPERATURE;
        }
    }
    else
    {
        switch (mode)
        {
            case DisplayMode::TEMPERATURE: return DisplayMode::MOTION;
            case DisplayMode::HUMIDITY:    return DisplayMode::TEMPERATURE;
            case DisplayMode::LIGHT:       return DisplayMode::HUMIDITY;
            case DisplayMode::MOTION:      return DisplayMode::LIGHT;
        }
    }

    return DisplayMode::TEMPERATURE;
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
