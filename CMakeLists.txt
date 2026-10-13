cmake_minimum_required(VERSION 3.20)

project(XiaoJiAutoKey VERSION 1.1.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# Static Qt path (single exe output):
set(CMAKE_PREFIX_PATH "C:/Qt/5.15.2-static" ${CMAKE_PREFIX_PATH})
# Dynamic Qt path (Anaconda) - uncomment if you need dynamic build:
# set(CMAKE_PREFIX_PATH "D:/software/anaconda/Library" ${CMAKE_PREFIX_PATH})

find_package(Qt5 COMPONENTS Core Gui Widgets Svg REQUIRED)

set(MOC_HEADERS
    include/app/MainWindow.h
    include/app/input/GlobalInputHook.h
    include/app/input/RawInputManager.h
    include/app/coordinators/SettingsCoordinator.h
    include/app/coordinators/DriverCoordinator.h
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
    include/app/dialogs/BindingEditDialog.h
    include/app/dialogs/AppMessageDialog.h
    include/app/dialogs/CloseConfirmDialog.h
    include/app/dialogs/KeyCaptureDialogBase.h
    include/app/dialogs/MacroStepDialog.h
    include/app/dialogs/PauseKeyDialog.h
    include/app/dialogs/ShareExportDialog.h
    include/app/dialogs/ShareImportDialog.h
    include/app/dialogs/SoundSettingsDialog.h
    include/app/dialogs/StatusOverlaySettingsDialog.h
    include/app/dialogs/UpdateDialog.h
    include/app/widgets/StatusOverlayWindow.h
    src/ui/components/DraggableTable.h
    src/ui/components/FluentButton.h
    src/ui/components/FluentCard.h
    src/ui/components/FluentComboBox.h
    src/ui/components/FluentLineEdit.h
    src/ui/components/FluentSidebar.h
    src/ui/components/FluentSwitch.h
    src/ui/components/FluentTable.h
    src/ui/components/FluentTitleBar.h
    src/ui/components/ClickPositionGroup.h
    src/ui/components/ScreenPointPicker.h
    src/ui/pages/KeyPage.h
    src/ui/pages/MacroPage.h
    src/ui/pages/DriverPage.h
    src/ui/pages/SettingsPage.h
    src/ui/pages/AboutPage.h
    include/app/coordinators/UpdateCoordinator.h
    include/app/services/UpdateService.h
)

qt5_wrap_cpp(MOC_SOURCES ${MOC_HEADERS})
qt5_add_resources(RESOURCE_SOURCES resources.qrc)

set(AUTOKEY_DD_DIR "${CMAKE_SOURCE_DIR}/third_party/dd")
set(AUTOKEY_DD_DLL32 "${AUTOKEY_DD_DIR}/DD32.dll")
set(AUTOKEY_DD_DLL64 "${AUTOKEY_DD_DIR}/DD64.dll")
set(AUTOKEY_DD_HID_DIR "${AUTOKEY_DD_DIR}/hid")
set(AUTOKEY_DD_HID_DLL "${AUTOKEY_DD_HID_DIR}/ddhid.63340.dll")
set(AUTOKEY_DD_HID_DRV_DIR "${AUTOKEY_DD_HID_DIR}/drv")
set(AUTOKEY_DD_HID_DDC "${AUTOKEY_DD_HID_DRV_DIR}/ddc.exe")
set(AUTOKEY_DD_HID_CAT "${AUTOKEY_DD_HID_DRV_DIR}/ddhid63340.cat")
set(AUTOKEY_DD_HID_INF "${AUTOKEY_DD_HID_DRV_DIR}/ddhid63340.inf")
set(AUTOKEY_DD_HID_SYS "${AUTOKEY_DD_HID_DRV_DIR}/ddhid63340.sys")
set(AUTOKEY_USAGE_GUIDE "${CMAKE_SOURCE_DIR}/assets/使用说明.mhtml")

set(CORE_SOURCES
    src/core/AppTypes.cpp
    src/core/ClickPositioning.cpp
    src/core/ConfigStore.cpp
    src/core/DdDriver.cpp
    src/core/FactionTheme.cpp
    src/core/ForegroundWatcher.cpp
    src/core/InputDriverRouter.cpp
    src/core/JsonLite.cpp
    src/core/MacroEngine.cpp
    src/core/Ch552UsbDriver.cpp
    src/core/PicoUsbDriver.cpp
    src/core/RepeaterEngine.cpp
    src/core/ShareCodec.cpp
)

set(UI_COMPONENT_SOURCES
    src/ui/components/FactionMenu.cpp
    src/ui/components/FactionIcon.cpp
    src/ui/components/FluentPopupMenu.cpp
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
    src/ui/components/ClickPositionGroup.cpp
    src/ui/components/ScreenPointPicker.cpp
)

set(UI_PAGE_SOURCES
    src/ui/pages/KeyPage.cpp
    src/ui/pages/MacroPage.cpp
    src/ui/pages/DriverPage.cpp
    src/ui/pages/SettingsPage.cpp
    src/ui/pages/AboutPage.cpp
)

set(UI_COMPONENT_HEADERS
    src/ui/components/FactionMenu.h
    src/ui/components/FactionIcon.h
    src/ui/components/FluentPopupMenu.h
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
    src/ui/components/ClickPositionGroup.h
    src/ui/components/ScreenPointPicker.h
)

set(UI_PAGE_HEADERS
    src/ui/pages/KeyPage.h
    src/ui/pages/MacroPage.h
    src/ui/pages/DriverPage.h
    src/ui/pages/SettingsPage.h
    src/ui/pages/AboutPage.h
)

add_executable(XiaoJiAutoKey WIN32
    include/app/input/GlobalInputHook.h
    include/app/input/InputKeyUtils.h
    include/app/input/RawInputManager.h
    include/app/config/ActiveConfigValidator.h
    include/app/controllers/ControllerResult.h
    include/app/config/ConfigNormalizer.h
    include/app/coordinators/SettingsCoordinator.h
    include/app/coordinators/DriverCoordinator.h
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
    include/app/coordinators/UpdateCoordinator.h
    include/app/controllers/MacroController.h
    include/app/widgets/StatusOverlayWindow.h
    include/app/MainWindow.h
    include/app/view/MainWindowView.h
    include/app/controllers/ProfileController.h
    include/app/services/AppPaths.h
    include/app/services/AutoStartService.h
    include/app/services/DdRuntimeService.h
    include/app/services/SoundService.h
    include/app/services/UpdateService.h
    include/app/dialogs/BindingEditDialog.h
    include/app/dialogs/AppMessageDialog.h
    include/app/dialogs/ConfigConfirmDialog.h
    include/app/dialogs/ConfigNameDialog.h
    include/app/dialogs/CloseConfirmDialog.h
    include/app/dialogs/GameProcessDialog.h
    include/app/dialogs/KeyCaptureDialogBase.h
    include/app/dialogs/MacroStepDialog.h
    include/app/dialogs/PauseKeyDialog.h
    include/app/dialogs/ShareExportDialog.h
    include/app/dialogs/ShareImportDialog.h
    include/app/dialogs/SoundSettingsDialog.h
    include/app/dialogs/StatusOverlaySettingsDialog.h
    include/app/dialogs/UpdateDialog.h
    include/core/ClickPositioning.h
    include/core/ForegroundWatcher.h
    include/core/ShareCodec.h
    include/core/MacroEngine.h
    include/core/InputDriver.h
    include/core/InputDriverRouter.h
    include/core/Ch552UsbDriver.h
    include/core/PicoUsbDriver.h
    src/app/input/GlobalInputHook.cpp
    src/app/input/RawInputManager.cpp
    src/app/config/ActiveConfigValidator.cpp
    src/app/config/ConfigNormalizer.cpp
    src/app/coordinators/SettingsCoordinator.cpp
    src/app/coordinators/DriverCoordinator.cpp
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
    src/app/coordinators/UpdateCoordinator.cpp
    src/app/controllers/MacroController.cpp
    src/app/widgets/StatusOverlayWindow.cpp
    src/app/main.cpp
    src/app/main_window/MainWindow.cpp
    src/app/main_window/MainWindowHooks.cpp
    src/app/main_window/MainWindowUi.cpp
    src/app/view/MainWindowView.cpp
    src/app/controllers/ProfileController.cpp
    src/app/services/AppPaths.cpp
    src/app/services/AutoStartService.cpp
    src/app/services/DdRuntimeService.cpp
    src/app/services/SoundService.cpp
    src/app/services/UpdateService.cpp
    src/app/services/AgentService.cpp
    src/app/dialogs/AppMessageDialog.cpp
    src/app/dialogs/ConfigConfirmDialog.cpp
    src/app/dialogs/ConfigNameDialog.cpp
    src/app/dialogs/DialogChrome.cpp
    src/app/dialogs/BindingEditDialog.cpp
    src/app/dialogs/CloseConfirmDialog.cpp
    src/app/dialogs/GameProcessDialog.cpp
    src/app/dialogs/KeyCaptureDialogBase.cpp
    src/app/dialogs/MacroStepDialog.cpp
    src/app/dialogs/PauseKeyDialog.cpp
    src/app/dialogs/ShareExportDialog.cpp
    src/app/dialogs/ShareImportDialog.cpp
    src/app/dialogs/SoundSettingsDialog.cpp
    src/app/dialogs/StatusOverlaySettingsDialog.cpp
    src/app/dialogs/UpdateDialog.cpp
    ${CORE_SOURCES}
    ${UI_COMPONENT_SOURCES}
    ${UI_COMPONENT_HEADERS}
    ${UI_PAGE_SOURCES}
    ${UI_PAGE_HEADERS}
    ${MOC_SOURCES}
    ${RESOURCE_SOURCES}
    # miniz (public domain / MIT) - used to unpack downloaded update archives.
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
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
    third_party/miniz
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
    Qt5::Svg
    shlwapi
    winmm
    winhttp
    dwmapi
    hid
    setupapi
    cfgmgr32
    advapi32
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
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AUTOKEY_DD_DLL32}"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver/DD32.dll"
        COMMENT "Copying DD32.dll to the target driver directory"
    )
