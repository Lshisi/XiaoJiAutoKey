cmake_minimum_required(VERSION 3.20)

project(XiaoJiAutoKey VERSION 1.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Static Qt path (single exe output):
set(CMAKE_PREFIX_PATH "C:/Qt/5.15.2-static" ${CMAKE_PREFIX_PATH})
# Dynamic Qt path (Anaconda) - uncomment if you need dynamic build:
# set(CMAKE_PREFIX_PATH "D:/software/anaconda/Library" ${CMAKE_PREFIX_PATH})

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

set(AUTOKEY_DD_DIR "${CMAKE_SOURCE_DIR}/third_party/dd")
set(AUTOKEY_DD_DLL32 "${AUTOKEY_DD_DIR}/DD32.dll")
set(AUTOKEY_DD_DLL64 "${AUTOKEY_DD_DIR}/DD64.dll")

set(CORE_SOURCES
    src/AppTypes.cpp
    src/ConfigStore.cpp
    src/DdDriver.cpp
    src/FactionTheme.cpp
    src/JsonLite.cpp
    src/ProfileManager.cpp
    src/RepeaterEngine.cpp
)

set(UI_COMPONENT_SOURCES
    src/ui/components/FluentTitleBar.cpp
    src/ui/components/FluentSidebar.cpp
    src/ui/components/FluentButton.cpp
    src/ui/components/FluentCard.cpp
    src/ui/components/FluentSwitch.cpp
    src/ui/components/FluentLineEdit.cpp
    src/ui/components/FluentComboBox.cpp
    src/ui/components/FluentTable.cpp
)

set(UI_PAGE_SOURCES
    src/ui/pages/KeyPage.cpp
    src/ui/pages/SettingsPage.cpp
    src/ui/pages/AboutPage.cpp
)

set(UI_COMPONENT_HEADERS
    src/ui/components/FluentTitleBar.h
    src/ui/components/FluentSidebar.h
    src/ui/components/FluentButton.h
    src/ui/components/FluentCard.h
    src/ui/components/FluentSwitch.h
    src/ui/components/FluentLineEdit.h
    src/ui/components/FluentComboBox.h
    src/ui/components/FluentTable.h
)

set(UI_PAGE_HEADERS
    src/ui/pages/KeyPage.h
    src/ui/pages/SettingsPage.h
    src/ui/pages/AboutPage.h
)

add_executable(XiaoJiAutoKey WIN32
    include/MainWindow.h
    src/main.cpp
    src/MainWindow.cpp
    ${CORE_SOURCES}
    ${UI_COMPONENT_SOURCES}
    ${UI_COMPONENT_HEADERS}
    ${UI_PAGE_SOURCES}
    ${UI_PAGE_HEADERS}
    resources.qrc
    app.rc
)

target_include_directories(XiaoJiAutoKey PRIVATE
    include
    src
    src/ui
    src/ui/styles
    src/ui/components
    src/ui/pages
)

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
    dwmapi
)

if(MSVC)
    target_compile_options(XiaoJiAutoKey PRIVATE /W4 /permissive- /utf-8)
    set_property(TARGET XiaoJiAutoKey PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # Require administrator privileges
    target_link_options(XiaoJiAutoKey PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
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
