#include "editor_mode.h"
#include <QDebug>
#include <QShortcut>

namespace ncktv {

EditorMode::EditorMode(Project* project, QWidget* parent)
    : QWidget(parent)
    , m_project(project)
{
    m_undoManager = new UndoManager(this);
    m_timingHandler = new TimingOffsetHandler(this);

    setupUi();
    setupToolBar();
    setupConnections();
    applyTheme();
    syncViewState();
}

void EditorMode::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Toolbar
    m_toolBar = new QToolBar(this);
    mainLayout->addWidget(m_toolBar);

    // Main vertically split area
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    mainLayout->addWidget(m_mainSplitter, 1);

    // Top split: Editor (left) | Preview (right)
    m_topSplitter = new QSplitter(Qt::Horizontal, m_mainSplitter);
    m_topSplitter->setOpaqueResize(false);

    m_syllableEditor = new SyllableEditor(this);
    m_previewWidget  = new KaraokePreview(this);
    
    m_topSplitter->addWidget(m_syllableEditor);
    m_topSplitter->addWidget(m_previewWidget);
    m_topSplitter->setStretchFactor(0, 1);
    m_topSplitter->setStretchFactor(1, 1);

    // Bottom split: Timeline and Waveform (stacked/combined)
    QWidget* bottomContainer = new QWidget(m_mainSplitter);
    auto* bottomLayout = new QVBoxLayout(bottomContainer);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);

    m_audioPlayer = new AudioPlayer(this);
    m_waveformWidget = new WaveformWidget(this);
    m_timelineWidget = new TimelineWidget(this);

    bottomLayout->addWidget(m_audioPlayer);
    bottomLayout->addWidget(m_waveformWidget);
    bottomLayout->addWidget(m_timelineWidget);

    // Add to main splitter
    m_mainSplitter->addWidget(m_topSplitter);
    m_mainSplitter->addWidget(bottomContainer);
    
    // 60/40 vertical split
    m_mainSplitter->setSizes({600, 400});
}

void EditorMode::setupToolBar() {
    m_toolBar->addAction("Save", this, [this]() { emit requestSave(); });
    m_toolBar->addSeparator();
    m_toolBar->addAction("Undo", m_undoManager, &UndoManager::undo);
    m_toolBar->addAction("Redo", m_undoManager, &UndoManager::redo);
    m_toolBar->addSeparator();

    // Add play controls
    m_toolBar->addAction("▶ Play", this, [this]() {
        // Toggle play state via m_audioPlayer
    });
}

void EditorMode::setupConnections() {
    // Example: Connecting unified timecode to components
    // connect(m_audioPlayer, &AudioPlayer::timecodeChanged, this, &EditorMode::onTimecodeChanged);
    // connect(m_waveformWidget, &WaveformWidget::seekRequested, m_audioPlayer, &AudioPlayer::seek);
    // connect(m_timelineWidget, &TimelineWidget::seekRequested, m_audioPlayer, &AudioPlayer::seek);
}

void EditorMode::applyTheme() {
    // Styling handled via main_window passing the stylesheet down
    // Component specific styles can be dynamically calculated here
}

void EditorMode::syncViewState() {
    if (!m_project) return;
    // Load project data into components
    // m_syllableEditor->loadLyrics(&m_project->lyrics);
    // m_timelineWidget->loadTimeline(&m_project->timeline);
}

void EditorMode::onPlayPauseToggled(bool isPlaying) {
    qDebug() << "Play state changed:" << isPlaying;
}

void EditorMode::onTimecodeChanged(double timeSeconds) {
    // Dispatch time to preview, waveform, and timeline
    // m_previewWidget->updateTime(timeSeconds);
    // m_waveformWidget->updateCursor(timeSeconds);
    // m_timelineWidget->updateCursor(timeSeconds);
}

void EditorMode::onLineSelected(int lineIndex) {
    // Synchronize selection across components
    // m_waveformWidget->zoomToLine(lineIndex);
}

void EditorMode::onWordSelected(int lineIndex, int wordIndex) {
    // Update inspector or property panel
}

void EditorMode::onLyricsChanged() {
    emit unsavedChangesChanged(true);
    // Refresh preview and waveform markers
}

} // namespace ncktv
