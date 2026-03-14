#pragma once

#include <QWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QProgressBar>

#include "../core/project/ncktv_project.hpp"
#include "undo_manager.h"
#include "timing_offset_handler.h"
#include "../components/timeline_widget.h"
#include "../components/karaoke_preview.h"
#include "../components/waveform_widget.h"
#include "../components/audio_player.h"
#include "../components/lyrical_pro_widget.h"
#include <QListWidget>
#include <memory>

#include "../dialogs/export_dialog.h"
#include "../workers/export_worker.h"
#include "../../core/parsers/ass_generator.h"

#include "../core/config/config_manager.h"

namespace ncktv {

class EditorMode : public QWidget {
    Q_OBJECT

public:
    explicit EditorMode(std::shared_ptr<core::Project> project, ConfigManager* config, QWidget* parent = nullptr);

signals:
    void requestSave();
    void unsavedChangesChanged(bool hasUnsavedChanges);

private slots:
    void onPlayPauseToggled(bool isPlaying);
    void onTimecodeChanged(double timeSeconds);
    void onLineSelected(int lineIndex);
    void onWordSelected(int lineIndex, int wordIndex);
    void onWordsChanged();
    void syncViewState();
    
    void onTrackSelectionChanged(int index);
    void onAddSubtitleClicked();
    void updateSubtitleList();
    
    // Auto/Import slots
    void onAutoWhisperClicked();
    void onImportSubtitleClicked();

    // Lyrical Pro Mode
    void toggleLyricalPro();
    void exitLyricalPro();

    void onExportClicked();
    void onExportSettingsClicked();

private:
    void setupUi();
    void setupToolBar();
    void setupConnections();
    void applyTheme();
    void syncLyricalProState();

    std::shared_ptr<core::Project> m_project;
    double m_currentTime = 0.0;
    
    // Components
    KaraokePreview*      m_previewWidget = nullptr;
    QListWidget*         m_subtitleList = nullptr;   // replaces SyllableEditor
    TimelineWidget*      m_timelineWidget = nullptr;
    WaveformWidget*      m_waveformWidget = nullptr;
    AudioPlayer*         m_audioPlayer = nullptr;

    // Controllers
    UndoManager*         m_undoManager = nullptr;
    TimingOffsetHandler* m_timingHandler = nullptr;

    // Layout — root stack (page 0 = normal editor, page 1 = Lyrical Pro)
    QStackedWidget* m_rootStack   = nullptr;
    QWidget*        m_editorPage  = nullptr;   // page 0
    LyricalProWidget* m_lyricalPro = nullptr;  // page 1

    QToolBar*  m_toolBar = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    QSplitter* m_lowerSplitter = nullptr;
    QSplitter* m_topSplitter = nullptr;
    
    QComboBox*   m_trackSelector = nullptr;
    QLineEdit*   m_subtitleInput = nullptr;
    QPushButton* m_addSubtitleBtn = nullptr;
    QPushButton* m_whisperBtn = nullptr;
    QPushButton* m_precisionBtn = nullptr;
    QPushButton* m_lyricalProBtn = nullptr;   // toolbar button
    QComboBox*   m_langCombo = nullptr;
    QPushButton* m_importBtn = nullptr;
    QLabel*      m_statusLabel = nullptr;

    // Dedicated video-only player (always plays original file, muted)
    QMediaPlayer* m_videoPlayer = nullptr;
    QAudioOutput* m_videoAudioOutput = nullptr;
    ConfigManager* m_config = nullptr;
};

} // namespace ncktv

