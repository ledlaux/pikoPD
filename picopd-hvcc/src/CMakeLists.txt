cmake_minimum_required(VERSION 3.13)

add_compile_definitions(HV_BARE_METAL)

# 1. Project Setup
project(heavy C CXX ASM)

# 2. RP2350 Architecture Flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mthumb -mcpu=cortex-m33")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mthumb -mcpu=cortex-m33")

# 3. Path & SDK Imports
set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
set(PICO_EXTRAS_PATH "$ENV{PICO_SDK_PATH}/pico-extras")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

# 4. TinyUSB Configuration
set(PICO_TINYUSB_HOST_ENABLED 0)
set(PICO_TINYUSB_DEVICE_ENABLED 1)

pico_sdk_init()

# 5. Collect All Source Files
file(GLOB ALL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

# 6. Create Executable
set(TARGET_NAME "heavy.elf")
add_executable(${TARGET_NAME} ${ALL_SOURCES})

# 7. Include Paths
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# 8. Link Libraries
target_link_libraries(${TARGET_NAME}
    pico_stdlib
    pico_audio_i2s
    tinyusb_device
    tinyusb_board
    hardware_clocks
    hardware_adc
    hardware_pwm
)

# 9. Hardware Config
target_compile_definitions(${TARGET_NAME} PRIVATE
    CFG_TUSB_MCU=OPT_MCU_RP2040  
    CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE
    PICO_RP2350=1
    HV_UNIX=1
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=2000
)

# 10. USB/Serial Enable
pico_enable_stdio_usb(${TARGET_NAME} 0)
pico_enable_stdio_uart(${TARGET_NAME} 0)

# 11. Output Generation
pico_add_extra_outputs(${TARGET_NAME})

