# ─── NC-KTV Core Library ─────────────────────────────────────────────────────
# Static library containing all non-GUI logic: data structures, parsers,
# audio processing, project I/O, plugin system, etc.

set(CORE_SOURCES
    # Lyrics data structures
    lyrics/lyrics_data.cpp

    # Timeline data structures
    timeline/timeline_data.cpp

    # Audio engine
    audio/audio_clock.cpp
    audio/audio_processor.cpp
    audio/vocal_remover.cpp
    audio/ffmpeg_utils.cpp

    # Effects
    effects/effect_compositor.cpp

    # Subtitle parsers
    parsers/subtitle_parser.cpp
    parsers/lrc_parser.cpp
    parsers/srt_parser.cpp
    parsers/ass_generator.cpp

    # Text processing
    text/romanizer.cpp

    # Project I/O
    project/project.cpp
    project/nctv_format.cpp

    # Plugin system
    plugins/plugin_manager.cpp

    # Theme system
    themes/theme_manager.cpp

    # Networking
    net/lrclib_client.cpp

    # System utilities
    system/gpu_detector.cpp
)

set(CORE_HEADERS
    lyrics/lyrics_data.h
    timeline/timeline_data.h
    audio/audio_clock.h
    audio/audio_processor.h
    audio/vocal_remover.h
    audio/ffmpeg_utils.h
    effects/effect_compositor.h
    parsers/subtitle_parser.h
    parsers/lrc_parser.h
    parsers/srt_parser.h
    parsers/ass_generator.h
    text/romanizer.h
    project/project.h
    project/nctv_format.h
    config/config_manager.h
    plugins/plugin_base.h
    plugins/plugin_manager.h
    themes/theme_manager.h
    net/lrclib_client.h
    system/gpu_detector.h
)

add_library(ncktv_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

# ── Include paths ─────────────────────────────────────────────────────────────
# Expose src/core itself so #include "audio/audio_clock.h" and
# #include "nlohmann/json.hpp" both resolve correctly.
target_include_directories(ncktv_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ── Qt dependencies ───────────────────────────────────────────────────────────
target_link_libraries(ncktv_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Network
)

# ── Optional: OpenSSL (for AES-256-GCM in NCTV file format) ──────────────────
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND OR OPENSSL_FOUND)
    message(STATUS "NC-KTV: OpenSSL found — encrypted project files enabled")
    target_link_libraries(ncktv_core PRIVATE OpenSSL::Crypto)
    target_compile_definitions(ncktv_core PUBLIC NCKTV_HAS_OPENSSL=1)
else()
    message(STATUS "NC-KTV: OpenSSL NOT found — project files will use plain JSON")
    target_compile_definitions(ncktv_core PUBLIC NCKTV_HAS_OPENSSL=0)
endif()

# ── Optional: zstd (compression in NCTV file format) ─────────────────────────
find_package(zstd QUIET)
if(TARGET zstd::libzstd_shared OR TARGET zstd::libzstd_static OR TARGET PkgConfig::zstd)
    message(STATUS "NC-KTV: zstd found — project file compression enabled")
    if(TARGET zstd::libzstd_static)
        target_link_libraries(ncktv_core PRIVATE zstd::libzstd_static)
    else()
        target_link_libraries(ncktv_core PRIVATE zstd::libzstd_shared)
    endif()
    target_compile_definitions(ncktv_core PUBLIC NCKTV_HAS_ZSTD=1)
else()
    # Try plain package name (some distros)
    find_library(ZSTD_LIB NAMES zstd libzstd QUIET)
    if(ZSTD_LIB)
        message(STATUS "NC-KTV: zstd found (plain lib) — compression enabled")
        target_link_libraries(ncktv_core PRIVATE ${ZSTD_LIB})
        target_compile_definitions(ncktv_core PUBLIC NCKTV_HAS_ZSTD=1)
    else()
        message(STATUS "NC-KTV: zstd NOT found — project files will skip compression")
        target_compile_definitions(ncktv_core PUBLIC NCKTV_HAS_ZSTD=0)
    endif()
endif()

# ── Optional: ICU (for romanization) ─────────────────────────────────────────
find_package(ICU COMPONENTS uc i18n QUIET)
if(ICU_FOUND)
    target_link_libraries(ncktv_core PUBLIC ICU::uc ICU::i18n ICU::data)
    target_compile_definitions(ncktv_core PUBLIC NCKTV_HAS_ICU=1)
endif()
