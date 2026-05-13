include_directories(../include)
add_library(distance-sensor
    distance_sensor.cpp
)

pico_generate_pio_header(distance-sensor ${CMAKE_CURRENT_LIST_DIR}/sensor.pio)

target_link_libraries(distance-sensor pico_stdlib hardware_pio hardware_irq)

