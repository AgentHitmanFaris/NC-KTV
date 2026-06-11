# ─── NC-KTV Core Library ─────────────────────────────────────────────────────
# Static library containing core data structures, track management,
# project state, and AV/AI processing pipelines.

set(CORE_SOURCES
    timeline/clip.cpp
    timeline/track.cpp
    timeline/timeline_manager.cpp
    timeline/lyric_engine.cpp
    timeline/romanizer.cpp
    timeline/models/track_list_model.cpp
    timeline/models/clip_list_model.cpp
    audio/audio_reader.cpp
    audio/audio_engine.cpp
    audio/stem_resampler.cpp
    audio/stem_separator.cpp
    audio/stem_separation_worker.cpp
    audio/render_engine.cpp
    audio/render_worker.cpp
    youtube/youtube_manager.cpp
)

set(CORE_HEADERS
    timeline/clip.h
    timeline/track.h
    timeline/timeline_manager.h
    timeline/lyric_engine.h
    timeline/romanizer.h
    timeline/models/track_list_model.h
    timeline/models/clip_list_model.h
    audio/audio_reader.h
    audio/audio_engine.h
    audio/stem_resampler.h
    audio/stem_separator.h
    audio/stem_separation_worker.h
    audio/render_engine.h
    audio/render_worker.h
    youtube/youtube_manager.h
)

add_library(ncktv_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

# ── Include Paths ────────────────────────────────────────────────────────────
target_include_directories(ncktv_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/thirdparty
)

# ── Qt & External dependencies ────────────────────────────────────────────────
target_link_libraries(ncktv_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Multimedia
        nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(ncktv_core PUBLIC dxgi)
endif()

if(TARGET onnx_shared)
    target_link_libraries(ncktv_core PUBLIC onnx_shared)
    target_compile_definitions(ncktv_core PUBLIC NCKTV_HAS_ONNX=1)
else()
    target_compile_definitions(ncktv_core PUBLIC NCKTV_HAS_ONNX=0)
endif()
