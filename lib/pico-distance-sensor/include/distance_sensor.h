#pragma once

#include "hardware/pio.h"

/**
 * Interface for using an HC-SR04 distance sensor. Has not been tested with other sensors
 */
class DistanceSensor {
public:
    /**
     * Initializes the sensor.
     * @note Due to the PIO implementation, the echo pin must be
     *       connected to trigger_gpio + 1. If you connect the
     *       trigger pin to GPIO 5, then GPIO 6 must be the echo pin.
     * @param[in] trigger_gpio GPIO connected to the trigger pin
     * @param[in] pio PIO block to use, Either pio0 or pio1
     * @param[in] sm The state machine to use, 0 to 3
     */
    DistanceSensor(PIO pio, uint sm, uint trigger_gpio);

    /**
     * Gets the distance sensor mapped to the given state machine
     * @param[in] pio 0 or 1
     * @param[in] sm 0 to 4
     * @returns NULL or DistanceSensor pointer
     */
    static DistanceSensor* GetMappedSensor(uint pio, uint sm);

    /**
     * Trigger a sensor read
     */
    void TriggerRead();

    /**
     * Last distance reading
     */
    uint32_t distance = 0;

    volatile bool is_sensing = false;

    /**
     * PIO bound to this sensor
     */
    PIO pio;

    /**
     * State machine this sensor is running on
     */
    uint sm;

protected:
    /**
     * My solution to the "C++ IRQ problem" The problem is that
     * an IRQ needs to be a plain C function, so I can't have a
     * function that modifies the running instance of the
     * DistanceSensor that triggered the request. There's no way
     * for the IRQ to know which sensor sent the request.
     *
     * But I can store the instance of the distance sensor
     * in a map, so the sensor running on PIO 0, state machine 1
     * will map to _sensor_map[0][1]. Thus the ISR can access the
     * sensor instance to provide a reading
     */
    static DistanceSensor* _sensor_map[2][4];

    /**
     * Offset into PIO memory for the sensor program
     * Cached here so we don't waste space in PIO memory by reloading it.
     */
    static uint _PioOffsets[2];

    /**
     * Initializes the distance program to run on the given gpio pin
     * @param[in] trigger_gpio GPIO connected to the trigger pin
     * @param[in] pio PIO block to use, Either pio0 or pio1
     * @param[in] sm The state machine to use, 0 to 3
     * @param[in] handler Function to execute when distance is sampled
     */
    static void _InitializeSensorForGpio(uint gpio, PIO pio, uint sm, irq_handler_t handler);

    /**
     * Checks if the sensor program is already loaded in the PIO
     * @note This checks the _is_loaded_in_pio mapping
     * @param[in] pio PIO memory to check
     */
    static bool _IsProgramLoaded(PIO pio);

    /**
     * Loads the PIO program into memory if it's not already loaded
     * @param[in] pio PIO block to use, Either pio0 or pio1
     * @returns Program offset
     */
    static uint _LoadSensorProgram(PIO pio);
};
