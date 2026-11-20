cmake_minimum_required(VERSION 3.20)

project(XiaoJiAutoKey VERSION 1.1.7 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Svg Qml Quick QuickWidgets QuickControls2 ShaderTools)
qt_standard_project_setup(REQUIRES 6.8)

set(MOC_HEADERS
    include/app/dialogs/KeyOverviewDialog.h
    include/app/dialogs/QmlDialog.h
    include/app/view/MainWindowView.h
    src/ui/models/PageModel.h
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
    include/app/coordinators/UsageStatisticsCoordinator.h
    include/app/coordinators/DriverSelfCheckCoordinator.h
    include/app/dialogs/DriverDiagnosticDialog.h
    include/app/coordinators/OnboardingCoordinator.h
    include/app/dialogs/BindingEditDialog.h
    include/app/dialogs/BatchEditDialog.h
    include/app/dialogs/AppMessageDialog.h
    include/app/dialogs/DriverSelfCheckDialog.h
    include/app/dialogs/CloseConfirmDialog.h
    include/app/dialogs/KeyCaptureDialogBase.h
    include/app/dialogs/MacroStepDialog.h
    include/app/dialogs/MacroRecordingDialog.h
    include/app/dialogs/PauseKeyDialog.h
    include/app/dialogs/SequenceSwitchKeyDialog.h
    include/app/dialogs/ShareExportDialog.h
    include/app/dialogs/ShareImportDialog.h
    include/app/dialogs/SoundSettingsDialog.h
    include/app/dialogs/StatusOverlaySettingsDialog.h
    include/app/dialogs/UpdateDialog.h
    include/app/widgets/StatusOverlayWindow.h
    src/ui/components/ScreenPointPicker.h
    src/ui/models/KeyPageModel.h
    src/ui/models/MacroPageModel.h
    src/ui/models/DriverPageModel.h
    src/ui/models/SettingsPageModel.h
    src/ui/models/AboutPageModel.h
    src/ui/models/UsagePageModel.h
    src/ui/models/ShareImportBannerModel.h
    include/app/coordinators/UpdateCoordinator.h
    include/app/services/UpdateService.h
)

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
    src/core/DiagLog.cpp
    src/core/FactionTheme.cpp
    src/core/ForegroundWatcher.cpp
    src/core/InputDriverRouter.cpp
    src/core/JsonLite.cpp
    src/core/MacroEngine.cpp
    src/core/MacroTiming.cpp
    src/core/MacroActions.cpp
    src/core/MacroValidation.cpp
    src/core/MacroRecording.cpp
    src/core/Ch552UsbDriver.cpp
    src/core/PicoUsbDriver.cpp
    src/core/RepeaterEngine.cpp
    src/core/RepeatModeRules.cpp
    src/core/ShareCodec.cpp
    src/core/SoundSettings.cpp
)

set(UI_COMPONENT_SOURCES
    src/ui/components/QmlIconProvider.cpp
    src/ui/components/ScreenPointPicker.cpp
)

set(UI_MODEL_SOURCES
    src/ui/models/KeyOverviewData.cpp
    src/ui/models/KeyPageModel.cpp
    src/ui/models/MacroPageModel.cpp
    src/ui/models/DriverPageModel.cpp
    src/ui/models/SettingsPageModel.cpp
    src/ui/models/AboutPageModel.cpp
    src/ui/models/UsagePageModel.cpp
    src/ui/models/ShareImportBannerModel.cpp
    src/ui/models/SoundSettingsData.cpp
)

set(UI_COMPONENT_HEADERS
    src/ui/components/QmlIconProvider.h
    src/ui/components/ScreenPointPicker.h
)

set(UI_MODEL_HEADERS
    src/ui/models/KeyOverviewData.h
    src/ui/models/UsagePageModel.h
    src/ui/models/ShareImportBannerModel.h
    src/ui/models/KeyPageModel.h
    src/ui/models/MacroPageModel.h
    src/ui/models/DriverPageModel.h
    src/ui/models/SettingsPageModel.h
    src/ui/models/AboutPageModel.h
)

