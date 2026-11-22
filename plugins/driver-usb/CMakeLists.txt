cmake_minimum_required(VERSION 3.20)
project(XiaoJiPluginUSB VERSION 1.0.0 LANGUAGES CXX)
set(XIAOJI_PLUGIN_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk" CACHE PATH "XiaoJi Plugin SDK source")
if(NOT TARGET XiaoJi::PluginSDK)
    add_subdirectory("${XIAOJI_PLUGIN_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/sdk")
endif()
xiaoji_add_plugin(XiaoJiPluginUSB MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    OUTPUT_NAME xiaoji-plugin-usb
    SOURCES src/Plugin.cpp src/Ch552UsbDriver.cpp
    LIBRARIES setupapi user32)
target_include_directories(XiaoJiPluginUSB PRIVATE src)
