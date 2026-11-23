cmake_minimum_required(VERSION 3.21)
project(XiaojiEngineVerification LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Engine verification currently requires Windows.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)
get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
file(READ "${REPO}/CMakeLists.txt" host_cmake)
string(REGEX MATCH "project\\(XiaoJiAutoKey VERSION ([0-9.]+)" host_project "${host_cmake}")
set(host_version "${CMAKE_MATCH_1}")
if(NOT host_version)
    message(FATAL_ERROR "Cannot determine the production host version.")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${REPO}/CMakeLists.txt")
add_subdirectory("${REPO}/sdk" "${CMAKE_CURRENT_BINARY_DIR}/plugin-sdk")
add_subdirectory("${REPO}/plugins/driver-dd" "${CMAKE_CURRENT_BINARY_DIR}/plugins/driver-dd" EXCLUDE_FROM_ALL)
add_subdirectory("${REPO}/plugins/driver-xiaoji" "${CMAKE_CURRENT_BINARY_DIR}/plugins/driver-xiaoji" EXCLUDE_FROM_ALL)

add_executable(engine-checks
    EngineChecks.cpp RegressionCases.cpp ScenarioRunner.cpp RecordingDriver.cpp
    "${REPO}/include/app/input/GlobalInputHook.h"
    "${REPO}/include/app/input/RawInputManager.h"
    "${REPO}/include/app/coordinators/EngineHookCoordinator.h"
    "${REPO}/include/core/plugins/NativePluginModule.h"
    "${REPO}/src/app/input/GlobalInputHook.cpp"
    "${REPO}/src/app/input/RawInputManager.cpp"
    "${REPO}/src/app/coordinators/EngineHookCoordinator.cpp"
    "${REPO}/src/core/AppTypes.cpp"
    "${REPO}/src/core/ClickPositioning.cpp"
    "${REPO}/src/core/ConfigStore.cpp"
    "${REPO}/src/core/plugins/NativePluginDriver.cpp"
    "${REPO}/src/core/plugins/NativePluginModule.cpp"
    "${REPO}/src/core/plugins/PluginManifest.cpp"
    "${REPO}/src/core/DiagLog.cpp"
    "${REPO}/src/core/InputDriverRouter.cpp"
    "${REPO}/src/core/PauseModifierGate.cpp"
    "${REPO}/src/core/JsonLite.cpp"
    "${REPO}/src/core/MacroEngine.cpp"
    "${REPO}/src/core/MacroActions.cpp"
    "${REPO}/src/core/MacroTiming.cpp"
    "${REPO}/src/core/MacroValidation.cpp"
    "${REPO}/src/core/RepeaterEngine.cpp"
    "${REPO}/src/core/RepeatModeRules.cpp"
    "${REPO}/src/core/SoundSettings.cpp")
target_include_directories(engine-checks PRIVATE
    "${REPO}/include" "${REPO}/include/core" "${REPO}/include/app")
target_compile_definitions(engine-checks PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    APP_VERSION="${host_version}"
    XIAOJI_VERIFICATION_DD_MANIFEST="$<TARGET_FILE_DIR:XiaoJiPluginDD>/plugin.json"
    XIAOJI_VERIFICATION_XIAOJI_MANIFEST="$<TARGET_FILE_DIR:XiaoJiPluginXiaoji>/plugin.json")
target_compile_options(engine-checks PRIVATE /utf-8 /W4 /permissive-)
set_property(TARGET engine-checks PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(engine-checks PRIVATE Qt6::Core Qt6::Widgets XiaoJi::PluginSDK
    user32 shell32 ole32 setupapi cfgmgr32 newdev advapi32 winmm)
add_dependencies(engine-checks XiaoJiPluginDDPackage XiaoJiPluginXiaojiPackage)
