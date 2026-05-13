#pragma once
#include "distance_sensor.h"

// Create a derived test class for access to protected members
class TestHarness : public DistanceSensor {
public:
    TestHarness(uint trigger, PIO pio, uint sm) : DistanceSensor(trigger, pio, sm) {};

    static uint GetOffset(PIO pio);
    static bool IsLoaded(PIO pio);
    static DistanceSensor* GetMappedSensor(PIO pio, uint sm);
};

