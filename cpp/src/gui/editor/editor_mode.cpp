#include "editor_mode.h"
#include <QDebug>
#include <QShortcut>
#include <QLabel>
#include <cmath>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QEvent>
#include <QProgressBar>
#include "../workers/transcription_worker.h"
#include "../dialogs/word_editor.h"

namespace ncktv {

class ProcessingOverlay : public QWidget {
public:
    QProgressBar* bar;
    QLabel* msg;
    ProcessingOverlay(QWidget* parent) : QWidget(parent) {
        setGeometry(parent->rect());
        setStyleSheet("QWidget { background: rgba(0, 0, 0, 180); }");

        auto* layout = new QVBoxLayout(this);
        layout->setAlignment(Qt::AlignCenter);
        
        auto* popup = new QWidget(this);
        popup->setFixedSize(400, 150);
        popup->setStyleSheet("QWidget { background: #1e1e1e; border: 1px solid #444; border-radius: 10px; }");
        
        auto* popupLayout = new QVBoxLayout(popup);
        popupLayout->setAlignment(Qt::AlignCenter);
        popupLayout->setSpacing(20);

        msg = new QLabel("Processing AI Transcription...", popup);
        msg->setStyleSheet("color: white; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        msg->setAlignment(Qt::AlignCenter);
        popupLayout->addWidget(msg);
        
        bar = new QProgressBar(popup);
        bar->setRange(0, 100);
        bar->setFixedHeight(16);
        bar->setTextVisible(false);
        bar->setStyleSheet("QProgressBar { border: 1px solid #555; border-radius: 4px; background: #222; } QProgressBar::chunk { background-color: #4f46e5; border-radius: 2px; }");
        popupLayout->addWidget(bar);
        
        layout->addWidget(popup);
        
        // Prevent all underlying clicks
        setCursor(Qt::BusyCursor);
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
    }
    protected:
    void mousePressEvent(QMouseEvent*) override {} // consume clicks
};

EditorMode::EditorMode(std::shared_ptr<core::Project> project, QWidget* parent)
    : QWidget(parent)
    , m_project(std::move(project))
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
    // Root layout hosts a QStackedWidget:
    //   Page 0 — normal editor (toolbar + splitters)
    //   Page 1 — Lyrical Pro full-screen teleprompter
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_rootStack = new QStackedWidget(this);
    rootLayout->addWidget(m_rootStack);

    // ── Page 0 : Normal Editor ─────────────────────────────────────────────
    m_editorPage = new QWidget(m_rootStack);
    auto* mainLayout = new QVBoxLayout(m_editorPage);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Toolbar
    m_toolBar = new QToolBar(m_editorPage);
    mainLayout->addWidget(m_toolBar);

    // Main horizontal split area (Left vs Right)
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(m_mainSplitter, 1);

