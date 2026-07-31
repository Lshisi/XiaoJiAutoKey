if(NOT DEFINED SOURCE_DIR OR NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "SOURCE_DIR and DEST_DIR are required")
endif()

set(REQUIRED_FILES
    XiaoJiVhid.sys
    XiaoJiVhid.inf
    XiaoJiVhid.cat
)

foreach(FILE_NAME IN LISTS REQUIRED_FILES)
    if(NOT EXISTS "${SOURCE_DIR}/${FILE_NAME}")
        message(STATUS "XiaoJiVhid package not copied; missing ${SOURCE_DIR}/${FILE_NAME}")
        return()
    endif()
endforeach()

file(MAKE_DIRECTORY "${DEST_DIR}")
foreach(FILE_NAME IN LISTS REQUIRED_FILES)
    configure_file("${SOURCE_DIR}/${FILE_NAME}" "${DEST_DIR}/${FILE_NAME}" COPYONLY)
endforeach()
