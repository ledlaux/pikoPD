#include "test_harness.h"

uint TestHarness::GetOffset(PIO pio) {
    return DistanceSensor::_PioOffsets[pio];
}


bool TestHarness::IsLoaded(PIO pio) {
    return DistanceSensor::_IsProgramLoaded(pio);
}

DistanceSensor* TestHarness::GetMappedSensor(PIO pio, uint sm) {
    return _sensor_map[pio][sm];
}
