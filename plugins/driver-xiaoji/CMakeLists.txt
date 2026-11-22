cmake_minimum_required(VERSION 3.20)
project(XiaoJiPluginXiaoji VERSION 1.0.0 LANGUAGES C CXX)
set(XIAOJI_PLUGIN_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk" CACHE PATH "XiaoJi Plugin SDK source")
if(NOT TARGET XiaoJi::PluginSDK)
    add_subdirectory("${XIAOJI_PLUGIN_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/sdk")
endif()
add_library(XiaoJiPluginNative STATIC third_party/xiaoji/xiaoji_native.cpp)
target_include_directories(XiaoJiPluginNative PUBLIC third_party/xiaoji)
target_compile_features(XiaoJiPluginNative PUBLIC cxx_std_20)
target_compile_definitions(XiaoJiPluginNative PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(XiaoJiPluginNative PUBLIC setupapi cfgmgr32 newdev advapi32 shell32 ole32)
set_target_properties(XiaoJiPluginNative PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF)
if(MSVC)
    target_compile_options(XiaoJiPluginNative PRIVATE /utf-8 /W4 /permissive-)
    set_property(TARGET XiaoJiPluginNative PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
xiaoji_add_plugin(XiaoJiPluginXiaoji MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    OUTPUT_NAME xiaoji-plugin-xiaoji
    SOURCES src/Plugin.cpp src/XiaojiDriver.cpp src/XiaojiKeyMap.cpp
    LIBRARIES XiaoJiPluginNative user32)
target_include_directories(XiaoJiPluginXiaoji PRIVATE src)

add_executable(XiaoJiPluginManager third_party/xiaoji/management_cli.cpp)
target_link_libraries(XiaoJiPluginManager PRIVATE XiaoJiPluginNative wintrust crypt32)
set_target_properties(XiaoJiPluginManager PROPERTIES OUTPUT_NAME xiaoji AUTOMOC OFF AUTORCC OFF AUTOUIC OFF)
target_compile_definitions(XiaoJiPluginManager PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(XiaoJiPluginManager PRIVATE /utf-8 /W4 /permissive-)
    set_property(TARGET XiaoJiPluginManager PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
find_program(XIAOJI_PLUGIN_POWERSHELL NAMES powershell.exe REQUIRED)
set(XIAOJI_PLUGIN_INSTALLER "${CMAKE_CURRENT_BINARY_DIR}/installer/xiaoji-setup.exe")
add_custom_command(OUTPUT "${XIAOJI_PLUGIN_INSTALLER}"
    COMMAND "${XIAOJI_PLUGIN_POWERSHELL}" -NoProfile -NonInteractive -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-xiaoji-installer.ps1" -OutputPath "${XIAOJI_PLUGIN_INSTALLER}"
    DEPENDS scripts/build-xiaoji-installer.ps1 third_party/xiaoji/LocalInstaller.cs
        third_party/xiaoji/LocalSetup.cs third_party/xiaoji/DriverStore.cs
        third_party/xiaoji/driver/xiaoji.inf third_party/xiaoji/driver/xiaoji.dll
        third_party/xiaoji/driver/xiaoji.cat third_party/xiaoji/driver/payload.json
    VERBATIM)
add_custom_target(XiaoJiPluginInstaller DEPENDS "${XIAOJI_PLUGIN_INSTALLER}")
add_dependencies(XiaoJiPluginXiaojiPackage XiaoJiPluginManager XiaoJiPluginInstaller)
add_custom_command(TARGET XiaoJiPluginXiaojiPackage POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:XiaoJiPluginXiaoji>/driver/xiaoji"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:XiaoJiPluginManager>" "$<TARGET_FILE_DIR:XiaoJiPluginXiaoji>/driver/xiaoji/xiaoji.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${XIAOJI_PLUGIN_INSTALLER}" "$<TARGET_FILE_DIR:XiaoJiPluginXiaoji>/driver/xiaoji/xiaoji-setup.exe"
    VERBATIM)
foreach(license LICENSE-AGPL-3.0 LICENSE-HIDMaestro-MIT)
    add_custom_command(TARGET XiaoJiPluginXiaojiPackage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third_party/xiaoji/${license}" "$<TARGET_FILE_DIR:XiaoJiPluginXiaoji>/driver/xiaoji/${license}"
        VERBATIM)
endforeach()
if(XIAOJI_SIGNED_DRIVER_DIR)
    foreach(payload xiaoji.inf xiaoji.dll xiaoji.cat)
        if(NOT EXISTS "${XIAOJI_SIGNED_DRIVER_DIR}/${payload}")
            message(FATAL_ERROR "Signed XiaojiDriver package is incomplete: ${payload}")
        endif()
        add_custom_command(TARGET XiaoJiPluginXiaojiPackage POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:XiaoJiPluginXiaoji>/driver/xiaoji/package"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${XIAOJI_SIGNED_DRIVER_DIR}/${payload}"
                "$<TARGET_FILE_DIR:XiaoJiPluginXiaoji>/driver/xiaoji/package/${payload}"
            VERBATIM)
    endforeach()
endif()
