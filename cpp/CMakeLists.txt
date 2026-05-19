cmake_minimum_required(VERSION 3.25)
project(NC-KTV VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── VCPKG Integration (Manifest Mode) ───────────────────────────────────────
# Detect and apply vcpkg toolchain automatically if present in the environment
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    message(STATUS "Vcpkg toolchain injected: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# ─── Find Core Dependencies ──────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Network)
include(FetchContent)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG v1.5.5
    SOURCE_SUBDIR build/cmake
)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zstd)
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

# ─── External Content (ONNX Runtime) ─────────────────────────────────────────
# ONNX GPU builds are retained in FetchContent to preserve the MinGW dlltool workflow.
include(FetchContent)

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
            HINTS "${CMAKE_CXX_COMPILER_DIR}" "D:/ProgramData/Qt/Tools/mingw1310_64/bin"
            REQUIRED
        )
        
        find_program(GENDEF_EXE gendef
            HINTS "${CMAKE_CXX_COMPILER_DIR}" "D:/ProgramData/Qt/Tools/mingw1310_64/bin"
        )

        if(GENDEF_EXE)
            add_custom_command(
                OUTPUT  "${ORT_IMPLIB}"
                COMMAND "${GENDEF_EXE}" "${ORT_DLL}"
                COMMAND "${CMAKE_COMMAND}" -E rename "${CMAKE_BINARY_DIR}/onnxruntime.def" "${ORT_DEF}"
                COMMAND "${DLLTOOL_EXE}" -d "${ORT_DEF}" -l "${ORT_IMPLIB}"
                DEPENDS "${ORT_DLL}"
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                COMMENT "Generating MinGW .a for ONNX Runtime via gendef+dlltool"
                VERBATIM
            )
        else()
            add_custom_command(
                OUTPUT  "${ORT_IMPLIB}"
                COMMAND "${DLLTOOL_EXE}" --dllname onnxruntime.dll --output-lib "${ORT_IMPLIB}" "${ORT_DLL}"
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

# ─── Enable Qt auto-tools ────────────────────────────────────────────────────
# Ensure tools are run after dependencies to avoid MOC scanning third-party headers
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─── Project Targets ─────────────────────────────────────────────────────────
add_subdirectory(src/core)
add_subdirectory(src/gui)
add_subdirectory(tests)