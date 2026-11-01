
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(pico_i2s C CXX ASM)

pico_sdk_init()

add_executable(pico_i2s)

pico_set_program_name(pico_i2s "pico_i2s")
pico_set_program_version(pico_i2s "0.1")

pico_enable_stdio_uart(pico_i2s 0)
pico_enable_stdio_usb(pico_i2s 1)

# Generate I2S PIO header
pico_generate_pio_header(
    pico_i2s
    ${CMAKE_CURRENT_LIST_DIR}/lib/Pico-I2S/src/i2s.pio
)

# ---------------------------------------------------------
# Main project sources
# ---------------------------------------------------------

target_sources(pico_i2s PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/Pico-I2S/src/buffer_queue.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/Pico-I2S/src/dma_reader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/Pico-I2S/src/dma_writer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/Pico-I2S/src/i2s_master_clock.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/Pico-I2S/src/i2s_master_input.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/Pico-I2S/src/i2s_master_output.cpp
)

# ---------------------------------------------------------
# Heavy-generated sources
# ---------------------------------------------------------

file(GLOB HVCC_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/hvcc/*.c
    ${CMAKE_CURRENT_LIST_DIR}/hvcc/*.cpp
)

target_sources(pico_i2s PRIVATE
    ${HVCC_SOURCES}
)

# ---------------------------------------------------------
# Include directories
# ---------------------------------------------------------

target_include_directories(pico_i2s PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/hvcc
    ${CMAKE_CURRENT_LIST_DIR}/lib/Pico-I2S/src

)

# ---------------------------------------------------------
# Libraries
# ---------------------------------------------------------

target_link_libraries(pico_i2s PRIVATE
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_clocks
    pico_cyw43_arch_none
    pico_multicore

)

target_compile_definitions(pico_i2s PRIVATE
    HV_BARE_METAL
)


# ---------------------------------------------------------
# UF2 / ELF / BIN outputs
# ---------------------------------------------------------

pico_add_extra_outputs(pico_i2s)