qt_add_executable(XiaoJiAutoKey MANUAL_FINALIZATION WIN32
    include/app/input/GlobalInputHook.h
    include/app/input/InputKeyUtils.h
    include/app/input/RawInputManager.h
    include/app/config/ActiveConfigValidator.h
    include/app/controllers/ControllerResult.h
    include/app/config/ConfigNormalizer.h
    include/app/diagnostics/DriverDiagnosticReport.h
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
    include/app/coordinators/DriverSelfCheckCoordinator.h
    include/app/coordinators/UpdateCoordinator.h
    include/app/controllers/MacroController.h
    include/app/widgets/StatusOverlayWindow.h
    include/app/MainWindow.h
    include/app/view/MainWindowView.h
    include/app/controllers/ProfileController.h
    include/app/services/AppPaths.h
    include/app/services/AutoStartService.h
    include/app/services/CodeIntegrityInspector.h
    include/app/services/DdRuntimeService.h
    include/app/services/SoundService.h
    include/app/services/UpdateService.h
    include/app/dialogs/BindingEditDialog.h
    include/app/dialogs/AppMessageDialog.h
    include/app/dialogs/ConfigConfirmDialog.h
    include/app/dialogs/ConfigNameDialog.h
    include/app/dialogs/ShareNamePreview.h
    include/app/dialogs/DriverSelfCheckDialog.h
    include/app/dialogs/CloseConfirmDialog.h
    include/app/dialogs/GameProcessDialog.h
    include/app/dialogs/KeyCaptureDialogBase.h
    include/app/dialogs/MacroStepDialog.h
    include/app/dialogs/MacroRecordingDialog.h
    include/app/dialogs/PauseKeyDialog.h
    include/app/dialogs/SequenceSwitchKeyDialog.h
    include/app/dialogs/ShareExportDialog.h
    include/app/dialogs/ShareImportDialog.h
    include/app/dialogs/SoundSettingsDialog.h
    include/app/dialogs/StatusOverlaySettingsDialog.h
    include/app/dialogs/UpdateDialog.h
    include/core/ClickPositioning.h
    include/core/DiagLog.h
    include/core/ForegroundWatcher.h
    include/core/ShareCodec.h
    include/core/MacroEngine.h
    include/core/MacroTiming.h
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
    src/app/coordinators/UsageStatisticsCoordinator.cpp
    src/app/coordinators/DriverSelfCheckCoordinator.cpp
    src/app/diagnostics/DriverDiagnosticReport.cpp
    src/app/coordinators/OnboardingCoordinator.cpp
    src/app/coordinators/UpdateCoordinator.cpp
    src/app/controllers/MacroController.cpp
    src/app/widgets/StatusOverlayWindow.cpp
    src/app/widgets/OverlayBackdropCapture.cpp
    include/app/widgets/OverlayBackdropCapture.h
    src/app/main.cpp
    src/app/main_window/MainWindow.cpp
    src/app/main_window/MainWindowHooks.cpp
    src/app/main_window/MainWindowUi.cpp
    src/app/view/MainWindowView.cpp
    src/app/controllers/ProfileController.cpp
    src/app/services/AppPaths.cpp
    src/app/services/AutoStartService.cpp
    src/app/services/CodeIntegrityInspector.cpp
    src/app/services/DdRuntimeService.cpp
    src/app/services/SoundService.cpp
    src/app/services/UpdateService.cpp
    src/app/services/AgentService.cpp
    src/app/dialogs/AppMessageDialog.cpp
    src/app/dialogs/QmlDialog.cpp
    src/app/dialogs/KeyOverviewDialog.cpp
    src/app/dialogs/ConfigConfirmDialog.cpp
    src/app/dialogs/ConfigNameDialog.cpp
    src/app/dialogs/DialogChrome.cpp
    src/app/dialogs/DriverSelfCheckDialog.cpp
    src/app/dialogs/DriverDiagnosticDialog.cpp
    src/app/dialogs/BindingEditDialog.cpp
    src/app/dialogs/BatchEditDialog.cpp
    src/app/dialogs/CloseConfirmDialog.cpp
    src/app/dialogs/GameProcessDialog.cpp
    src/app/dialogs/KeyCaptureDialogBase.cpp
    src/app/dialogs/MacroStepDialog.cpp
    src/app/dialogs/MacroRecordingDialog.cpp
    src/app/dialogs/PauseKeyDialog.cpp
    src/app/dialogs/SequenceSwitchKeyDialog.cpp
    src/app/dialogs/ShareExportDialog.cpp
    src/app/dialogs/ShareImportDialog.cpp
    src/app/dialogs/SoundSettingsDialog.cpp
    src/app/dialogs/StatusOverlaySettingsDialog.cpp
    src/app/dialogs/UpdateDialog.cpp
    ${CORE_SOURCES}
    ${UI_COMPONENT_SOURCES}
    ${UI_COMPONENT_HEADERS}
    ${UI_MODEL_SOURCES}
    ${UI_MODEL_HEADERS}
    ${MOC_HEADERS}
    resources.qrc
    # miniz (public domain / MIT) - used to unpack downloaded update archives.
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
    app.rc
)

