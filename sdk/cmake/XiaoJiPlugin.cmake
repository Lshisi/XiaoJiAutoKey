function(xiaoji_add_plugin target)
    cmake_parse_arguments(PLUGIN "" "MANIFEST;OUTPUT_NAME" "SOURCES;LIBRARIES;RESOURCES" ${ARGN})
    if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Native XiaoJi plugins require Windows x64")
    endif()
    if(NOT PLUGIN_MANIFEST OR NOT EXISTS "${PLUGIN_MANIFEST}")
        message(FATAL_ERROR "A plugin.json manifest is required")
    endif()
    file(READ "${PLUGIN_MANIFEST}" manifest_json)
    string(JSON plugin_id GET "${manifest_json}" id)
    string(JSON plugin_version GET "${manifest_json}" version)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PLUGIN_MANIFEST}")
    add_library(${target} SHARED ${PLUGIN_SOURCES})
    target_link_libraries(${target} PRIVATE XiaoJi::PluginSDK ${PLUGIN_LIBRARIES})
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
        XIAOJI_PLUGIN_ID="${plugin_id}" XIAOJI_PLUGIN_VERSION="${plugin_version}")
    set_target_properties(${target} PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF
        XIAOJI_PLUGIN_ID "${plugin_id}" XIAOJI_PLUGIN_VERSION "${plugin_version}"
        # Qt's global runtime directory can otherwise merge unrelated manifests and payloads.
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    if(PLUGIN_OUTPUT_NAME)
        set_target_properties(${target} PROPERTIES OUTPUT_NAME "${PLUGIN_OUTPUT_NAME}")
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4 /permissive-)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    # Explicit package target also refreshes payloads when the DLL did not relink.
    add_custom_target(${target}Package ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PLUGIN_MANIFEST}" "$<TARGET_FILE_DIR:${target}>/plugin.json"
        DEPENDS ${target}
        VERBATIM)
    get_filename_component(manifest_directory "${PLUGIN_MANIFEST}" DIRECTORY)
    if(EXISTS "${manifest_directory}/LICENSE")
        add_custom_command(TARGET ${target}Package POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${manifest_directory}/LICENSE" "$<TARGET_FILE_DIR:${target}>/LICENSE"
            VERBATIM)
    endif()
    foreach(resource IN LISTS PLUGIN_RESOURCES)
        get_filename_component(resource_name "${resource}" NAME)
        add_custom_command(TARGET ${target}Package POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${resource}" "$<TARGET_FILE_DIR:${target}>/${resource_name}"
            VERBATIM)
    endforeach()
endfunction()
