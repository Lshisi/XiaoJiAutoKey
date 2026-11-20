cmake_minimum_required(VERSION 3.20)
project(XiaoJiPreview LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg ShaderTools)
qt_standard_project_setup(REQUIRES 6.8)

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
qt_add_executable(XiaoJiPreview MACOSX_BUNDLE WIN32
    main.cpp
    PreviewController.h
    PreviewController.cpp
    PreviewExamples.cpp
    PreviewWorkspaceModel.h
    PreviewWorkspaceModel.cpp
    PreviewWorkspaceController.h
    PreviewWorkspaceController.cpp
    PreviewDialogModel.h
    PreviewKeyInput.h
    PreviewDialogModel.cpp
    PreviewOnboardingModel.h
    PreviewOnboardingModel.cpp
    ${PROJECT_ROOT}/src/ui/models/PageModel.h
    ${PROJECT_ROOT}/src/ui/models/KeyOverviewData.h
    ${PROJECT_ROOT}/src/ui/models/KeyOverviewData.cpp
    ${PROJECT_ROOT}/src/ui/models/UsagePageModel.h
    ${PROJECT_ROOT}/src/ui/models/UsagePageModel.cpp
    ${PROJECT_ROOT}/src/ui/models/DriverPageModel.h
    ${PROJECT_ROOT}/src/ui/models/DriverPageModel.cpp
    ${PROJECT_ROOT}/src/ui/models/SettingsPageModel.h
    ${PROJECT_ROOT}/src/ui/models/SettingsPageModel.cpp
    ${PROJECT_ROOT}/src/ui/models/AboutPageModel.h
    ${PROJECT_ROOT}/src/ui/models/AboutPageModel.cpp
    ${PROJECT_ROOT}/src/ui/models/ShareImportBannerModel.h
    ${PROJECT_ROOT}/src/ui/models/ShareImportBannerModel.cpp
    ${PROJECT_ROOT}/src/core/FactionTheme.cpp
    ${PROJECT_ROOT}/src/core/MacroTiming.cpp
    ${PROJECT_ROOT}/src/core/MacroActions.cpp
    ${PROJECT_ROOT}/src/core/MacroRecording.cpp
    ${PROJECT_ROOT}/src/core/RepeatModeRules.cpp
    ${PROJECT_ROOT}/src/core/SoundSettings.cpp
    ${PROJECT_ROOT}/src/ui/models/SoundSettingsData.cpp
    ${PROJECT_ROOT}/src/ui/components/QmlIconProvider.h
    ${PROJECT_ROOT}/src/ui/components/QmlIconProvider.cpp
)

file(GLOB PREVIEW_QML_FILES CONFIGURE_DEPENDS "${PROJECT_ROOT}/src/qml/*.qml")
list(APPEND PREVIEW_QML_FILES PreviewWindow.qml PreviewDialogWindow.qml PreviewOverlayWindow.qml PreviewPickerWindow.qml)
foreach(qml_file IN LISTS PREVIEW_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
    if(WIN32 AND IS_ABSOLUTE "${qml_file}")
        # Qt 6.8 cachegen puts a trailing ".." in the shared-source output directory.
        set_source_files_properties("${qml_file}" PROPERTIES QT_QML_SKIP_CACHEGEN TRUE)
    endif()
endforeach()
set_source_files_properties("${PROJECT_ROOT}/src/qml/Theme.qml" PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties("${PROJECT_ROOT}/src/qml/Typography.qml" PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(XiaoJiPreview
    URI XiaoJi.UI
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${PREVIEW_QML_FILES}
)
target_include_directories(XiaoJiPreview PRIVATE
    ${PROJECT_ROOT}/include/core
    ${PROJECT_ROOT}/src
    ${PROJECT_ROOT}/src/ui/styles
)
target_link_libraries(XiaoJiPreview PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Svg
)
file(STRINGS "${PROJECT_ROOT}/CMakeLists.txt" APP_PROJECT_LINE REGEX "^project\\(XiaoJiAutoKey VERSION")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" PREVIEW_APP_VERSION "${APP_PROJECT_LINE}")
target_compile_definitions(XiaoJiPreview PRIVATE APP_VERSION="${PREVIEW_APP_VERSION}" NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(XiaoJiPreview PRIVATE /utf-8 /W4 /permissive-)
endif()
file(GLOB FACTION_ICONS CONFIGURE_DEPENDS "${PROJECT_ROOT}/assets/factions/*.svg")
file(GLOB CONTROL_ICONS CONFIGURE_DEPENDS "${PROJECT_ROOT}/assets/icons/*.svg")
qt_add_resources(XiaoJiPreview preview_assets
    PREFIX "/" BASE "${PROJECT_ROOT}"
    FILES "${PROJECT_ROOT}/assets/icon.png" "${PROJECT_ROOT}/assets/icon-active.png" ${FACTION_ICONS} ${CONTROL_ICONS}
)
qt_add_shaders(XiaoJiPreview preview_shaders
    PREFIX /shaders BASE "${PROJECT_ROOT}/src/qml/shaders"
    FILES "${PROJECT_ROOT}/src/qml/shaders/glass-refract.frag"
)
