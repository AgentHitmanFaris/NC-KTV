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

    # Configuration
    # config/config_manager.cpp

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

target_include_directories(ncktv_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ncktv_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Network
    PRIVATE
        zstd
        OpenSSL::Crypto
    #     nlohmann_json::nlohmann_json
    #     yaml-cpp
    #     Eigen3::Eigen
    #     $<TARGET_NAME_IF_EXISTS:zstd::libzstd_shared>
    #     $<TARGET_NAME_IF_EXISTS:zstd::libzstd_static>
    #     OpenSSL::SSL
    #     ICU::uc
    #     ICU::i18n
)

# ICU (optional)
if(ICU_FOUND)
    target_link_libraries(ncktv_core PUBLIC ICU::uc ICU::i18n ICU::data)
    target_compile_definitions(ncktv_core PUBLIC NCKTV_HAS_ICU=1)
endif()