endif()

if(EXISTS "${AUTOKEY_DD_DLL64}")
    add_custom_command(TARGET XiaoJiAutoKey POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AUTOKEY_DD_DLL64}"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver/DD64.dll"
        COMMENT "Copying DD64.dll to the target driver directory"
    )
endif()

if(EXISTS "${AUTOKEY_DD_HID_DLL}")
    add_custom_command(TARGET XiaoJiAutoKey POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver/hid"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AUTOKEY_DD_HID_DLL}"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver/hid/ddhid.63340.dll"
        COMMENT "Copying DD HID runtime DLL to the target driver directory"
    )
endif()

if(EXISTS "${AUTOKEY_DD_HID_DDC}" OR
   EXISTS "${AUTOKEY_DD_HID_CAT}" OR
   EXISTS "${AUTOKEY_DD_HID_INF}" OR
   EXISTS "${AUTOKEY_DD_HID_SYS}")
    add_custom_command(TARGET XiaoJiAutoKey POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver/hid/drv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AUTOKEY_DD_HID_DDC}"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver/hid/drv/ddc.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AUTOKEY_DD_HID_CAT}"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver/hid/drv/ddhid63340.cat"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AUTOKEY_DD_HID_INF}"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver/hid/drv/ddhid63340.inf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AUTOKEY_DD_HID_SYS}"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/driver/hid/drv/ddhid63340.sys"
        COMMENT "Copying DD HID driver helper files to the target driver directory"
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/assets/audio")
    add_custom_command(TARGET XiaoJiAutoKey POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/audio"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/audio"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/audio"
        COMMENT "Copying built-in audio files to the target audio directory"
    )
endif()

if(EXISTS "${AUTOKEY_USAGE_GUIDE}")
    add_custom_command(TARGET XiaoJiAutoKey POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AUTOKEY_USAGE_GUIDE}"
            "$<TARGET_FILE_DIR:XiaoJiAutoKey>/使用说明.mhtml"
        COMMENT "Copying offline usage guide to the target directory"
    )
endif()

add_custom_command(TARGET XiaoJiAutoKey POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "$<TARGET_FILE_DIR:XiaoJiAutoKey>/DD32.dll"
        "$<TARGET_FILE_DIR:XiaoJiAutoKey>/DD64.dll"
    COMMENT "Removing legacy DD DLLs from the executable root directory"
)
