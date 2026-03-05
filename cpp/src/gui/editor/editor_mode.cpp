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
            m_project->lyrics.lines[idx].text = newText;
            // Also rebuild each word's text (join all words)
            m_project->lyrics.lines[idx].words.clear();
            QStringList parts = newText.split(" ", Qt::SkipEmptyParts);
            const auto& line = m_project->lyrics.lines[idx];
            double dur = std::max(1.0, static_cast<double>(newText.length()) / 3.0);
            double wDur = dur / std::max(1, static_cast<int>(parts.size()));
            for (int i = 0; i < parts.size(); ++i) {
                double ws = line.startTime + i * wDur;
                m_project->lyrics.lines[idx].addWord(parts[i], ws, ws + wDur);
            }
            m_project->isDirty = true;
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

    // Timeline subtitle drag → update project lyrics then refresh list + preview
    connect(m_timelineWidget, &TimelineWidget::subtitleMoved,
            this, [this](int idx, double newStart, double newEnd) {
        if (!m_project || idx < 0 || idx >= m_project->lyrics.lines.size()) return;
        auto& ln = m_project->lyrics.lines[idx];
        double dur = newEnd - newStart;
        // Shift all word times by the same delta
        double delta = newStart - ln.startTime;
        for (auto& w : ln.words) { w.startTime += delta; w.endTime += delta; }
        ln.startTime = newStart;
        ln.endTime   = newEnd;
        m_project->isDirty = true;
        updateSubtitleList();
        m_previewWidget->loadLyrics(&m_project->lyrics);
        emit unsavedChangesChanged(true);
    });

    connect(m_timelineWidget, &TimelineWidget::subtitleDeleted,
            this, [this](int idx) {
        if (!m_project || idx < 0 || idx >= m_project->lyrics.lines.size()) return;
        m_project->lyrics.lines.removeAt(idx);
        m_project->isDirty = true;
        updateSubtitleList();
        m_previewWidget->loadLyrics(&m_project->lyrics);
        m_timelineWidget->loadLyrics(&m_project->lyrics);
        emit unsavedChangesChanged(true);
    });

    // Delete key on the left panel list
    m_subtitleList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_subtitleList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        auto* item = m_subtitleList->itemAt(pos);
        if (!item) return;
        QMenu menu(this);
        menu.addAction(QStringLiteral("✕  Delete"), this, [this, item]() {
            int idx = item->data(Qt::UserRole + 1).toInt();
            if (!m_project || idx < 0 || idx >= m_project->lyrics.lines.size()) return;
            m_project->lyrics.lines.removeAt(idx);
            m_project->isDirty = true;
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
        m_project->lyrics.lines.removeAt(idx);
        m_project->isDirty = true;
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
    if (m_project->sourceFile.has_value() && !m_project->sourceFile.value().isEmpty()) {
        if (!m_videoPlayer) {
            m_videoPlayer = new QMediaPlayer(this);
            m_videoAudioOutput = new QAudioOutput(this);
            m_videoAudioOutput->setVolume(0.0); // muted — audio comes from AudioPlayer
            m_videoPlayer->setAudioOutput(m_videoAudioOutput);
            m_previewWidget->setMediaPlayer(m_videoPlayer);
        }
        m_videoPlayer->setSource(QUrl::fromLocalFile(m_project->sourceFile.value()));
        m_videoPlayer->pause(); // force frame decoding for the initial static preview
    }
    
    // Default audio logic
    if (m_project->instrumentalPath.has_value() && !m_project->instrumentalPath.value().isEmpty()) {
        m_audioPlayer->loadSource(m_project->instrumentalPath.value());
        m_trackSelector->blockSignals(true);
        m_trackSelector->setCurrentIndex(1); // Instrumental
        m_trackSelector->blockSignals(false);
        qDebug() << "EditorMode: Loaded instrumental stem:" << m_project->instrumentalPath.value();
    } else if (m_project->originalAudioPath.has_value() && !m_project->originalAudioPath.value().isEmpty()) {
        m_audioPlayer->loadSource(m_project->originalAudioPath.value());
        m_trackSelector->blockSignals(true);
        m_trackSelector->setCurrentIndex(0); // Original
        m_trackSelector->blockSignals(false);
        qDebug() << "EditorMode: Loaded original audio:" << m_project->originalAudioPath.value();
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
        int mins  = static_cast<int>(line.startTime) / 60;
        double secs = std::fmod(line.startTime, 60.0);
        QString ts = QString("%1:%2")
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 5, 'f', 2, QChar('0'));
        auto* item = new QListWidgetItem(line.text);  // text only — editable inline
        item->setToolTip(QString("Start: %1  (double-click to edit)").arg(ts));
        item->setData(Qt::UserRole,     line.startTime);
        item->setData(Qt::UserRole + 1, i);            // line index for write-back
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_subtitleList->addItem(item);
    }
    m_subtitleList->blockSignals(false);
    // Also update the timeline subtitle track
    m_timelineWidget->loadLyrics(&m_project->lyrics);
}


