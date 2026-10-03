set(ONNXRUNTIME_VERSION 1.29.0)

if(WIN32)
    set(onnxruntime_platform win)
    set(onnxruntime_extension zip)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(onnxruntime_platform linux)
    set(onnxruntime_extension tgz)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

set(onnxruntime_archive "onnxruntime-${onnxruntime_platform}-x64-${ONNXRUNTIME_VERSION}.${onnxruntime_extension}")

FetchContent_Declare(onnxruntime
    URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/${onnxruntime_archive}"
    SOURCE_SUBDIR prebuilt
)
FetchContent_MakeAvailable(onnxruntime)

add_library(onnxruntime::onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime::onnxruntime PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${onnxruntime_SOURCE_DIR}/include"
)
if(WIN32)
    set_target_properties(onnxruntime::onnxruntime PROPERTIES
        IMPORTED_IMPLIB "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.lib"
        IMPORTED_LOCATION "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll"
    )
    file(GLOB onnxruntime_libraries "${onnxruntime_SOURCE_DIR}/lib/*.dll")
else()
    set_target_properties(onnxruntime::onnxruntime PROPERTIES
        IMPORTED_LOCATION "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so.${ONNXRUNTIME_VERSION}"
    )
    file(GLOB onnxruntime_libraries "${onnxruntime_SOURCE_DIR}/lib/*.so*")
endif()

function(copy_onnxruntime target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${onnxruntime_libraries} "$<TARGET_FILE_DIR:${target}>"
        VERBATIM
    )
endfunction()
