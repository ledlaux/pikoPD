cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(pikopd C CXX ASM)

# ==============================================================================
# SDK Import & Initialization
# ==============================================================================
set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
set(PICO_EXTRAS_PATH "$ENV{PICO_SDK_PATH}/pico-extras")

include("${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/pico_extras_import.cmake")

pico_sdk_init()

# ==============================================================================
# Build Configuration & Global Definitions
# ==============================================================================
add_compile_definitions(
    HV_BARE_METAL
    ENABLE_DEBUG=$<BOOL:${ENABLE_DEBUG}>
)

add_definitions(-DMAX_VOICES=${MAX_VOICES})

if(WEB)
    message(STATUS "Web Mode Active")
    set(USB_LIB "")
    add_compile_definitions(
        WEB=1
        WEB_ENABLED=1
        OSC_ENABLED=1
        ACTIVE_MODE=${ACTIVE_MODE}
        CFG_TUH_ENABLED=0
        CFG_TUD_ENABLED=1
    )
elseif(MIDI_HOST_ENABLED)
    set(USB_LIB "tinyusb_host")
    add_compile_definitions(
        WEB=0
        WEB_ENABLED=0
        OSC_ENABLED=0
        MIDI_HOST=1
        CFG_TUH_ENABLED=1
        CFG_TUD_ENABLED=0
        CFG_TUH_MIDI=1
        CFG_TUSB_RHPORT0_MODE=OPT_MODE_HOST
    )
else()
    set(USB_LIB "tinyusb_device")
    add_compile_definitions(
        WEB=0
        WEB_ENABLED=0
        OSC_ENABLED=0
        CFG_TUH_ENABLED=0
        CFG_TUD_ENABLED=1
        CFG_TUD_MIDI=1
        CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE
    )
endif()

# ==============================================================================
# Source File Discovery
# ==============================================================================
file(GLOB ALL_SOURCES 
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/hvcc/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/hvcc/*.cpp"
)

if(WEB)
    file(GLOB WEB_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/src/web/*.c"
    )
    list(APPEND ALL_SOURCES ${WEB_FILES})
    list(FILTER ALL_SOURCES EXCLUDE REGEX ".*/src/usb/.*")
endif()

# Exclude duplicate web generated asset
list(REMOVE_ITEM ALL_SOURCES "${CMAKE_CURRENT_LIST_DIR}/htmldata.c")

# ==============================================================================
# Target Setup & Compile Definitions
# ==============================================================================
add_executable(pikopd ${ALL_SOURCES})

# Explicit suffix mapping prevents conflicts during pico_add_extra_outputs
set_target_properties(pikopd PROPERTIES SUFFIX ".elf")

target_include_directories(pikopd PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src"
    "${CMAKE_CURRENT_LIST_DIR}/src/usb"
    "${CMAKE_CURRENT_LIST_DIR}/src/sensors"  
    "${CMAKE_CURRENT_LIST_DIR}/hvcc"
    "${PICO_SDK_PATH}/src/common/pico_base/include"
    "${PICO_SDK_PATH}/lib/tinyusb/src"
    "${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/rp2040"
    "${PICO_EXTRAS_PATH}/tinyusb"
    $<$<BOOL:${WEB}>:${CMAKE_CURRENT_LIST_DIR}/src/web>
)

target_include_directories(pikopd PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(pikopd PRIVATE
    CFG_TUSB_MCU=OPT_MCU_RP2040
    HV_UNIX=1
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=2000
  MPR121_ENABLED=$<IF:$<BOOL:${MPR121_ENABLED}>,1,0>
    CNY70_ENABLED=$<IF:$<BOOL:${CNY70_ENABLED}>,1,0>
    HX710_ENABLED=$<IF:$<BOOL:${HX710_ENABLED}>,1,0>
    DISPLAY_ENABLED=$<IF:$<BOOL:${ENABLE_DISPLAY}>,1,0>
    $<$<BOOL:${USE_DELAY}>:USE_DELAY>
    $<$<BOOL:${USE_REVERB}>:USE_REVERB>
    $<$<BOOL:${USE_LIMITER}>:USE_LIMITER>
)

target_compile_options(pikopd PRIVATE -O3 -ffast-math)

# ==============================================================================
# Peripherals & Sensors
# ==============================================================================
set(DISTANCE_SENSOR_PATH "${CMAKE_CURRENT_LIST_DIR}/../lib/pico-distance-sensor")

if(ENABLE_DISTANCE_SENSOR)
    target_compile_definitions(pikopd PRIVATE DISTANCE_SENSOR_ENABLED=1)
    add_subdirectory(${DISTANCE_SENSOR_PATH} pico_distance_sensor)
    pico_generate_pio_header(pikopd ${CMAKE_CURRENT_LIST_DIR}/src/sensors/hc-sr04.pio)
    target_link_libraries(pikopd PRIVATE distance-sensor)
else()
    target_compile_definitions(pikopd PRIVATE DISTANCE_SENSOR_ENABLED=0)
endif()

target_include_directories(pikopd PRIVATE "${DISTANCE_SENSOR_PATH}/include")

if(PICO_ZERO_BOARD)
    target_compile_definitions(pikopd PRIVATE PICO_ZERO=1)
    pico_generate_pio_header(pikopd ${CMAKE_CURRENT_LIST_DIR}/src/led/ws2812.pio)
    target_link_libraries(pikopd PRIVATE hardware_pio)
endif()

# ==============================================================================
# Linking & Pico SDK Outputs
# ==============================================================================
target_link_libraries(pikopd PRIVATE
    pico_stdlib
    pico_audio_i2s
    pico_audio_pwm
    hardware_clocks
    hardware_adc
    hardware_pwm
    pico_multicore
    hardware_i2c
)

if(NOT WEB)
    target_link_libraries(pikopd PRIVATE
        ${USB_LIB}
        tinyusb_board
    )
endif()

if("${PICO_BOARD}" STREQUAL "pico_w" OR "${PICO_BOARD}" STREQUAL "pico2_w")
    target_compile_definitions(pikopd PRIVATE PICO_W=1)
    if(WEB)
        target_link_libraries(pikopd PRIVATE 
            pico_cyw43_arch_lwip_threadsafe_background
            pico_lwip_http
            pico_lwip_mdns
        )
    else()
        target_link_libraries(pikopd PRIVATE pico_cyw43_arch_none)
    endif()
endif()

pico_enable_stdio_usb(pikopd $<BOOL:${WEB}>)
pico_enable_stdio_uart(pikopd 0)

pico_add_extra_outputs(pikopd)
