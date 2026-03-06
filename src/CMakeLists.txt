cmake_minimum_required(VERSION 3.13)
project(heavy C CXX ASM)

add_compile_definitions(HV_BARE_METAL)

# Pico SDK paths
set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
set(PICO_EXTRAS_PATH "$ENV{PICO_SDK_PATH}/pico-extras")

# Include SDK cmake files from src/
include("${CMAKE_CURRENT_LIST_DIR}/src/pico_sdk_import.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/src/pico_extras_import.cmake")

pico_sdk_init()

# TinyUSB configuration
set(PICO_TINYUSB_HOST_ENABLED 0)
set(PICO_TINYUSB_DEVICE_ENABLED 1)

# --- Source files ---
file(GLOB SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"          
    "${CMAKE_CURRENT_LIST_DIR}/src/*.c"          
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"         
)

file(GLOB HVCC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/hvcc/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/hvcc/*.cpp"
)

set(ALL_SOURCES ${SRC_FILES} ${HVCC_FILES})

add_executable(heavy.elf ${ALL_SOURCES})

target_include_directories(heavy.elf PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src"
    "${CMAKE_CURRENT_LIST_DIR}/hvcc"
    "${PICO_SDK_PATH}/lib/tinyusb/src"
    "${PICO_EXTRAS_PATH}/tinyusb"
)

target_link_libraries(heavy.elf
    pico_stdlib
    pico_audio_i2s
    pico_audio_pwm
    tinyusb_device
    tinyusb_board
    hardware_clocks
    hardware_adc
    hardware_pwm
    pico_multicore
    pico_cyw43_arch_none
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

target_compile_options(heavy.elf PRIVATE -O3 -ffast-math)
target_compile_definitions(heavy.elf PRIVATE
    CFG_TUSB_MCU=OPT_MCU_RP2040
    CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE
    HV_UNIX=1
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=2000
)

pico_enable_stdio_usb(heavy.elf 0)
pico_enable_stdio_uart(heavy.elf 0)

pico_add_extra_outputs(heavy.elf)
