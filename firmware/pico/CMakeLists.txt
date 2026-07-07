cmake_minimum_required(VERSION 3.20)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(xiaoji_pico_firmware C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_executable(xiaoji_pico
    src/main.cpp
    src/usb_descriptors.cpp
)

target_include_directories(xiaoji_pico PRIVATE
    src
)

target_compile_definitions(xiaoji_pico PRIVATE
    CFG_TUSB_MCU=OPT_MCU_RP2040
)

target_link_libraries(xiaoji_pico PRIVATE
    pico_stdlib
    tinyusb_device
    tinyusb_board
)

pico_enable_stdio_usb(xiaoji_pico 0)
pico_enable_stdio_uart(xiaoji_pico 0)

pico_add_extra_outputs(xiaoji_pico)

add_custom_command(TARGET xiaoji_pico POST_BUILD
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_LIST_DIR}/tools/bin2uf2.py
        $<TARGET_FILE_DIR:xiaoji_pico>/xiaoji_pico.bin
        $<TARGET_FILE_DIR:xiaoji_pico>/xiaoji_pico.uf2
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/tools/bin2uf2.py
    COMMENT "Generating XiaoJi UF2"
)
