#pragma once
/*
 * NC-KTV GUI — Precision Mode
 * Dedicated layout for Karaoke Builder Studio style word-level authoring.
 */

#include <QWidget>
#include <QSplitter>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QScrollArea>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QToolBar>
#include "../core/project/ncktv_project.hpp"
#include "../editor/undo_manager.h"
#include "../components/audio_player.h"
#include "../components/karaoke_preview.h"
#include "../dialogs/word_editor.h" // We reuse WordCanvas from here

namespace ncktv {

class PrecisionMode : public QWidget {
    Q_OBJECT

public:
    explicit PrecisionMode(std::shared_ptr<core::Project> project, QWidget* parent = nullptr);

signals:
    void requestSave();
    void unsavedChangesChanged(bool hasUnsavedChanges);

private slots:
    void syncViewState();
    void onSubtitleSelected(QListWidgetItem* item);
    void onWordSelectionChanged(int index);
    void onWordModified();
    void onTimecodeChanged(double timeSeconds);
    void onZoomChanged(int value);
    void onTableCellChanged(int row, int col);
    
    // Tools
    void onAddWord();
    void onSplitWord();
    void onJoinWord();
    void onDeleteWord();
    void onPlayWord();
    
    // Tracks
    void onTrackSelectionChanged(int index);

private:
    void setupUi();
    void setupToolBar();
    void setupConnections();
    void applyTheme();
    void updateSubtitleList();
    void updateButtons();

    std::shared_ptr<core::Project> m_project;
    int m_currentLineIdx = -1;
    UndoManager* m_undoManager = nullptr;
    
    QToolBar* m_toolBar = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    QListWidget* m_subtitleList = nullptr;
    
    // Editor components
    WordCanvas* m_canvas = nullptr;
    AudioPlayer* m_audioPlayer = nullptr;
    
    // Edit toolbar
    QCheckBox* m_stickyCheck = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_splitBtn = nullptr;
    QPushButton* m_joinBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_playWordBtn = nullptr;
    QSlider* m_zoomSlider = nullptr;
    
    QTableWidget* m_wordTable = nullptr;
    QLabel* m_timeLabel = nullptr;
    QComboBox* m_trackSelector = nullptr;
    QScrollArea* m_scrollArea = nullptr;

    double m_autoStopAt = -1.0;
    double m_segmentStartTime = -1.0;
    
    KaraokePreview* m_previewWidget = nullptr;
    QMediaPlayer* m_videoPlayer = nullptr;
    QAudioOutput* m_videoAudioOutput = nullptr;
};

} // namespace ncktv
