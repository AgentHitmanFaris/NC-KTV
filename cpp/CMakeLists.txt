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
# The prebuilt zip ships .lib import libs which only work with MSVC.
# MinGW builds skip ONNX; vocal separation uses the Python subprocess bridge.
if(MSVC)
    set(ORT_VERSION "1.18.0")
    FetchContent_Declare(onnxruntime
        URL "https://github.com/microsoft/onnxruntime/releases/download/v1.18.0/onnxruntime-win-x64-gpu-1.18.0.zip"
    )
    FetchContent_MakeAvailable(onnxruntime)

    add_library(onnx_shared SHARED IMPORTED)
    set_property(TARGET onnx_shared PROPERTY IMPORTED_IMPLIB   "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.lib")
    set_property(TARGET onnx_shared PROPERTY IMPORTED_LOCATION "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll")
    target_include_directories(onnx_shared INTERFACE "${onnxruntime_SOURCE_DIR}/include")
    set(NCKTV_HAS_ONNX ON)
else()
    set(NCKTV_HAS_ONNX OFF)
endif()

# ─── Enable Qt auto-tools (AFTER third-party FetchContent) ───────────────────
# Now our own add_subdirectory targets see AUTOMOC ON.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─── Targets ─────────────────────────────────────────────────────────────────

add_subdirectory(src/core)
add_subdirectory(src/gui)
add_subdirectory(tests)