    // --- LEFT PANE : Subtitle List Panel ---
    QWidget* leftWidget = new QWidget(m_mainSplitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    // Header
    QWidget* headerWidget = new QWidget(leftWidget);
    headerWidget->setStyleSheet("background:#252526; border-bottom:1px solid #111;");
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(10, 6, 10, 6);
    
    auto* listHeader = new QLabel("✦  Subtitles", headerWidget);
    listHeader->setStyleSheet("color:#ccc; font-weight:bold; border:none;");
    headerLayout->addWidget(listHeader);
    
    m_statusLabel = new QLabel("", headerWidget);
    m_statusLabel->setStyleSheet("color:#64748b; font-size:11px; border:none;");
    headerLayout->addWidget(m_statusLabel, 1, Qt::AlignRight);
    
    m_langCombo = new QComboBox(headerWidget);
    m_langCombo->addItems({"Auto", "en", "ms", "id", "ja", "ko", "zh"});
    m_langCombo->setStyleSheet("padding:2px 4px; font-size:11px; background:#333; color:#ccc; border:1px solid #555; border-radius:3px;");
    m_langCombo->setToolTip("Language for AI Transcription");
    headerLayout->addWidget(m_langCombo);
    
    m_whisperBtn = new QPushButton("🎙 AI", headerWidget);
    m_whisperBtn->setToolTip("Auto-transcribe vocals with Whisper");
    m_whisperBtn->setStyleSheet("padding:2px 6px; background:#4f46e5; color:white; border-radius:3px; border:none; font-size:11px;");
    headerLayout->addWidget(m_whisperBtn);

    m_precisionBtn = new QPushButton("⏱ Precision", headerWidget);
    m_precisionBtn->setToolTip("Open Precision Word-Timing Editor");
    m_precisionBtn->setStyleSheet("padding:2px 6px; background:#059669; color:white; border-radius:3px; border:none; font-size:11px;");
    headerLayout->addWidget(m_precisionBtn);
    
    m_importBtn = new QPushButton("📁 Import", headerWidget);
    m_importBtn->setToolTip("Import LRC, SRT, TXT, or JSON");
    m_importBtn->setStyleSheet("padding:2px 6px; background:#333; color:#ccc; border-radius:3px; border:1px solid #555; font-size:11px;");
    headerLayout->addWidget(m_importBtn);
    
    leftLayout->addWidget(headerWidget);

    // Subtitle list
    m_subtitleList = new QListWidget(leftWidget);
    m_subtitleList->setStyleSheet(
        "QListWidget { background:#1e1e1e; border:none; color:#ddd; }"
        "QListWidget::item { padding:5px 8px; border-bottom:1px solid #2d2d30; }"
        "QListWidget::item:selected { background:#007acc; color:#fff; }"
        "QListWidget::item:hover { background:#2d2d30; }"
    );
    leftLayout->addWidget(m_subtitleList, 1);

    // Subtitle input bar at bottom
    auto* subtitleLayout = new QHBoxLayout();
    subtitleLayout->setContentsMargins(6, 4, 6, 4);
    subtitleLayout->setSpacing(4);
    m_subtitleInput = new QLineEdit(leftWidget);
    m_subtitleInput->setPlaceholderText("Add subtitle at current time...");
    m_addSubtitleBtn = new QPushButton("+", leftWidget);
    m_addSubtitleBtn->setFixedWidth(32);
    subtitleLayout->addWidget(m_subtitleInput, 1);
    subtitleLayout->addWidget(m_addSubtitleBtn, 0);

    leftLayout->addLayout(subtitleLayout);

    // --- RIGHT PANE : Vertical Split (Preview top, Timeline bottom) ---
    QSplitter* rightSplitter = new QSplitter(Qt::Vertical, m_mainSplitter);
    
    // Top right: Preview
    m_previewWidget = new KaraokePreview(rightSplitter);
    rightSplitter->addWidget(m_previewWidget);

    // Bottom right: Timeline / Audio Control
    QWidget* bottomContainer = new QWidget(rightSplitter);
    auto* bottomLayout = new QVBoxLayout(bottomContainer);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);

    m_audioPlayer = new AudioPlayer(this);
    m_waveformWidget = new WaveformWidget(this);
    m_timelineWidget = new TimelineWidget(this);

    bottomLayout->addWidget(m_audioPlayer);
    bottomLayout->addWidget(m_waveformWidget);
    bottomLayout->addWidget(m_timelineWidget);

    rightSplitter->addWidget(bottomContainer);

    // Add to main splitter
    m_mainSplitter->addWidget(leftWidget);
    m_mainSplitter->addWidget(rightSplitter);
    
    // Proportions
    m_mainSplitter->setSizes({400, 1200});
    rightSplitter->setSizes({600, 400});

    m_rootStack->addWidget(m_editorPage);  // index 0

    // ── Page 1 : Lyrical Pro ───────────────────────────────────────────────
    m_lyricalPro = new LyricalProWidget(m_rootStack);
    m_rootStack->addWidget(m_lyricalPro);  // index 1

