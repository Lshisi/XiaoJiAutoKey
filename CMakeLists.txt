cmake_minimum_required(VERSION 3.20)

project(XiaoJiAutoKey VERSION 1.0.8 LANGUAGES CXX)

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
    src/core/AppTypes.cpp
    src/core/ConfigStore.cpp
    src/core/DdDriver.cpp
    src/core/FactionTheme.cpp
    src/core/JsonLite.cpp
    src/core/MacroEngine.cpp
    src/core/RepeaterEngine.cpp
)

set(UI_COMPONENT_SOURCES
    src/ui/components/FluentTitleBar.cpp
    src/ui/components/FluentSidebar.cpp
    src/ui/components/FluentButton.cpp
    src/ui/components/FluentCard.cpp
    src/ui/components/FluentSwitch.cpp
    src/ui/components/FluentCheckBox.cpp
    src/ui/components/FluentLineEdit.cpp
    src/ui/components/FluentComboBox.cpp
    src/ui/components/FluentTable.cpp
    src/ui/components/DraggableTable.cpp
)

set(UI_PAGE_SOURCES
    src/ui/pages/KeyPage.cpp
    src/ui/pages/MacroPage.cpp
    src/ui/pages/SettingsPage.cpp
    src/ui/pages/AboutPage.cpp
)

set(UI_COMPONENT_HEADERS
    src/ui/components/FluentTitleBar.h
    src/ui/components/FluentSidebar.h
    src/ui/components/FluentButton.h
    src/ui/components/FluentCard.h
    src/ui/components/FluentSwitch.h
    src/ui/components/FluentCheckBox.h
    src/ui/components/FluentLineEdit.h
    src/ui/components/FluentComboBox.h
    src/ui/components/FluentTable.h
    src/ui/components/DraggableTable.h
)

set(UI_PAGE_HEADERS
    src/ui/pages/KeyPage.h
    src/ui/pages/MacroPage.h
    src/ui/pages/SettingsPage.h
    src/ui/pages/AboutPage.h
)

add_executable(XiaoJiAutoKey WIN32
    include/app/input/GlobalInputHook.h
    include/app/input/InputKeyUtils.h
    include/app/config/ActiveConfigValidator.h
    include/app/controllers/ControllerResult.h
    include/app/config/ConfigNormalizer.h
    include/app/coordinators/SettingsCoordinator.h
    include/app/coordinators/ProfileCoordinator.h
    include/app/coordinators/MacroCoordinator.h
    include/app/coordinators/KeyBindingsCoordinator.h
    include/app/coordinators/EngineHookCoordinator.h
    include/app/coordinators/SoundCoordinator.h
    include/app/coordinators/CaptureCoordinator.h
    include/app/coordinators/PageRefreshCoordinator.h
    include/app/coordinators/CloseCoordinator.h
    include/app/coordinators/ConfigCoordinator.h
    include/app/coordinators/EngineStateCoordinator.h
    include/app/controllers/MacroController.h
    include/app/widgets/StatusOverlayWindow.h
    include/app/MainWindow.h
    include/app/view/MainWindowView.h
    include/app/controllers/ProfileController.h
    include/app/services/AppPaths.h
    include/app/services/SoundService.h
    include/app/dialogs/BindingEditDialog.h
    include/app/dialogs/AppMessageDialog.h
    include/app/dialogs/ConfigConfirmDialog.h
    include/app/dialogs/ConfigNameDialog.h
    include/app/dialogs/CloseConfirmDialog.h
    include/app/dialogs/KeyCaptureDialogBase.h
    include/app/dialogs/MacroStepDialog.h
    include/app/dialogs/PauseKeyDialog.h
    include/app/dialogs/SoundSettingsDialog.h
    include/core/MacroEngine.h
    src/app/input/GlobalInputHook.cpp
    src/app/config/ActiveConfigValidator.cpp
    src/app/config/ConfigNormalizer.cpp
    src/app/coordinators/SettingsCoordinator.cpp
    src/app/coordinators/ProfileCoordinator.cpp
    src/app/coordinators/MacroCoordinator.cpp
    src/app/coordinators/KeyBindingsCoordinator.cpp
    src/app/coordinators/EngineHookCoordinator.cpp
    src/app/coordinators/SoundCoordinator.cpp
    src/app/coordinators/CaptureCoordinator.cpp
    src/app/coordinators/PageRefreshCoordinator.cpp
    src/app/coordinators/CloseCoordinator.cpp
    src/app/coordinators/ConfigCoordinator.cpp
    src/app/coordinators/EngineStateCoordinator.cpp
    src/app/controllers/MacroController.cpp
    src/app/widgets/StatusOverlayWindow.cpp
    src/app/main.cpp
    src/app/main_window/MainWindow.cpp
    src/app/main_window/MainWindowHooks.cpp
    src/app/main_window/MainWindowUi.cpp
    src/app/view/MainWindowView.cpp
    src/app/controllers/ProfileController.cpp
    src/app/services/AppPaths.cpp
    src/app/services/SoundService.cpp
    src/app/dialogs/AppMessageDialog.cpp
    src/app/dialogs/ConfigConfirmDialog.cpp
    src/app/dialogs/ConfigNameDialog.cpp
    src/app/dialogs/DialogChrome.cpp
    src/app/dialogs/BindingEditDialog.cpp
    src/app/dialogs/CloseConfirmDialog.cpp
    src/app/dialogs/KeyCaptureDialogBase.cpp
    src/app/dialogs/MacroStepDialog.cpp
    src/app/dialogs/PauseKeyDialog.cpp
    src/app/dialogs/SoundSettingsDialog.cpp
    ${CORE_SOURCES}
    ${UI_COMPONENT_SOURCES}
    ${UI_COMPONENT_HEADERS}
    ${UI_PAGE_SOURCES}
    ${UI_PAGE_HEADERS}
    resources.qrc
    app.rc
)

target_include_directories(XiaoJiAutoKey PRIVATE
    include/app
    include/core
    src
    src/app
    src/app/main_window
    src/app/controllers
    src/app/services
    src/app/input
    src/app/view
    src/app/config
    src/app/coordinators
    src/app/widgets
    src/core
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
    target_link_options(XiaoJiAutoKey PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/assets/app.manifest"
    )
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
