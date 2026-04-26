# Pico C++ HC-SR04 Library

C++ library for reading an HC-SR04 distance sensor
using the raspberry pi pico.

## Features
- Specify the PIO / State machine to use
- IRQ driven reads
- Non blocking distance sensing

## How to use
```c++
// Include the library
#include "distance_sensor.h"

int main() {
    // Create an instance of the sensor
    // specify the pio, state machine, and gpio pin connected to trigger
    // echo pin must be on gpio pin trigger + 1.
    DistanceSensor hcsr04{pio0, 0, 0};

    // Trigger background sense
    hcsro4.TriggerRead();
    
    // wait for sensor to get a result
    while (hcsro4.is_sensing) {
        sleep_us(100);
    }

    // Read result
    printf("Reading %d centimeters\n", hcsr04.distance);
}
```

## How to include in your project
Including this library in your project is simple.

1. Clone the library into your repository. Using submodules is the simplest way to make sure
   all your repository users will have the same library you're using.
```
git submodule add https://github.com/dangarbri/pico-distance-sensor
```
if not using git, then just use `git clone` instead of `git submodule add`

2. Update your `CMakeLists.txt' to point to the library
```cmake
include_directories(
    ... # your include directories
    pico-distance-sensor/include # Add servo-lib's include directory to get "servo.h"
)
# Add the subdirectory so the library gets compiled
add_subdirectory(pico-distance-sensor)
# include it in your link libraries
target_link_libraries(your_program pico_stdlib distance-sensor)
```