    m_rootStack->setCurrentIndex(0);
}

void EditorMode::setupToolBar() {
    m_toolBar->addAction("Save", this, [this]() { emit requestSave(); });
    m_toolBar->addSeparator();
    m_toolBar->addAction("Undo", m_undoManager, &UndoManager::undo);
    m_toolBar->addAction("Redo", m_undoManager, &UndoManager::redo);
    m_toolBar->addSeparator();

    // Track Selection
    m_trackSelector = new QComboBox(this);
    m_trackSelector->addItems({"Original", "Instrumental", "Vocals Only"});
    m_toolBar->addWidget(m_trackSelector);
    connect(m_trackSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EditorMode::onTrackSelectionChanged);
    m_toolBar->addSeparator();

    // Add play controls
    m_toolBar->addAction("▶ Play", this, [this]() {
        // Toggle play state via m_audioPlayer
    });
    m_toolBar->addSeparator();

    // ── Lyrical Pro Mode button (rightmost, styled) ────────────────────────
    m_lyricalProBtn = new QPushButton("🎤 Lyrical Pro Mode", this);
    m_lyricalProBtn->setStyleSheet(
        "QPushButton { background: #8b4513; color: #ffeedd; border-radius: 5px;"
        "              border: none; font-size: 12px; font-weight: bold; padding: 4px 12px; }"
        "QPushButton:hover   { background: #a0522d; }"
        "QPushButton:pressed { background: #6b3010; }"
    );
    m_toolBar->addSeparator();
    m_toolBar->addWidget(m_lyricalProBtn);
    connect(m_lyricalProBtn, &QPushButton::clicked, this, &EditorMode::toggleLyricalPro);
}

void EditorMode::setupConnections() {
    // Sync playback position to waveform and timeline
    connect(m_audioPlayer, &AudioPlayer::positionChanged, this, &EditorMode::onTimecodeChanged);
    
    // Sync duration
    connect(m_audioPlayer, &AudioPlayer::durationChanged, this, [this](double duration) {
        m_waveformWidget->setDuration(duration);
        // m_timelineWidget->setDuration(duration);
    });

    // Handle seeking from UI components
    connect(m_waveformWidget, &WaveformWidget::seekRequested, m_audioPlayer, &AudioPlayer::seek);
    connect(m_timelineWidget, &TimelineWidget::seekRequested, m_audioPlayer, &AudioPlayer::seek);

    // Seek to subtitle when clicked in list
    connect(m_subtitleList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        double t = item->data(Qt::UserRole).toDouble();
        m_audioPlayer->seek(t);
    });

