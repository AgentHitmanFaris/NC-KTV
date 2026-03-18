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
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QPlainTextEdit>

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
    void requestSaveAs();
    void requestOpen();
    void requestNew();
    void requestPreferences();
    void requestExport();
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
    void onWaveformReady(const QVector<float>& minData, const QVector<float>& maxData, double sampleRate, int samplesPerPixel);
    
    // Auto/Import slots
    void onAutoWhisperClicked();
    void onImportSubtitleClicked();

public slots:
    void onExportClicked();
    void onExportSettingsClicked();

private:
    void setupUi();
    void setupToolBar();
    void setupConnections();
    void applyTheme();
    void requestWaveform(const QString& path);

    std::shared_ptr<core::Project> m_project;
    double m_currentTime = 0.0;
    
    // Components
    KaraokePreview*      m_previewWidget = nullptr;
    QTableWidget*        m_syncTable = nullptr;   // replaces m_subtitleList
    TimelineWidget*      m_timelineWidget = nullptr;
    WaveformWidget*      m_waveformWidget = nullptr;
    AudioPlayer*         m_audioPlayer = nullptr;

    // Controllers
    UndoManager*         m_undoManager = nullptr;
    TimingOffsetHandler* m_timingHandler = nullptr;

    // Layout — root layout
    QHBoxLayout* m_mainHLayout = nullptr;
    
    // Left Sidebar
    QWidget* m_sidebar = nullptr;
    QPushButton* m_modeLyricsBtn = nullptr;
    QPushButton* m_modeTimingBtn = nullptr;
    QPushButton* m_modeRenderBtn = nullptr;
    QPushButton* m_saveProjectBtn = nullptr;
    QPushButton* m_consoleBtn = nullptr;

    // Right Workspace
    QVBoxLayout* m_workspaceLayout = nullptr;
    QStackedWidget* m_viewStack = nullptr; // 0: Lyrics, 1: Timing, 2: Render
    
    // Lyrics Editor View (Page 0)
    QWidget* m_lyricsView = nullptr;
    
    // Timing Sync View (Page 1)
    QWidget* m_timingView = nullptr;

    // Video Render View (Page 2)
    QWidget* m_renderView = nullptr;
    
    // Lyrics Sub-Views
    QStackedWidget* m_lyricsSubStack = nullptr;
    QPlainTextEdit* m_sourceLyricsEdit = nullptr;
    QPushButton* m_lyrSourceBtn = nullptr;
    QPushButton* m_lyrHistoryBtn = nullptr;
    bool m_syncingFromEdit = false; // Guard against re-entrant edit<->model sync
    
    // Transport Bar (Global across views)
    QWidget* m_transportBar = nullptr;

    QToolBar*  m_toolBar = nullptr; // Leftover if needed, but we'll build custom

    QComboBox*   m_trackSelector = nullptr;
    QLineEdit*   m_subtitleInput = nullptr;
    QPushButton* m_addSubtitleBtn = nullptr;
    QPushButton* m_setStartBtn = nullptr;
    QPushButton* m_setEndBtn = nullptr;
    QPushButton* m_whisperBtn = nullptr;
    QPushButton* m_precisionBtn = nullptr;
    QPushButton* m_splitTokensBtn = nullptr;
    QComboBox*   m_langCombo = nullptr;
    QPushButton* m_importBtn = nullptr;
    QLabel*      m_statusLabel = nullptr;

    // Dedicated video-only player (always plays original file, muted)
    QMediaPlayer* m_videoPlayer = nullptr;
    QAudioOutput* m_videoAudioOutput = nullptr;
    ConfigManager* m_config = nullptr;
};

} // namespace ncktv

