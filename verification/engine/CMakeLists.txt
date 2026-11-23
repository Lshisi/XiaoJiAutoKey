cmake_minimum_required(VERSION 3.21)
project(XiaojiEngineVerification LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Engine verification currently requires Windows.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)
get_filename_component(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

add_executable(engine-checks
    EngineChecks.cpp RegressionCases.cpp ScenarioRunner.cpp RecordingDriver.cpp
    "${REPO}/include/app/input/GlobalInputHook.h"
    "${REPO}/include/app/input/RawInputManager.h"
    "${REPO}/include/app/coordinators/EngineHookCoordinator.h"
    "${REPO}/src/app/input/GlobalInputHook.cpp"
    "${REPO}/src/app/input/RawInputManager.cpp"
    "${REPO}/src/app/coordinators/EngineHookCoordinator.cpp"
    "${REPO}/src/core/AppTypes.cpp"
    "${REPO}/src/core/ClickPositioning.cpp"
    "${REPO}/src/core/ConfigStore.cpp"
    "${REPO}/src/core/DdDriver.cpp"
    "${REPO}/src/core/XiaojiDriver.cpp"
    "${REPO}/src/core/XiaojiKeyMap.cpp"
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
    "${REPO}/src/core/SoundSettings.cpp"
    "${REPO}/third_party/xiaoji/xiaoji_native.cpp")
target_include_directories(engine-checks PRIVATE
    "${REPO}/include/core" "${REPO}/include/app" "${REPO}/third_party/xiaoji")
target_compile_definitions(engine-checks PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(engine-checks PRIVATE /utf-8 /W4 /permissive-)
set_property(TARGET engine-checks PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(engine-checks PRIVATE Qt6::Core Qt6::Widgets
    user32 shell32 ole32 setupapi cfgmgr32 newdev advapi32 winmm)