    // Subtitle inline editing → write back to lyrics data
    connect(m_subtitleList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        int idx = item->data(Qt::UserRole + 1).toInt();
        if (m_project && idx >= 0 && idx < m_project->lyrics.lines.size()) {
            QString newText = item->text().trimmed();
            if (newText.isEmpty()) return;
            m_project->lyrics.lines[idx].text = newText.toStdString();
            // Also rebuild each word's text (join all words)
            m_project->lyrics.lines[idx].tokens.clear();
            QStringList parts = newText.split(" ", Qt::SkipEmptyParts);
            const auto& line = m_project->lyrics.lines[idx];
            double dur = std::max(1.0, static_cast<double>(newText.length()) / 3.0);
            double wDur = dur / std::max(1, static_cast<int>(parts.size()));
            for (int i = 0; i < parts.size(); ++i) {
                double ws = line.start_time + i * wDur;
                m_project->lyrics.lines[idx].tokens.emplace_back(parts[i].toStdString(), ws, ws + wDur);
            }
            // m_project->isDirty = true; // Use simple state mechanism if needed
            m_previewWidget->loadLyrics(&m_project->lyrics);
            m_timelineWidget->loadLyrics(&m_project->lyrics);
            emit unsavedChangesChanged(true);
        }
    });

    // Subtitle input
    connect(m_addSubtitleBtn, &QPushButton::clicked, this, &EditorMode::onAddSubtitleClicked);
    connect(m_subtitleInput, &QLineEdit::returnPressed, this, &EditorMode::onAddSubtitleClicked);
    
    // Auto Whisper & Import
    connect(m_whisperBtn, &QPushButton::clicked, this, &EditorMode::onAutoWhisperClicked);
    connect(m_importBtn, &QPushButton::clicked, this, &EditorMode::onImportSubtitleClicked);
    
    connect(m_precisionBtn, &QPushButton::clicked, this, [this]() {
        auto* item = m_subtitleList->currentItem();
        if (!item) {
            QMessageBox::information(this, "Select Subtitle", "Please select a subtitle line first to enter Precision Mode.");
            return;
        }
        int idx = item->data(Qt::UserRole + 1).toInt();
        if (!m_project || idx < 0 || idx >= m_project->lyrics.lines.size()) return;
        
        WordEditor editor(&m_project->lyrics.lines[idx], this);
        if (editor.exec() == QDialog::Accepted && editor.wasModified()) {
            updateSubtitleList();
            m_previewWidget->loadLyrics(&m_project->lyrics);
            m_timelineWidget->loadLyrics(&m_project->lyrics);
            emit unsavedChangesChanged(true);
        }
    });

    // Timeline subtitle drag → update project lyrics then refresh list + preview
    connect(m_timelineWidget, &TimelineWidget::subtitleMoved,
            this, [this](int idx, double newStart, double newEnd) {
        if (!m_project || idx < 0 || idx >= m_project->lyrics.lines.size()) return;
        auto& ln = m_project->lyrics.lines[idx];
        double dur = newEnd - newStart;
        // Shift all word times by the same delta
        double delta = newStart - ln.start_time;
        for (auto& w : ln.tokens) { w.start_time += delta; w.end_time += delta; }
        ln.start_time = newStart;
        ln.end_time   = newEnd;
        // m_project->isDirty = true;
        updateSubtitleList();
        m_previewWidget->loadLyrics(&m_project->lyrics);
        emit unsavedChangesChanged(true);
    });

    connect(m_timelineWidget, &TimelineWidget::subtitleDeleted,
            this, [this](int idx) {
        if (!m_project || idx < 0 || idx >= m_project->lyrics.lines.size()) return;
        m_project->lyrics.lines.erase(m_project->lyrics.lines.begin() + idx);
        updateSubtitleList();
        m_previewWidget->loadLyrics(&m_project->lyrics);
        m_timelineWidget->loadLyrics(&m_project->lyrics);
        emit unsavedChangesChanged(true);
    });

    connect(m_timelineWidget, &TimelineWidget::subtitleDoubleClicked,
            this, [this](int idx) {
        if (!m_project || idx < 0 || idx >= m_project->lyrics.lines.size()) return;
        
        WordEditor editor(&m_project->lyrics.lines[idx], this);
        if (editor.exec() == QDialog::Accepted && editor.wasModified()) {
            updateSubtitleList();
            m_previewWidget->loadLyrics(&m_project->lyrics);
            m_timelineWidget->loadLyrics(&m_project->lyrics);
            emit unsavedChangesChanged(true);
        }
    });

    // Context menu on the left panel list
    m_subtitleList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_subtitleList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        auto* item = m_subtitleList->itemAt(pos);
        if (!item) return;
        QMenu menu(this);

        // 1. Precision Word Editor
        menu.addAction(QStringLiteral("⏱  Precision Word Timing..."), this, [this, item]() {
            int idx = item->data(Qt::UserRole + 1).toInt();
            if (!m_project || idx < 0 || idx >= m_project->lyrics.lines.size()) return;
            
            WordEditor editor(&m_project->lyrics.lines[idx], this);
            if (editor.exec() == QDialog::Accepted && editor.wasModified()) {
                updateSubtitleList();
                m_previewWidget->loadLyrics(&m_project->lyrics);
                m_timelineWidget->loadLyrics(&m_project->lyrics);
                emit unsavedChangesChanged(true);
            }
        });

        menu.addSeparator();

        // 2. Delete
        menu.addAction(QStringLiteral("✕  Delete"), this, [this, item]() {
            int idx = item->data(Qt::UserRole + 1).toInt();
            if (!m_project || idx < 0 || idx >= m_project->lyrics.lines.size()) return;
            m_project->lyrics.lines.erase(m_project->lyrics.lines.begin() + idx);
            updateSubtitleList();
            m_previewWidget->loadLyrics(&m_project->lyrics);
            m_timelineWidget->loadLyrics(&m_project->lyrics);
            emit unsavedChangesChanged(true);
        });
        
        menu.exec(m_subtitleList->mapToGlobal(pos));
    });

    // Delete key shortcut
    auto* delShortcut = new QShortcut(QKeySequence::Delete, m_subtitleList);
    connect(delShortcut, &QShortcut::activated, this, [this]() {
        auto* item = m_subtitleList->currentItem();
        if (!item || !m_project) return;
        int idx = item->data(Qt::UserRole + 1).toInt();
        if (idx < 0 || idx >= m_project->lyrics.lines.size()) return;
        m_project->lyrics.lines.erase(m_project->lyrics.lines.begin() + idx);
        updateSubtitleList();
        m_previewWidget->loadLyrics(&m_project->lyrics);
        m_timelineWidget->loadLyrics(&m_project->lyrics);
        emit unsavedChangesChanged(true);
    });

    // Connect video output to preview panel (enables MP4 video display)
    // NOTE: We use a *dedicated* muted video player, not the audio stem player,
    // so video keeps showing when switching to Instrumental/Vocals stems.
    // (Video player is initialised in syncViewState once project paths are known.)

    // Mirror play/pause state from audio player to video player
    connect(m_audioPlayer->player(), &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState state) {
        if (!m_videoPlayer) return;
        if (state == QMediaPlayer::PlayingState) {
            m_videoPlayer->play();
        } else {
            // NEVER stop, always retain paused state so video frames decode while scrubbing!
            m_videoPlayer->pause();
        }
    });
}

