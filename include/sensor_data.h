#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

/**
 * One snapshot of the room readings.
 * This will be sent as one item through the sensor queue in Part V, number 25.
 */
struct SensorData
{
    float temperature;       // Degrees Celsius
    float humidity;          // Relative humidity percent
    int lightLevel;          // Normalized ADC level from 0 to 100 percent
    bool motionDetected;     // Filled in when the PIR sensor is added
};

#endif
