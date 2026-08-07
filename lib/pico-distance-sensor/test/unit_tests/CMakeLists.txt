include(CTest)
include_directories(
    ../../include
    .
    stubs
)

if(DEFINED ENV{HOST_CXX_COMPILER})
  set(CMAKE_CXX_COMPILER $ENV{HOST_CXX_COMPILER})
else()
  set(CMAKE_CXX_COMPILER "g++")
endif()

set(CMAKE_CXX_FLAGS "")
set(CMAKE_BUILD_TYPE Debug)
set(TEST_HARNESS_SOURCES
    ../../src/distance_sensor.cpp
    test_harness.cpp
    stubs/sensor_stubs.cpp
    stubs/hardware/pio_stubs.cpp
)

add_executable(test_sensor tests/test_sensor.cpp ${TEST_HARNESS_SOURCES})
add_test(NAME test_sensor COMMAND test_sensor)