void EditorMode::applyTheme() {
    // Premiere Pro Style Dark Theme
    QString qss = R"(
        QWidget {
            background-color: #1e1e1e;
            color: #d4d4d4;
            font-family: "Segoe UI", "Helvetica Neue", Arial, sans-serif;
            font-size: 13px;
        }
        
        QSplitter::handle {
            background-color: #111111;
            margin: 1px;
        }

        QToolBar {
            background-color: #252526;
            border-bottom: 1px solid #111111;
            padding: 4px;
        }

        QPushButton {
            background-color: #333333;
            border: 1px solid #111111;
            border-radius: 3px;
            padding: 5px 10px;
            color: #ffffff;
        }

        QPushButton:hover {
            background-color: #404040;
        }

        QPushButton:pressed {
            background-color: #202020;
        }

        QLineEdit, QComboBox {
            background-color: #2d2d30;
            border: 1px solid #3f3f46;
            border-radius: 3px;
            padding: 4px;
            color: #ffffff;
        }

        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #007acc;
        }

        QComboBox::drop-down {
            border: none;
        }
    )";
    this->setStyleSheet(qss);
}

void EditorMode::syncViewState() {
    if (!m_project) return;
    
    // Initialise the muted video player from the original source file
    if (m_project->source_file.has_value() && !m_project->source_file.value().empty()) {
        if (!m_videoPlayer) {
            m_videoPlayer = new QMediaPlayer(this);
            m_videoAudioOutput = new QAudioOutput(this);
            m_videoAudioOutput->setVolume(0.0); // muted — audio comes from AudioPlayer
            m_videoPlayer->setAudioOutput(m_videoAudioOutput);
            m_previewWidget->setMediaPlayer(m_videoPlayer);
        }
        m_videoPlayer->setSource(QUrl::fromLocalFile(QString::fromStdString(m_project->source_file.value().string())));
        m_videoPlayer->pause(); // force frame decoding for the initial static preview
    }
    
    // Default audio logic
    if (m_project->instrumental_file.has_value() && !m_project->instrumental_file.value().empty()) {
        m_audioPlayer->loadSource(QString::fromStdString(m_project->instrumental_file.value().string()));
        m_trackSelector->blockSignals(true);
        m_trackSelector->setCurrentIndex(1); // Instrumental
        m_trackSelector->blockSignals(false);
        qDebug() << "EditorMode: Loaded instrumental stem:" << QString::fromStdString(m_project->instrumental_file.value().string());
    } else if (m_project->audio_file.has_value() && !m_project->audio_file.value().empty()) {
        m_audioPlayer->loadSource(QString::fromStdString(m_project->audio_file.value().string()));
        m_trackSelector->blockSignals(true);
        m_trackSelector->setCurrentIndex(0); // Original
        m_trackSelector->blockSignals(false);
        qDebug() << "EditorMode: Loaded original audio:" << QString::fromStdString(m_project->audio_file.value().string());
    }

    // Load project data into components
    m_timelineWidget->loadTimeline(&m_project->timeline);
    m_timelineWidget->loadLyrics(&m_project->lyrics);
    m_previewWidget->loadLyrics(&m_project->lyrics);
    updateSubtitleList();
}

