cmake_minimum_required(VERSION 3.25)
project(NC-KTV VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# AUTOMOC/AUTORCC/AUTOUIC are enabled AFTER FetchContent so that
# third-party libraries (whisper.cpp, onnxruntime) never see Qt's MOC.

# ─── Qt6 Installation Path ───────────────────────────────────────────────────
# Qt prefix path is controlled via CMakePresets.json (CMAKE_PREFIX_PATH).
# MinGW preset  → D:/ProgramData/Qt/6.10.2/mingw_64
# Nothing hard-coded here.

# ─── Find Dependencies ───────────────────────────────────────────────────────

# Qt6 (required)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Network)

# OpenSSL (optional - enables encrypted project files)
find_package(OpenSSL QUIET)

# Vulkan (High-Performance Graphics API)
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "Vulkan SDK found: ${Vulkan_LIBRARY}")
    add_compile_definitions(NCKTV_HAS_VULKAN=1)
else()
    message(WARNING "Vulkan SDK not found. Vulkan rendering backend may be unavailable at runtime.")
    add_compile_definitions(NCKTV_HAS_VULKAN=0)
endif()

# ─── Fetch Content (AI & Data Libraries) ─────────────────────────────────────
include(FetchContent)

# 1. nlohmann/json (Header-only JSON parser)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# 2. whisper.cpp (Native Transcription Engine)
# NOTE: whisper.cpp is NOT compiled on MinGW because ggml.c (~80k lines) causes
# gcc to crash (heap exhaustion) on Windows. All transcription in NC-KTV is
# already routed through the Python subprocess bridge (src/gui/workers/transcription_worker.cpp
# launches the Python whisper CLI), so this C++ target is unused at runtime.
# To re-enable for MSVC: uncomment below and set WHISPER_CUBLAS ON.
#
# FetchContent_Declare(whisper
#     GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
#     GIT_TAG        v1.6.2  GIT_SHALLOW 1)
# set(WHISPER_BUILD_TESTS OFF CACHE BOOL "")
# set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "")
# set(WHISPER_CUBLAS OFF CACHE BOOL "")
# set(WHISPER_CUDA   OFF CACHE BOOL "")
# FetchContent_MakeAvailable(whisper)

# 3. ONNX Runtime (prebuilt, GPU-capable)
# Supports BOTH MSVC and MinGW on Windows:
#   MSVC  → links against the shipped onnxruntime.lib import library.
#   MinGW → uses dlltool to auto-generate a MinGW-compatible .a from the DLL.
if(WIN32)
    set(ORT_VERSION "1.18.0")
    FetchContent_Declare(onnxruntime
        URL "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-win-x64-gpu-${ORT_VERSION}.zip"
    )
    FetchContent_MakeAvailable(onnxruntime)

    set(ORT_DLL "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll")
    set(ORT_INC "${onnxruntime_SOURCE_DIR}/include")

    add_library(onnx_shared SHARED IMPORTED)
    target_include_directories(onnx_shared INTERFACE "${ORT_INC}")
    set_property(TARGET onnx_shared PROPERTY IMPORTED_LOCATION "${ORT_DLL}")

    if(MSVC)
        # MSVC: use the shipped .lib import library directly
        set_property(TARGET onnx_shared PROPERTY IMPORTED_IMPLIB "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.lib")
    else()
        # MinGW: generate a .a import library from the DLL using dlltool
        set(ORT_IMPLIB "${CMAKE_BINARY_DIR}/libonnxruntime.a")
        set(ORT_DEF    "${CMAKE_BINARY_DIR}/onnxruntime.def")

        find_program(DLLTOOL_EXE dlltool
            HINTS "${CMAKE_CXX_COMPILER_DIR}"
                  "D:/ProgramData/Qt/Tools/mingw1310_64/bin"
            REQUIRED
        )
        find_program(GENDEF_EXE gendef
            HINTS "${CMAKE_CXX_COMPILER_DIR}"
                  "D:/ProgramData/Qt/Tools/mingw1310_64/bin"
        )

        if(GENDEF_EXE)
            # Best path: gendef extracts the exports, dlltool creates the .a
            add_custom_command(
                OUTPUT  "${ORT_IMPLIB}"
                COMMAND "${GENDEF_EXE}" "${ORT_DLL}"
                COMMAND "${CMAKE_COMMAND}" -E rename
                        "${CMAKE_BINARY_DIR}/onnxruntime.def" "${ORT_DEF}"
                COMMAND "${DLLTOOL_EXE}" -d "${ORT_DEF}" -l "${ORT_IMPLIB}"
                DEPENDS "${ORT_DLL}"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                COMMENT "Generating MinGW .a for ONNX Runtime via gendef+dlltool"
                VERBATIM
            )
        else()
            # Fallback: dlltool alone using --dllname
            add_custom_command(
                OUTPUT  "${ORT_IMPLIB}"
                COMMAND "${DLLTOOL_EXE}"
                        --dllname onnxruntime.dll
                        --output-lib "${ORT_IMPLIB}"
                        "${ORT_DLL}"
                DEPENDS "${ORT_DLL}"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                COMMENT "Generating MinGW .a for ONNX Runtime via dlltool"
                VERBATIM
            )
        endif()

        add_custom_target(onnxruntime_implib DEPENDS "${ORT_IMPLIB}")
        set_property(TARGET onnx_shared PROPERTY IMPORTED_IMPLIB "${ORT_IMPLIB}")
    endif()

    set(NCKTV_HAS_ONNX ON)
else()
    set(NCKTV_HAS_ONNX OFF)
endif()

# 4. zstd (Fast Data Compression)
FetchContent_Declare(zstd
    URL "https://github.com/facebook/zstd/archive/refs/tags/v1.5.5.tar.gz"
    SOURCE_SUBDIR "build/cmake"
)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zstd)

# 5. LibreSSL (OpenSSL-compatible Crypto Engineering)
FetchContent_Declare(libressl
    URL "https://github.com/libressl-portable/portable/releases/download/v3.9.1/libressl-3.9.1.tar.gz"
)
set(LIBRESSL_APPS OFF CACHE BOOL "" FORCE)
set(LIBRESSL_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libressl)

# ─── Enable Qt auto-tools (AFTER third-party FetchContent) ───────────────────
# Now our own add_subdirectory targets see AUTOMOC ON.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─── Targets ─────────────────────────────────────────────────────────────────

add_subdirectory(src/core)
add_subdirectory(src/gui)
add_subdirectory(tests)
