add_executable(test_program
    main.c
)

include_directories(../../include)
add_executable(test_class
    class_main.cxx
    ../../src/distance_sensor.cpp
)

pico_generate_pio_header(test_program ${CMAKE_CURRENT_LIST_DIR}/../../src/sensor.pio)
pico_generate_pio_header(test_class ${CMAKE_CURRENT_LIST_DIR}/../../src/sensor.pio)

pico_enable_stdio_usb(test_program 1)
pico_enable_stdio_uart(test_program 0)

pico_enable_stdio_usb(test_class 1)
pico_enable_stdio_uart(test_class 0)

pico_add_extra_outputs(test_program)
pico_add_extra_outputs(test_class)

target_link_libraries(test_program pico_stdlib hardware_pio hardware_irq)
target_link_libraries(test_class pico_stdlib hardware_pio hardware_irq)