void EditorMode::onPlayPauseToggled(bool isPlaying) {
    qDebug() << "Play state changed:" << isPlaying;
}

void EditorMode::onTimecodeChanged(double timeSeconds) {
    m_currentTime = timeSeconds;
    m_previewWidget->updateTime(timeSeconds);
    m_waveformWidget->updateCursor(timeSeconds);
    m_timelineWidget->updateCursor(timeSeconds);

    // Keep Lyrical Pro teleprompter in sync
    if (m_lyricalPro && m_rootStack->currentIndex() == 1) {
        m_lyricalPro->updateTime(timeSeconds);
    }
    
    // Keep video player position in sync (only correct if drifted > 300ms)
    if (m_videoPlayer) {
        qint64 videoPosMs  = m_videoPlayer->position();
        qint64 audioPosMs  = static_cast<qint64>(timeSeconds * 1000.0);
        qint64 diff = videoPosMs - audioPosMs;
        if (diff < -300 || diff > 300)
            m_videoPlayer->setPosition(audioPosMs);
    }
}

void EditorMode::onLineSelected(int lineIndex) {
    // Synchronize selection across components
    // m_waveformWidget->zoomToLine(lineIndex);
}

void EditorMode::onWordSelected(int lineIndex, int wordIndex) {
    // Update inspector or property panel
}

void EditorMode::onWordsChanged() {
    emit unsavedChangesChanged(true);
}

void EditorMode::updateSubtitleList() {
    if (!m_project) return;
    m_subtitleList->blockSignals(true);  // prevent itemChanged firing during repopulate
    m_subtitleList->clear();
    const auto& lines = m_project->lyrics.lines;
    for (int i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        int mins  = static_cast<int>(line.start_time) / 60;
        double secs = std::fmod(line.start_time, 60.0);
        QString ts = QString("%1:%2")
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 5, 'f', 2, QChar('0'));
        auto* item = new QListWidgetItem(QString::fromStdString(line.text));  // text only — editable inline
        item->setToolTip(QString("Start: %1  (double-click to edit)").arg(ts));
        item->setData(Qt::UserRole,     line.start_time);
        item->setData(Qt::UserRole + 1, i);            // line index for write-back
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_subtitleList->addItem(item);
    }
    m_subtitleList->blockSignals(false);
    // Also update the timeline subtitle track
    m_timelineWidget->loadLyrics(&m_project->lyrics);
    // Refresh Lyrical Pro view if visible
    if (m_lyricalPro) m_lyricalPro->loadLyrics(&m_project->lyrics);
}


void EditorMode::onTrackSelectionChanged(int index) {
    if (!m_project) return;

    QString newSource;
    switch (index) {
        case 0: // Original
            if (m_project->audio_file.has_value()) newSource = QString::fromStdString(m_project->audio_file.value().string());
            break;
        case 1: // Instrumental
            if (m_project->instrumental_file.has_value()) newSource = QString::fromStdString(m_project->instrumental_file.value().string());
            break;
        case 2: // Vocals Only
            if (m_project->vocals_file.has_value()) newSource = QString::fromStdString(m_project->vocals_file.value().string());
            break;
    }

    if (!newSource.isEmpty()) {
        m_audioPlayer->loadSource(newSource);
        qDebug() << "EditorMode: Switched audio track to:" << newSource;
    } else {
        qDebug() << "EditorMode: Selected track source is not available.";
    }
}

