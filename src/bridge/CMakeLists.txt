# ─── NC-KTV C-Style Bridge Target ─────────────────────────────────────────────
# Builds the shared library DLL for consumption by WPF.

set(BRIDGE_SOURCES
    ncktv_bridge.cpp
    ncktv_bridge.h
)

add_library(ncktv_bridge SHARED ${BRIDGE_SOURCES})

# Set binary output directory to the root build folder to match tests and ensure correct windeployqt behavior
set_target_properties(ncktv_bridge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Include paths
target_include_directories(ncktv_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
)

# Compile definitions
target_compile_definitions(ncktv_bridge PRIVATE NCKTV_BRIDGE_EXPORTS)

# Link dependencies
target_link_libraries(ncktv_bridge PRIVATE
    ncktv_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

if(TARGET onnx_shared)
    target_link_libraries(ncktv_bridge PRIVATE onnx_shared)
endif()

# ─── Post-Build DLL Deployments ───────────────────────────────────────────────
if(WIN32)
    # 1. Copy ONNX Runtime DLLs
    if(TARGET onnx_shared)
        file(GLOB ORT_DLLS "${onnxruntime_SOURCE_DIR}/runtimes/win-x64/native/*.dll")
        foreach(DLL_PATH ${ORT_DLLS})
            add_custom_command(TARGET ncktv_bridge POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL_PATH}"
                    $<TARGET_FILE_DIR:ncktv_bridge>
                COMMENT "Deploying ONNX Runtime DLL ${DLL_PATH} to build bin..."
            )
        endforeach()
    endif()

    # 2. Copy FFmpeg and FFprobe from backed up folder
    add_custom_command(TARGET ncktv_bridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/ffmpeg/bin/ffmpeg.exe"
            "${CMAKE_SOURCE_DIR}/ffmpeg/bin/ffprobe.exe"
            $<TARGET_FILE_DIR:ncktv_bridge>
        COMMENT "Deploying FFmpeg executables to build bin..."
    )

    # 3. Deploy Qt runtime dependencies (DLLs, plugins)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt6
        HINTS "${CMAKE_PREFIX_PATH}/bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ncktv_bridge POST_BUILD
            COMMAND cmd.exe /C "set \"PATH=${CMAKE_PREFIX_PATH}/bin;%PATH%\" && ${WINDEPLOYQT_EXECUTABLE} --no-translations --no-compiler-runtime --debug $<TARGET_FILE:ncktv_bridge> || exit 0"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_PREFIX_PATH}/plugins/platforms/qoffscreen.dll" "${CMAKE_BINARY_DIR}/platforms/qoffscreen.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_PREFIX_PATH}/plugins/platforms/qoffscreend.dll" "${CMAKE_BINARY_DIR}/platforms/qoffscreend.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_PREFIX_PATH}/plugins/platforms/qminimal.dll" "${CMAKE_BINARY_DIR}/platforms/qminimal.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_PREFIX_PATH}/plugins/platforms/qminimald.dll" "${CMAKE_BINARY_DIR}/platforms/qminimald.dll"
            COMMENT "Running windeployqt6 on ncktv_bridge and deploying offscreen/minimal platforms..."
        )
    else()
        message(WARNING "windeployqt6 not found. Qt runtime DLLs will not be auto-deployed.")
    endif()
endif()
