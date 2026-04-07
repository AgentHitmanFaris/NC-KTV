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
#include <QButtonGroup>

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
#include <QPoint>
#include <QDrag>
#include <QMimeData>

#include "../dialogs/export_dialog.h"
#include "../workers/export_worker.h"
#include "../../core/parsers/ass_generator.h"

#include "../core/config/config_manager.h"

namespace ncktv {

class EditorMode : public QWidget {
    Q_OBJECT

public:
    explicit EditorMode(std::shared_ptr<core::Project> project, ConfigManager* config, QWidget* parent = nullptr);

    // Tool mode enum — matches Premiere Pro tools (excluding Hand, Rate Stretch, Ripple Edit)
    enum class ToolMode {
        Selection,      // V — default pointer
        TrackSelect,    // A — select all clips on track forward
        RollingEdit,    // N — adjust edit point between two clips
        Razor,          // C — cut clips
        Slip,           // Y — slip clip content
        Slide,          // U — slide clip position
        Pen,            // P — add keyframes
        Zoom            // Z — zoom in/out
    };

signals:
    void requestSave();
    void requestSaveAs();
    void requestOpen();
    void requestNew();
    void requestPreferences();
    void requestExport();
    void unsavedChangesChanged(bool hasUnsavedChanges);
    void toolModeChanged(ToolMode mode);

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

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

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
    QVBoxLayout* m_rootLayout = nullptr;

    // Top bar & tab bar
    QPushButton* m_tabLyricsBtn = nullptr;
    QPushButton* m_tabTimingBtn = nullptr;
    QPushButton* m_tabRenderBtn = nullptr;
    QPushButton* m_saveProjectBtn = nullptr;
    QPushButton* m_consoleBtn = nullptr;

    // Playback time display in transport bar
    QLabel* m_playbackTimeLabel = nullptr;

    // Main content stack
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

    // Tools panel (Task 3.2)
    QButtonGroup* m_toolBtnGroup = nullptr;
    ToolMode m_activeTool = ToolMode::Selection;

    // Workspace splitters (Task 3.1)
    QSplitter* m_workspaceSplitter = nullptr;
    QSplitter* m_contentSplitter = nullptr;

    // Source panel timecode overlay
    QLabel* m_sourceTimecodeLabel = nullptr;

    // Display mode toggle button in source panel header
    QPushButton* m_displayModeBtn = nullptr;

    // Lyrics blocks panel (below timeline, for drag-to-timeline)
    QWidget* m_lyricsBlocksPanel = nullptr;

    // Drag tracking for lyric blocks
    QPoint m_dragStartPos;
};

} // namespace ncktv