void EditorMode::onAddSubtitleClicked() {
    if (!m_project) return;

    QString text = m_subtitleInput->text().trimmed();
    if (text.isEmpty()) return;

    // Create a new lyric line starting at current time
    core::LyricsLine newLine;
    newLine.text = text.toStdString();
    newLine.start_time = m_currentTime;
    
    // Estimate end time based on number of characters (very rough approximation)
    // About 3 chars per second for a typical song
    double estimatedDuration = std::max(1.0, text.length() / 3.0);
    newLine.end_time = m_currentTime + estimatedDuration; 
    
    // SyllableEditor expects words to be populated to render the blocks
    // Split text by space and add as words
    QStringList words = text.split(" ", Qt::SkipEmptyParts);
    if (!words.isEmpty()) {
        double wordDuration = estimatedDuration / words.size();
        for (int i = 0; i < words.size(); ++i) {
            double wordStart = m_currentTime + (i * wordDuration);
            newLine.tokens.emplace_back(words[i].toStdString(), wordStart, wordStart + wordDuration);
        }
    } else {
        // Fallback
        newLine.tokens.emplace_back(text.toStdString(), m_currentTime, m_currentTime + estimatedDuration);
    }
    
    m_project->lyrics.lines.push_back(std::move(newLine));
    
    m_subtitleInput->clear();
    
    updateSubtitleList();
    m_previewWidget->loadLyrics(&m_project->lyrics);
    emit unsavedChangesChanged(true);
}

void EditorMode::onAutoWhisperClicked() {
    if (!m_project || (!m_project->vocals_file.has_value() && !m_project->audio_file.has_value())) {
        QMessageBox::warning(this, "No Audio", "No viable audio track exists to transcribe.");
        return;
    }
    
    QString targetAudio = m_project->vocals_file.has_value() && !m_project->vocals_file.value().empty()
                          ? QString::fromStdString(m_project->vocals_file.value().string()) 
                          : QString::fromStdString(m_project->audio_file.value().string());

    if (QMessageBox::question(this, "Run Whisper?", "This will overwrite all existing subtitles with AI transcription.\nContinue?") != QMessageBox::Yes) {
        return;
    }

    m_whisperBtn->setEnabled(false);
    m_importBtn->setEnabled(false);
    m_langCombo->setEnabled(false);
    m_statusLabel->setText("AI Processing...");
    
    auto* overlay = new ProcessingOverlay(this);
    overlay->show();
    
    auto* tWorker = new TranscriptionWorker(this);
    connect(tWorker, &TranscriptionWorker::transcriptionComplete, this, [this, overlay](const QString& resultJson) {
        m_statusLabel->setText("AI Complete");
        m_whisperBtn->setEnabled(true);
        m_importBtn->setEnabled(true);
        m_langCombo->setEnabled(true);
        overlay->deleteLater();
        m_project->lyrics.importFromWhisperJson(resultJson.toStdString());
        updateSubtitleList();
        m_previewWidget->loadLyrics(&m_project->lyrics);
        emit unsavedChangesChanged(true);
    });
    connect(tWorker, &TranscriptionWorker::error, this, [this, overlay](const QString& err) {
        m_statusLabel->setText("Failed");
        m_whisperBtn->setEnabled(true);
        m_importBtn->setEnabled(true);
        m_langCombo->setEnabled(true);
        overlay->deleteLater();
        QMessageBox::warning(this, "Transcription Failed", err);
    });
    connect(tWorker, &TranscriptionWorker::progressUpdated, this, [overlay](const QString& msg){
        overlay->msg->setText(msg);
        overlay->bar->setValue(50);
    });
    
    QString langCode = m_langCombo->currentText();
    tWorker->startTranscription(targetAudio, "base", langCode);
}

