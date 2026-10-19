find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(ARCANUM_FFMPEG_ROOT "${CMAKE_BINARY_DIR}/ffmpeg" CACHE PATH "FFmpeg Bink decoder installation")
set(ffmpeg_extra_args)
if(MSVC)
    list(APPEND ffmpeg_extra_args --msvc)
endif()
file(MAKE_DIRECTORY "${ARCANUM_FFMPEG_ROOT}/include")
set(ffmpeg_outputs)
set(ffmpeg_byproducts)
foreach(pair avformat:62 avcodec:62 avutil:60)
    string(REPLACE ":" ";" fields "${pair}")
    list(GET fields 0 name)
    list(GET fields 1 version)
    add_library(ffmpeg_${name} SHARED IMPORTED)
    if(WIN32)
        set(library "${ARCANUM_FFMPEG_ROOT}/bin/${name}-${version}.dll")
        if(MSVC)
            set(import_library "${ARCANUM_FFMPEG_ROOT}/lib/${name}.lib")
        else()
            set(import_library "${ARCANUM_FFMPEG_ROOT}/lib/lib${name}.dll.a")
        endif()
        set_target_properties(ffmpeg_${name} PROPERTIES IMPORTED_IMPLIB "${import_library}")
        list(APPEND ffmpeg_byproducts "${import_library}")
    elseif(APPLE)
        set(library "${ARCANUM_FFMPEG_ROOT}/lib/lib${name}.${version}.dylib")
    else()
        set(library "${ARCANUM_FFMPEG_ROOT}/lib/lib${name}.so.${version}")
    endif()
    set_target_properties(ffmpeg_${name} PROPERTIES IMPORTED_LOCATION "${library}"
        INTERFACE_INCLUDE_DIRECTORIES "${ARCANUM_FFMPEG_ROOT}/include")
    list(APPEND ffmpeg_outputs "${library}")
endforeach()
add_custom_command(OUTPUT ${ffmpeg_outputs}
    BYPRODUCTS ${ffmpeg_byproducts}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/../tools/build-ffmpeg.py"
        --work "${CMAKE_BINARY_DIR}/ffmpeg-build" --prefix "${ARCANUM_FFMPEG_ROOT}"
        --cc "${CMAKE_C_COMPILER}" --system "${CMAKE_SYSTEM_NAME}" --arch "${CMAKE_SYSTEM_PROCESSOR}"
        ${ffmpeg_extra_args}
    DEPENDS "${CMAKE_SOURCE_DIR}/../tools/build-ffmpeg.py"
    VERBATIM)
add_custom_target(arcanum-ffmpeg DEPENDS ${ffmpeg_outputs})
foreach(name avformat avcodec avutil)
    add_dependencies(ffmpeg_${name} arcanum-ffmpeg)
endforeach()
