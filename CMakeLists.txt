cmake_minimum_required(VERSION 3.20)

project(XiaoJiAutoKey VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Static Qt path (single exe output):
set(CMAKE_PREFIX_PATH "C:/Qt/5.15.2-static" ${CMAKE_PREFIX_PATH})
# Dynamic Qt path (Anaconda) - uncomment if you need dynamic build:
# set(CMAKE_PREFIX_PATH "D:/software/anaconda/Library" ${CMAKE_PREFIX_PATH})set(CMAKE_PREFIX_PATH "D:/software/anaconda/Library" ${CMAKE_PREFIX_PATH})

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

set(AUTOKEY_DD_DIR "${CMAKE_SOURCE_DIR}/third_party/dd")
set(AUTOKEY_DD_DLL32 "${AUTOKEY_DD_DIR}/DD32.dll")
set(AUTOKEY_DD_DLL64 "${AUTOKEY_DD_DIR}/DD64.dll")

add_executable(XiaoJiAutoKey WIN32
    include/MainWindow.h
    src/main.cpp
    src/AppTypes.cpp
    src/ConfigStore.cpp
    src/DdDriver.cpp
    src/JsonLite.cpp
    src/RepeaterEngine.cpp
    src/MainWindow.cpp
    resources.qrc
    app.rc
)

target_include_directories(XiaoJiAutoKey PRIVATE include)

target_compile_definitions(XiaoJiAutoKey PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    APP_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(XiaoJiAutoKey PRIVATE
    Qt5::Widgets
    shlwapi
    winmm
)

if(MSVC)
    target_compile_options(XiaoJiAutoKey PRIVATE /W4 /permissive- /utf-8)
    set_property(TARGET XiaoJiAutoKey PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_target_properties(XiaoJiAutoKey PROPERTIES OUTPUT_NAME "XiaoJiAutoKey")

if(EXISTS "${AUTOKEY_DD_DLL32}")
    add_custom_command(TARGET XiaoJiAutoKey POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AUTOKEY_DD_DLL32}"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/DD32.dll"
        COMMENT "Copying DD32.dll to the target output directory"
    )
endif()

if(EXISTS "${AUTOKEY_DD_DLL64}")
    add_custom_command(TARGET XiaoJiAutoKey POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AUTOKEY_DD_DLL64}"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/DD64.dll"
        COMMENT "Copying DD64.dll to the target output directory"
    )
endif()