void EditorMode::onImportSubtitleClicked() {
    if (!m_project) return;
    QString filter = "Subtitle Files (*.lrc *.srt *.txt *.json);;Lyric (*.lrc);;SubRip (*.srt);;Text (*.txt);;Whisper JSON (*.json);;All Files (*)";
    QString path = QFileDialog::getOpenFileName(this, "Import Subtitles", "", filter);
    if (path.isEmpty()) return;

    if (QMessageBox::question(this, "Import?", "This will replace your current subtitles with the exact contents of the selected file.\nContinue?") != QMessageBox::Yes) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not read file: " + path);
        return;
    }

    QString ext = QFileInfo(path).suffix().toLower();
    QString contents = QString::fromUtf8(file.readAll());
    
    // We will parse standard .lrc format manually or implement importFrom... on LyricsData in Phase 6
    if (ext == "lrc") {
        // m_project->lyrics.importFromLrc(contents);
    } else if (ext == "srt") {
        // m_project->lyrics.importFromSrt(contents);
    } else if (ext == "json") {
        // m_project->lyrics.importFromWhisperJson(contents);
    } else {
        // m_project->lyrics.importFromText(contents);
    }
    
    updateSubtitleList();
    m_previewWidget->loadLyrics(&m_project->lyrics);
    emit unsavedChangesChanged(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Lyrical Pro Mode
// ─────────────────────────────────────────────────────────────────────────────

void EditorMode::toggleLyricalPro() {
    if (!m_rootStack || !m_lyricalPro) return;

    if (m_rootStack->currentIndex() == 0) {
        // Switch TO Lyrical Pro
        syncLyricalProState();
        m_rootStack->setCurrentIndex(1);
        m_lyricalProBtn->setText("▣  Normal Mode");
        m_lyricalProBtn->setStyleSheet(
            "QPushButton { background: #3d5a3e; color: #c8f0c8; border-radius: 5px;"
            "              border: none; font-size: 12px; font-weight: bold; padding: 4px 12px; }"
            "QPushButton:hover   { background: #4e724f; }"
            "QPushButton:pressed { background: #2b3f2c; }"
        );

        // Wire exit signals (safe to reconnect — Qt deduplicates)
        connect(m_lyricalPro, &LyricalProWidget::exitRequested,
                this, &EditorMode::exitLyricalPro, Qt::UniqueConnection);
        connect(m_lyricalPro, &LyricalProWidget::playPauseToggled,
                this, [this]() {
            if (m_audioPlayer) {
                auto* p = m_audioPlayer->player();
                if (p->playbackState() == QMediaPlayer::PlayingState)
                    p->pause();
                else
                    p->play();
            }
        }, Qt::UniqueConnection);
        connect(m_lyricalPro, &LyricalProWidget::seekBackward,
                this, [this]() {
            if (m_audioPlayer) m_audioPlayer->seek(std::max(0.0, m_currentTime - 5.0));
        }, Qt::UniqueConnection);
        connect(m_lyricalPro, &LyricalProWidget::seekForward,
                this, [this]() {
            if (m_audioPlayer) m_audioPlayer->seek(m_currentTime + 5.0);
        }, Qt::UniqueConnection);
    } else {
        exitLyricalPro();
    }
}

void EditorMode::exitLyricalPro() {
    if (!m_rootStack) return;
    m_rootStack->setCurrentIndex(0);
    if (m_lyricalProBtn) {
        m_lyricalProBtn->setText("🎤 Lyrical Pro Mode");
        m_lyricalProBtn->setStyleSheet(
            "QPushButton { background: #8b4513; color: #ffeedd; border-radius: 5px;"
            "              border: none; font-size: 12px; font-weight: bold; padding: 4px 12px; }"
            "QPushButton:hover   { background: #a0522d; }"
            "QPushButton:pressed { background: #6b3010; }"
        );
    }
}

void EditorMode::syncLyricalProState() {
    if (!m_lyricalPro || !m_project) return;

    m_lyricalPro->loadLyrics(&m_project->lyrics);
    m_lyricalPro->updateTime(m_currentTime);

    // Set project title if available
    if (m_project->source_file.has_value()) {
        QString src = QString::fromStdString(m_project->source_file.value().stem().string());
        m_lyricalPro->setProjectTitle(src);
    }

    // Mirror current playback state
    bool isPlaying = m_audioPlayer &&
                     m_audioPlayer->player()->playbackState() == QMediaPlayer::PlayingState;
    m_lyricalPro->setPlaying(isPlaying);
}

} // namespace ncktv
