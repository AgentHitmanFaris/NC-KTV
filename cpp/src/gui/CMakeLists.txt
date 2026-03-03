# ─── NC-KTV GUI Application ──────────────────────────────────────────────────

set(GUI_SOURCES
    main.cpp
    main_window.cpp

    # Wizard
    wizard/wizard_mode.cpp

    # Editor
    editor/editor_mode.cpp
    editor/timing_offset_handler.cpp
    editor/undo_manager.cpp

    # Components
    components/syllable_editor.cpp
    components/karaoke_preview.cpp
    components/timeline_widget.cpp
    components/waveform_widget.cpp
    components/audio_player.cpp
    components/curve_editor.cpp
    components/effect_panel.cpp
    components/timing_calibration.cpp

    # Dialogs
    dialogs/export_dialog.cpp
    dialogs/import_dialog.cpp
    dialogs/new_project_dialog.cpp
    dialogs/model_manager_dialog.cpp
    dialogs/model_manager_widget.cpp
    dialogs/plugin_manager_dialog.cpp
    dialogs/theme_manager_dialog.cpp
    dialogs/preferences_dialog.cpp
    dialogs/shortcuts_dialog.cpp
    dialogs/video_options_dialog.cpp
    dialogs/export_credits_dialog.cpp
    dialogs/word_editor.cpp

    # Workers
    workers/transcription_worker.cpp
    workers/processing_worker.cpp
    workers/export_worker.cpp
    workers/online_search_worker.cpp
    workers/progress_reporter.cpp

    # Resources
    resources/resources.qrc
)

set(GUI_HEADERS
    main_window.h
    wizard/wizard_mode.h
    editor/editor_mode.h
    editor/timing_offset_handler.h
    editor/undo_manager.h
    components/syllable_editor.h
    components/karaoke_preview.h
    components/timeline_widget.h
    components/waveform_widget.h
    components/audio_player.h
    components/curve_editor.h
    components/effect_panel.h
    components/timing_calibration.h
    dialogs/export_dialog.h
    dialogs/import_dialog.h
    dialogs/new_project_dialog.h
    dialogs/model_manager_dialog.h
    dialogs/model_manager_widget.h
    dialogs/plugin_manager_dialog.h
    dialogs/theme_manager_dialog.h
    dialogs/preferences_dialog.h
    dialogs/shortcuts_dialog.h
    dialogs/video_options_dialog.h
    dialogs/export_credits_dialog.h
    dialogs/word_editor.h
    workers/transcription_worker.h
    workers/processing_worker.h
    workers/export_worker.h
    workers/online_search_worker.h
    workers/progress_reporter.h
)

# Win32 app (no console window in Release)
add_executable(ncktv WIN32 ${GUI_SOURCES} ${GUI_HEADERS})

target_include_directories(ncktv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ncktv PRIVATE
    ncktv_core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::Core5Compat
)
