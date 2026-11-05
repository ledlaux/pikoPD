/**
 * MIT License
 * 
 * Copyright (c) 2022 Daniel Garcia-Briseno
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "distance_sensor.h"
#include "test_harness.h"
#include <cassert>
#include <cstdio>

/**
 * Simulate triggering the IRQ when the test calls pio_sm_put
 */
void pio_sm_put(PIO pio, uint sm, uint32_t data) {
    
}

/**
 * Verify the pio program is loaded in memory
 */
void test_IsProgramLoaded_AfterLoading() {
    // Load on pio0.
    printf("Testing that programs are loaded in memory.");
    assert(TestHarness::IsLoaded(0) == true);
    assert(TestHarness::IsLoaded(1) == true);
    puts(" Success");
}

/**
 * Tests that in the beginning, the programs are not loaded into pico memory
 */
void test_IsProgramLoaded_AtFirst() {
    // Assert that at the start of the program
    // each distance sensor is not loaded
    printf("Checking that programs are not loaded in memory: ");
    assert(TestHarness::IsLoaded(0) == false);
    assert(TestHarness::IsLoaded(1) == false);
    puts("Success.");
}

/**
 * Verify the sensor map has no sensor pointers when the program loads
 */
void test_SensorMapEmptyAtStart() {
    printf("Testing that all sensors are mapped to 0 at program load.");
    for (uint pio = 0; pio < 2; pio++) {
        for (uint sm = 0; sm < 4; sm++) {
            assert(TestHarness::GetMappedSensor(pio, sm) == 0);
        }
    }
    puts("Success.");
}

/**
 * Test that the internal sensor map has the right location
 */
void test_SensorLocationIsCorrect(DistanceSensor* address, PIO pio, uint sm) {
    assert(TestHarness::GetMappedSensor(pio, sm) == address);
}

int main() {
    test_SensorMapEmptyAtStart();
    test_IsProgramLoaded_AtFirst();

    // Create instances
    DistanceSensor pio0_sensor{15, 0, 1};
    DistanceSensor pio1_sensor{15, 1, 2};

    test_IsProgramLoaded_AfterLoading();
    // Test the sensor address mapping is right for the loaded pios
    test_SensorLocationIsCorrect(&pio0_sensor, 0, 1);
    test_SensorLocationIsCorrect(&pio1_sensor, 1, 2);
    // Verify some other mappings are still 0
    test_SensorLocationIsCorrect(0, 0, 3);
    test_SensorLocationIsCorrect(0, 1, 3);

    return 0;
}
