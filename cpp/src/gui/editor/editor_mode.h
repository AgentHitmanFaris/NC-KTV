#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>

#include "project/project.h"
#include "undo_manager.h"
#include "timing_offset_handler.h"
#include "../components/timeline_widget.h"
#include "../components/karaoke_preview.h"
#include "../components/syllable_editor.h"
#include "../components/waveform_widget.h"
#include "../components/audio_player.h"

namespace ncktv {

class EditorMode : public QWidget {
    Q_OBJECT
public:
    explicit EditorMode(Project* project, QWidget* parent = nullptr);

signals:
    void requestSave();
    void unsavedChangesChanged(bool hasUnsavedChanges);

private slots:
    void onPlayPauseToggled(bool isPlaying);
    void onTimecodeChanged(double timeSeconds);
    void onLineSelected(int lineIndex);
    void onWordSelected(int lineIndex, int wordIndex);
    void onLyricsChanged();
    void syncViewState();

private:
    void setupUi();
    void setupToolBar();
    void setupConnections();
    void applyTheme();

    Project* m_project = nullptr;

    // Components
    KaraokePreview*      m_previewWidget = nullptr;
    SyllableEditor*      m_syllableEditor = nullptr;
    TimelineWidget*      m_timelineWidget = nullptr;
    WaveformWidget*      m_waveformWidget = nullptr;
    AudioPlayer*         m_audioPlayer = nullptr;

    // Controllers
    UndoManager*         m_undoManager = nullptr;
    TimingOffsetHandler* m_timingHandler = nullptr;

    // Layout
    QToolBar*  m_toolBar = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    QSplitter* m_lowerSplitter = nullptr;
    QSplitter* m_topSplitter = nullptr;
};

} // namespace ncktv

