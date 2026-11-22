cmake_minimum_required(VERSION 3.20)
project(XiaoJiPluginSDK VERSION 1.0.0 LANGUAGES C CXX)

add_library(XiaoJiPluginSDK INTERFACE)
add_library(XiaoJi::PluginSDK ALIAS XiaoJiPluginSDK)
target_include_directories(XiaoJiPluginSDK INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(XiaoJiPluginSDK INTERFACE cxx_std_20)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
install(TARGETS XiaoJiPluginSDK EXPORT XiaoJiPluginSDKTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE README.md plugin.schema.json DESTINATION ${CMAKE_INSTALL_DATADIR}/xiaoji-plugin-sdk)
install(DIRECTORY examples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/xiaoji-plugin-sdk/examples PATTERN "build*" EXCLUDE)
install(FILES cmake/XiaoJiPlugin.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/XiaoJiPluginSDK)
install(EXPORT XiaoJiPluginSDKTargets NAMESPACE XiaoJi:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/XiaoJiPluginSDK)
configure_package_config_file(cmake/XiaoJiPluginSDKConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/XiaoJiPluginSDKConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/XiaoJiPluginSDK)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/XiaoJiPluginSDKConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/XiaoJiPluginSDKConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/XiaoJiPluginSDKConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/XiaoJiPluginSDK)
include(cmake/XiaoJiPlugin.cmake)