void EditorMode::onTrackSelectionChanged(int index) {
    if (!m_project) return;

    QString newSource;
    switch (index) {
        case 0: // Original
            if (m_project->originalAudioPath.has_value()) newSource = m_project->originalAudioPath.value();
            break;
        case 1: // Instrumental
            if (m_project->instrumentalPath.has_value()) newSource = m_project->instrumentalPath.value();
            break;
        case 2: // Vocals Only
            if (m_project->vocalsPath.has_value()) newSource = m_project->vocalsPath.value();
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
    LyricLine newLine;
    newLine.text = text;
    newLine.startTime = m_currentTime;
    
    // Estimate end time based on number of characters (very rough approximation)
    // About 3 chars per second for a typical song
    double estimatedDuration = std::max(1.0, text.length() / 3.0);
    newLine.endTime = m_currentTime + estimatedDuration; 
    
    // SyllableEditor expects words to be populated to render the blocks
    // Split text by space and add as words
    QStringList words = text.split(" ", Qt::SkipEmptyParts);
    if (!words.isEmpty()) {
        double wordDuration = estimatedDuration / words.size();
        for (int i = 0; i < words.size(); ++i) {
            double wordStart = m_currentTime + (i * wordDuration);
            newLine.addWord(words[i], wordStart, wordStart + wordDuration);
        }
    } else {
        // Fallback
        newLine.addWord(text, m_currentTime, m_currentTime + estimatedDuration);
    }
    
    m_project->lyrics.addLine(newLine);
    m_project->isDirty = true;
    
    m_subtitleInput->clear();
    
    updateSubtitleList();
    m_previewWidget->loadLyrics(&m_project->lyrics);
    emit unsavedChangesChanged(true);
}

void EditorMode::onAutoWhisperClicked() {
    if (!m_project || (!m_project->vocalsPath.has_value() && !m_project->originalAudioPath.has_value())) {
        QMessageBox::warning(this, "No Audio", "No viable audio track exists to transcribe.");
        return;
    }
    
    QString targetAudio = m_project->vocalsPath.has_value() && !m_project->vocalsPath.value().isEmpty()
                          ? m_project->vocalsPath.value() 
                          : m_project->originalAudioPath.value();

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
        m_project->lyrics.importFromWhisperJson(resultJson);
        m_project->isDirty = true;
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
    connect(tWorker, &TranscriptionWorker::progress, this, [overlay](int val, const QString& msg){
        overlay->msg->setText(msg);
        overlay->bar->setValue(val);
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
    
    if (ext == "lrc") {
        m_project->lyrics.importFromLrc(contents);
    } else if (ext == "srt") {
        m_project->lyrics.importFromSrt(contents);
    } else if (ext == "json") {
        m_project->lyrics.importFromWhisperJson(contents);
    } else {
        m_project->lyrics.importFromText(contents);
    }
    
    m_project->isDirty = true;
    updateSubtitleList();
    m_previewWidget->loadLyrics(&m_project->lyrics);
    emit unsavedChangesChanged(true);
}

} // namespace ncktv
