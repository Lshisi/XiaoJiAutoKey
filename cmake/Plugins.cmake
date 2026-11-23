set(XIAOJI_PLUGIN_SDK "${CMAKE_CURRENT_SOURCE_DIR}/sdk" CACHE PATH "XiaoJi Plugin SDK source")
if(NOT TARGET XiaoJi::PluginSDK)
    add_subdirectory("${XIAOJI_PLUGIN_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/plugin-sdk")
endif()
add_subdirectory(plugins/driver-dd)
add_subdirectory(plugins/driver-xiaoji)
add_subdirectory(plugins/driver-usb EXCLUDE_FROM_ALL)

function(xiaoji_deploy_builtin_plugins target)
    add_custom_target(${target}BuiltinPlugins)
    add_dependencies(${target} ${target}BuiltinPlugins)
    foreach(plugin_target XiaoJiPluginXiaoji XiaoJiPluginDD)
        get_target_property(plugin_id ${plugin_target} XIAOJI_PLUGIN_ID)
        get_target_property(plugin_version ${plugin_target} XIAOJI_PLUGIN_VERSION)
        add_dependencies(${target}BuiltinPlugins ${plugin_target}Package)
        set(destination "$<TARGET_FILE_DIR:${target}>/plugins/builtin/${plugin_id}/${plugin_version}")
        add_custom_command(TARGET ${target}BuiltinPlugins POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${destination}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${plugin_target}>" "${destination}/$<TARGET_FILE_NAME:${plugin_target}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE_DIR:${plugin_target}>/plugin.json" "${destination}/plugin.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE_DIR:${plugin_target}>/LICENSE" "${destination}/LICENSE"
            VERBATIM)
        add_custom_command(TARGET ${target}BuiltinPlugins POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:${plugin_target}>/driver" "${destination}/driver"
            VERBATIM)
    endforeach()
endfunction()
