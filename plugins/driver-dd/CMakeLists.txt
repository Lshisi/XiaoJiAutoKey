cmake_minimum_required(VERSION 3.20)
project(XiaoJiPluginDD VERSION 1.0.0 LANGUAGES CXX)
set(XIAOJI_PLUGIN_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk" CACHE PATH "XiaoJi Plugin SDK source")
if(NOT TARGET XiaoJi::PluginSDK)
    add_subdirectory("${XIAOJI_PLUGIN_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/sdk")
endif()
xiaoji_add_plugin(XiaoJiPluginDD MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    OUTPUT_NAME xiaoji-plugin-dd
    SOURCES src/Plugin.cpp src/DdDriver.cpp
    LIBRARIES user32 advapi32
    RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/driver")
target_include_directories(XiaoJiPluginDD PRIVATE src)
