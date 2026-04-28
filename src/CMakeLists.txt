cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(pikopd C CXX ASM)

if(WEB)
    add_compile_definitions(
        WEB=1
        WEB_ENABLED=1
        ACTIVE_MODE=${ACTIVE_MODE}
        OSC_ENABLED=1
    )
    message(STATUS "Web Mode Active")
else()
    add_compile_definitions(
        WEB=0
        WEB_ENABLED=0
        OSC_ENABLED=0
    )
endif()

add_compile_definitions(HV_BARE_METAL)

set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
set(PICO_EXTRAS_PATH "$ENV{PICO_SDK_PATH}/pico-extras")

include("${CMAKE_CURRENT_LIST_DIR}/src/pico_sdk_import.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/src/pico_extras_import.cmake")

if(ENABLE_DEBUG)
    add_compile_definitions(ENABLE_DEBUG=1)
else()
    add_compile_definitions(ENABLE_DEBUG=0)
endif()

add_definitions(-DMAX_VOICES=${MAX_VOICES})

if(WEB)
    set(USB_LIB "")
    add_compile_definitions(
        CFG_TUH_ENABLED=0
        CFG_TUD_ENABLED=1
    )
else()
    if(MIDI_HOST_ENABLED)
        set(USB_LIB "tinyusb_host")
        add_compile_definitions(
            MIDI_HOST=1
            CFG_TUH_ENABLED=1
            CFG_TUD_ENABLED=0
            CFG_TUH_MIDI=1
            CFG_TUSB_RHPORT0_MODE=OPT_MODE_HOST
        )
    else()
        set(USB_LIB "tinyusb_device")
        add_compile_definitions(
            CFG_TUH_ENABLED=0
            CFG_TUD_ENABLED=1
            CFG_TUD_MIDI=1
            CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE
        )
    endif()
endif()

if(WEB)
    include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/src/web
    )
endif()

pico_sdk_init()

file(GLOB ALL_SOURCES 
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/usb/*.c"
   "${CMAKE_CURRENT_LIST_DIR}/src/screen/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/hvcc/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/hvcc/*.cpp"
)

if(WEB)
    file(GLOB WEB_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/src/web/*.c"
    )
    list(APPEND ALL_SOURCES ${WEB_FILES})
endif()

file(GLOB HVCC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/hvcc/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/hvcc/*.cpp"
)

if(WEB)
    list(FILTER ALL_SOURCES EXCLUDE REGEX ".*/src/usb/.*")
endif()

list(REMOVE_ITEM ALL_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/web/htmldata.c")


add_executable(pikopd.elf ${ALL_SOURCES})

target_include_directories(pikopd.elf PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src"
    "${CMAKE_CURRENT_LIST_DIR}/src/usb"
    "${CMAKE_CURRENT_LIST_DIR}/src/sensors"  
    "${CMAKE_CURRENT_LIST_DIR}/hvcc"
)

if(NOT WEB)
    target_include_directories(pikopd.elf PRIVATE
        "${PICO_SDK_PATH}/lib/tinyusb/src"
        "${PICO_EXTRAS_PATH}/tinyusb"
    )
endif()

target_compile_definitions(pikopd.elf PRIVATE
    CFG_TUSB_MCU=OPT_MCU_RP2040
    HV_UNIX=1
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=2000
)


option(ENABLE_DISTANCE_SENSOR "Build with HC-SR04 support" ON)

if(ENABLE_DISTANCE_SENSOR)
    target_compile_definitions(pikopd.elf PRIVATE DISTANCE_SENSOR_ENABLED=1)
    
    set(DISTANCE_SENSOR_PATH "${CMAKE_CURRENT_LIST_DIR}/../lib/pico-distance-sensor")

    add_subdirectory(${DISTANCE_SENSOR_PATH} pico_distance_sensor)

    pico_generate_pio_header(pikopd.elf ${CMAKE_CURRENT_LIST_DIR}/../lib/pico-distance-sensor/src/sensor.pio)

    target_include_directories(pikopd.elf PRIVATE "${DISTANCE_SENSOR_PATH}/include")
    target_link_libraries(pikopd.elf PRIVATE distance-sensor)
else()
    add_compile_definitions(DISTANCE_SENSOR_ENABLED=0)
endif()



target_link_libraries(pikopd.elf PRIVATE
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
    target_link_libraries(pikopd.elf PRIVATE
        ${USB_LIB}
        tinyusb_board
    )
endif()

if(PICO_ZERO_BOARD)
    target_compile_definitions(pikopd.elf PRIVATE PICO_ZERO=1)
    pico_generate_pio_header(pikopd.elf ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)
    target_link_libraries(pikopd.elf PRIVATE hardware_pio)
endif()

if("${PICO_BOARD}" STREQUAL "pico_w")
    target_compile_definitions(pikopd.elf PRIVATE PICO_W=1)

    if(WEB)
        target_link_libraries(pikopd.elf PRIVATE 
            pico_cyw43_arch_lwip_threadsafe_background
            pico_lwip_http
            pico_lwip_mdns
        )
    else()
        target_link_libraries(pikopd.elf PRIVATE pico_cyw43_arch_none)
    endif()
endif()


if(WEB)
    pico_enable_stdio_usb(pikopd.elf 1)
else()
    pico_enable_stdio_usb(pikopd.elf 0)
endif()

pico_enable_stdio_uart(pikopd.elf 0)

pico_add_extra_outputs(pikopd.elf)

target_compile_options(pikopd.elf PRIVATE -O3 -ffast-math)