set(QML_FILES
    src/qml/Main.qml
    src/qml/Theme.qml
    src/qml/Typography.qml
    src/qml/AppShell.qml
    src/qml/OnboardingOverlay.qml
    src/qml/GuideHint.qml
    src/qml/TitleBar.qml
    src/qml/CaptionButton.qml
    src/qml/ShareImportBanner.qml
    src/qml/QuietLabel.qml
    src/qml/QuietHeading.qml
    src/qml/QuietIcon.qml
    src/qml/QuietToolTip.qml
    src/qml/ActionButton.qml
    src/qml/IconButton.qml
    src/qml/QuietCheck.qml
    src/qml/QuietSwitch.qml
    src/qml/QuietRadio.qml
    src/qml/QuietCombo.qml
    src/qml/QuietNumber.qml
    src/qml/QuietScrollBar.qml
    src/qml/QuietScrollView.qml
    src/qml/QuietCard.qml
    src/qml/FactionPicker.qml
    src/qml/FactionSymbol.qml
    src/qml/NavIcon.qml
    src/qml/SidebarNavigation.qml
    src/qml/ProfileToolbar.qml
    src/qml/ProfileMoreMenu.qml
    src/qml/NameCell.qml
    src/qml/RunPanel.qml
    src/qml/HotkeyPanel.qml
    src/qml/RepeatOptions.qml
    src/qml/MacroOptions.qml
    src/qml/ShortcutBar.qml
    src/qml/ShortcutTag.qml
    src/qml/PauseKeyList.qml
    src/qml/BindingRow.qml
    src/qml/KeyCell.qml
    src/qml/MacroStepRow.qml
    src/qml/MacroStepMenu.qml
    src/qml/WorkspaceRows.qml
    src/qml/WorkspaceTable.qml
    src/qml/MacroTimingSummary.qml
    src/qml/WorkspaceEnabledCell.qml
    src/qml/KeyWorkspace.qml
    src/qml/KeyPage.qml
    src/qml/MacroPage.qml
    src/qml/UsagePage.qml
    src/qml/UsageHeatmap.qml
    src/qml/UsageMonthlyTrend.qml
    src/qml/DriverPage.qml
    src/qml/SettingsPage.qml
    src/qml/AboutPage.qml
    src/qml/OverlayGlass.qml
    src/qml/OverlayWindow.qml
    src/qml/OverlayStatusBlock.qml
    src/qml/OverlayLock.qml
    src/qml/DialogShell.qml
    src/qml/DialogKeyOverview.qml
    src/qml/KeyOverviewKeyboard.qml
    src/qml/KeyOverviewKey.qml
    src/qml/KeyOverviewDetails.qml
    src/qml/KeyOverviewPopup.qml
    src/qml/DialogMessage.qml
    src/qml/DialogName.qml
    src/qml/DialogClose.qml
    src/qml/DialogMacroRecording.qml
    src/qml/DialogMacroParameters.qml
    src/qml/DialogKeyEditor.qml
    src/qml/DialogBatchEdit.qml
    src/qml/DialogOverlaySettings.qml
    src/qml/DialogSound.qml
    src/qml/SoundEventList.qml
    src/qml/DialogProcesses.qml
    src/qml/DialogShare.qml
    src/qml/DialogReport.qml
    src/qml/PointPicker.qml
    src/qml/QuietTextField.qml
    src/qml/QuietTextArea.qml
    src/qml/TextEditMenu.qml
    src/qml/QuietMenuItem.qml
    src/qml/QuietSlider.qml
    src/qml/QuietProgress.qml
)
foreach(qml_file IN LISTS QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
set_source_files_properties(src/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(src/qml/Typography.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(XiaoJiAutoKey
    URI XiaoJi.UI
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${QML_FILES}
)
qt_add_shaders(XiaoJiAutoKey overlay_glass
    PREFIX /shaders
    BASE src/qml/shaders
    FILES src/qml/shaders/glass-refract.frag
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
    src/ui/models
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
    Qt6::Widgets
    Qt6::Svg
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::QuickControls2
    shlwapi
    winmm
    winhttp
    dwmapi
    d3d11
    dxgi
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

qt_finalize_executable(XiaoJiAutoKey)
