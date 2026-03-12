#include "precision_mode.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QComboBox>
#include <QAction>
#include <QMenu>

namespace ncktv {

PrecisionMode::PrecisionMode(std::shared_ptr<core::Project> project, QWidget* parent)
    : QWidget(parent)
    , m_project(std::move(project))
{
    m_undoManager = new UndoManager(this);
    
    setupUi();
    setupToolBar();
    applyTheme();
    setupConnections();
    syncViewState();
}

void PrecisionMode::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_toolBar = new QToolBar(this);
    mainLayout->addWidget(m_toolBar);

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(m_mainSplitter, 1);

    // ─── LEFT PANE : Subtitle List ───────────────────────────────────────────
    QWidget* leftWidget = new QWidget(m_mainSplitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    
    QLabel* listHeader = new QLabel("✦ Subtitle Lines", leftWidget);
    listHeader->setStyleSheet("background:#252526; color:#ccc; font-weight:bold; border-bottom:1px solid #111; padding:8px;");
    leftLayout->addWidget(listHeader);

    m_subtitleList = new QListWidget(leftWidget);
    m_subtitleList->setStyleSheet(
        "QListWidget { background:#1e1e1e; border:none; color:#ddd; }"
        "QListWidget::item { padding:8px; border-bottom:1px solid #2d2d30; }"
        "QListWidget::item:selected { background:#007acc; color:#fff; }"
        "QListWidget::item:hover { background:#2d2d30; }"
    );
    leftLayout->addWidget(m_subtitleList, 1);
    m_mainSplitter->addWidget(leftWidget);

    // ─── RIGHT PANE : Word Editor (Waveform) ─────────────────────────────────
    QWidget* rightWidget = new QWidget(m_mainSplitter);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(16, 14, 16, 14);
    rightLayout->setSpacing(10);
    
    // Waveform Toolbar
    auto* editorToolbar = new QHBoxLayout();
    m_stickyCheck = new QCheckBox("🧲 Sticky Borders", rightWidget);
    m_stickyCheck->setChecked(true);
    editorToolbar->addWidget(m_stickyCheck);
    editorToolbar->addSpacing(20);

    m_addBtn = new QPushButton("＋ Add Word", rightWidget);
    m_splitBtn = new QPushButton("✂ Split", rightWidget);
    m_joinBtn = new QPushButton("⫘ Join Next", rightWidget);
    m_deleteBtn = new QPushButton("✕ Delete", rightWidget);
    m_playWordBtn = new QPushButton("▶ Play Segment", rightWidget);
    
    editorToolbar->addWidget(m_addBtn);
    editorToolbar->addWidget(m_splitBtn);
    editorToolbar->addWidget(m_joinBtn);
    editorToolbar->addWidget(m_deleteBtn);
    editorToolbar->addWidget(m_playWordBtn);
    editorToolbar->addStretch();

    editorToolbar->addWidget(new QLabel("Zoom: ", rightWidget));
    m_zoomSlider = new QSlider(Qt::Horizontal, rightWidget);
    m_zoomSlider->setRange(50, 1000);
    m_zoomSlider->setValue(300);
    m_zoomSlider->setFixedWidth(150);
    editorToolbar->addWidget(m_zoomSlider);

    rightLayout->addLayout(editorToolbar);

    // Canvas container and right side editor list
    QSplitter* innerSplitter = new QSplitter(Qt::Horizontal, rightWidget);
    
    m_scrollArea = new QScrollArea(innerSplitter);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border:1px solid #3f3f46; border-radius:4px; min-height: 250px; }");
    m_canvas = new WordCanvas(m_scrollArea);
    m_scrollArea->setWidget(m_canvas);
    innerSplitter->addWidget(m_scrollArea);

    // Right-side vertical word list
    QWidget* editContainer = new QWidget(innerSplitter);
    QVBoxLayout* editContainerLayout = new QVBoxLayout(editContainer);
    editContainerLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* editHeader = new QLabel("Lyrics Map:", editContainer);
    editContainerLayout->addWidget(editHeader);

    m_wordTable = new QTableWidget(0, 1, editContainer);
    m_wordTable->horizontalHeader()->setStretchLastSection(true);
    m_wordTable->horizontalHeader()->hide();
    m_wordTable->verticalHeader()->hide();
    m_wordTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wordTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_wordTable->setStyleSheet(
        "QTableWidget { background:#252526; color:#ccc; border:1px solid #3f3f46; }"
        "QTableWidget::item:selected { background:#007acc; color:white; }"
    );
    editContainerLayout->addWidget(m_wordTable, 1);
    innerSplitter->addWidget(editContainer);
    
    innerSplitter->setSizes({800, 200});
    rightLayout->addWidget(innerSplitter, 1);

    // Editor fields bottom (just the time label now)
    auto* editLayout = new QHBoxLayout();
    editLayout->addStretch();
    m_timeLabel = new QLabel("-- / --", rightWidget);
    m_timeLabel->setFixedWidth(150);
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    editLayout->addWidget(m_timeLabel);
    rightLayout->addLayout(editLayout);

    // Audio Player & Video Preview at bottom
    auto* bottomLayout = new QHBoxLayout();
    m_audioPlayer = new AudioPlayer(rightWidget);
    bottomLayout->addWidget(m_audioPlayer, 1);

    m_previewWidget = new KaraokePreview(rightWidget);
    m_previewWidget->setFixedSize(320, 180);
    bottomLayout->addWidget(m_previewWidget, 0);

    rightLayout->addLayout(bottomLayout);

    m_mainSplitter->addWidget(rightWidget);
    m_mainSplitter->setSizes({ 300, 900 });
}

void PrecisionMode::setupToolBar() {
    m_toolBar->addAction("Save", this, [this]() { emit requestSave(); });
    m_toolBar->addSeparator();
    m_toolBar->addAction("Undo", m_undoManager, &UndoManager::undo);
    m_toolBar->addAction("Redo", m_undoManager, &UndoManager::redo);
    m_toolBar->addSeparator();

    m_trackSelector = new QComboBox(this);
    m_trackSelector->addItems({"Original", "Instrumental", "Vocals Only"});
    m_toolBar->addWidget(m_trackSelector);
}

void PrecisionMode::setupConnections() {
    connect(m_subtitleList, &QListWidget::itemClicked, this, &PrecisionMode::onSubtitleSelected);
    connect(m_canvas, &WordCanvas::selectionChanged, this, &PrecisionMode::onWordSelectionChanged);
    connect(m_canvas, &WordCanvas::wordModified, this, &PrecisionMode::onWordModified);
    connect(m_canvas, &WordCanvas::seekRequested, this, [this](double t) {
        m_autoStopAt = -1.0;
        m_segmentStartTime = -1.0;
        m_audioPlayer->seek(t);
    });
    connect(m_stickyCheck, &QCheckBox::toggled, m_canvas, &WordCanvas::setSticky);
    connect(m_zoomSlider, &QSlider::valueChanged, this, &PrecisionMode::onZoomChanged);
    
    connect(m_addBtn, &QPushButton::clicked, this, &PrecisionMode::onAddWord);
    connect(m_splitBtn, &QPushButton::clicked, this, &PrecisionMode::onSplitWord);
    connect(m_joinBtn, &QPushButton::clicked, this, &PrecisionMode::onJoinWord);
    connect(m_deleteBtn, &QPushButton::clicked, this, &PrecisionMode::onDeleteWord);
    connect(m_playWordBtn, &QPushButton::clicked, this, &PrecisionMode::onPlayWord);
    
    connect(m_wordTable, &QTableWidget::cellChanged, this, &PrecisionMode::onTableCellChanged);
    connect(m_wordTable, &QTableWidget::cellClicked, this, [this](int row, int col) {
        m_canvas->setSelectedIndex(row);
    });

    connect(m_trackSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PrecisionMode::onTrackSelectionChanged);
            
    connect(m_audioPlayer, &AudioPlayer::positionChanged, this, &PrecisionMode::onTimecodeChanged);

    // Sync video player status with audio player
    connect(m_audioPlayer->player(), &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (m_videoPlayer) {
            if (state == QMediaPlayer::PlayingState) {
                // Ensure synced timeline start when hitting play
                m_videoPlayer->setPosition(m_audioPlayer->player()->position());
                m_videoPlayer->play();
            } else {
                m_videoPlayer->pause();
            }
        }
    });

    // Seek video when audio player creates a seek request
    connect(m_audioPlayer, &AudioPlayer::seekRequested, this, [this](double t) {
        m_autoStopAt = -1.0;
        m_segmentStartTime = -1.0;
        if (m_videoPlayer) m_videoPlayer->setPosition(static_cast<qint64>(t * 1000.0));
    });
}

void PrecisionMode::applyTheme() {
    QString qss = R"(
        QWidget { background-color: #1e1e1e; color: #d4d4d4; font-size: 13px; }
        QToolBar { background-color: #252526; border-bottom: 1px solid #111111; padding: 4px; }
        QPushButton { background-color: #333333; border: 1px solid #555; border-radius: 3px; padding: 5px 10px; color: #ccc; }
        QPushButton:hover { background-color: #404040; }
        QPushButton:disabled { color:#555; border-color:#444; }
        QSplitter::handle { background-color: #111111; margin: 1px; }
    )";
    setStyleSheet(qss);
}

void PrecisionMode::syncViewState() {
    if (!m_project) return;
    
    // Initialize the muted video player from the original source file
    if (m_project->source_file.has_value() && !m_project->source_file.value().empty()) {
        if (!m_videoPlayer) {
            m_videoPlayer = new QMediaPlayer(this);
            m_videoAudioOutput = new QAudioOutput(this);
            m_videoAudioOutput->setVolume(0.0); // muted
            m_videoPlayer->setAudioOutput(m_videoAudioOutput);
            m_previewWidget->setMediaPlayer(m_videoPlayer);
        }
        m_videoPlayer->setSource(QUrl::fromLocalFile(QString::fromStdString(m_project->source_file.value().string())));
        m_videoPlayer->pause();
    }
    m_previewWidget->loadLyrics(&m_project->lyrics);
    
    updateSubtitleList();
    if (m_project->instrumental_file.has_value()) {
        m_audioPlayer->loadSource(QString::fromStdString(m_project->instrumental_file.value().string()));
        m_trackSelector->setCurrentIndex(1);
    } else if (m_project->audio_file.has_value()) {
        m_audioPlayer->loadSource(QString::fromStdString(m_project->audio_file.value().string()));
        m_trackSelector->setCurrentIndex(0);
    }
}

void PrecisionMode::updateSubtitleList() {
    if (!m_project) return;
    
    m_subtitleList->blockSignals(true);
    m_subtitleList->clear();
    
    for (int i = 0; i < m_project->lyrics.lines.size(); ++i) {
        const auto& line = m_project->lyrics.lines[i];
        
        int mins  = static_cast<int>(line.start_time) / 60;
        double secs = std::fmod(line.start_time, 60.0);
        QString ts = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 5, 'f', 2, QChar('0'));
        
        auto* item = new QListWidgetItem(QString("[%1] %2").arg(ts, QString::fromStdString(line.text)));
        item->setData(Qt::UserRole, i);
        m_subtitleList->addItem(item);
    }
    m_subtitleList->blockSignals(false);
}

void PrecisionMode::onSubtitleSelected(QListWidgetItem* item) {
    if (!m_project || !item) return;
    m_currentLineIdx = item->data(Qt::UserRole).toInt();
    
    auto& line = m_project->lyrics.lines[m_currentLineIdx];
    m_canvas->setLine(&line);

    // Populate the right side table with words
    m_wordTable->blockSignals(true);
    m_wordTable->clearContents();
    m_wordTable->setRowCount(line.tokens.size());
    for (int i = 0; i < line.tokens.size(); ++i) {
        auto* tItem = new QTableWidgetItem(QString::fromStdString(line.tokens[i].text));
        m_wordTable->setItem(i, 0, tItem);
    }
    m_wordTable->blockSignals(false);
    
    m_canvas->setSelectedIndex(0);

    // Auto-scroll audio player correctly
    m_audioPlayer->seek(line.start_time);
}

void PrecisionMode::onWordSelectionChanged(int index) {
    updateButtons();
    if (!m_project || m_currentLineIdx < 0 || index < 0) return;
    
    const auto& line = m_project->lyrics.lines[m_currentLineIdx];
    if (index < line.tokens.size()) {
        m_wordTable->blockSignals(true);
        m_wordTable->selectRow(index);
        m_wordTable->blockSignals(false);

        onWordModified(); // updates the time label
        // Auto-play from this word? Just seek.
        m_audioPlayer->seek(line.tokens[index].start_time);
    }
}

void PrecisionMode::onWordModified() {
    if (!m_project || m_currentLineIdx < 0) return;
    emit unsavedChangesChanged(true);
    // m_project->isDirty = true;
    
    int idx = m_canvas->selectedIndex();
    auto& line = m_project->lyrics.lines[m_currentLineIdx];
    if (idx >= 0 && idx < line.tokens.size()) {
        const auto& w = line.tokens[idx];
        m_timeLabel->setText(QString("%1s - %2s\nDur: %3s")
            .arg(w.start_time, 0, 'f', 2).arg(w.end_time, 0, 'f', 2).arg(w.end_time - w.start_time, 0, 'f', 2));
    }
    
    // Automatically rebuild the parent lyric line's text block so it matches the words
    QStringList parts;
    for (const auto& w : line.tokens) if (!QString::fromStdString(w.text).isEmpty()) parts << QString::fromStdString(w.text);
    QString::fromStdString(line.text) = parts.join(' ');
    
    if (!line.tokens.empty()) {
        line.start_time = line.tokens.front().start_time;
        line.end_time = line.tokens.back().end_time;
    }
    
    // Sync list item text
    m_subtitleList->item(m_currentLineIdx)->setText(QString("[%1] %2")
        .arg(QString::number(line.start_time, 'f', 2), QString::fromStdString(line.text)));
        
    // Optionally update table text if it was modified outside the table
    if (idx >= 0 && idx < line.tokens.size() && m_wordTable->item(idx, 0)) {
        m_wordTable->blockSignals(true);
        m_wordTable->item(idx, 0)->setText(QString::fromStdString(line.tokens[idx].text));
        m_wordTable->blockSignals(false);
    }
}

void PrecisionMode::onTableCellChanged(int row, int col) {
    if (!m_project || m_currentLineIdx < 0) return;
    auto& line = m_project->lyrics.lines[m_currentLineIdx];
    if (row >= 0 && row < line.tokens.size()) {
        auto* item = m_wordTable->item(row, col);
        if (item) {
            line.tokens[row].text = item->text().toStdString();
            onWordModified();
            m_canvas->update();
        }
    }
}

void PrecisionMode::updateButtons() {
    int idx = m_canvas->selectedIndex();
    auto* line = (m_project && m_currentLineIdx >= 0) ? &m_project->lyrics.lines[m_currentLineIdx] : nullptr;
    bool hasSel = (line && idx >= 0 && idx < line->tokens.size());
    
    m_splitBtn->setEnabled(hasSel);
    m_deleteBtn->setEnabled(hasSel);
    m_joinBtn->setEnabled(hasSel && idx < line->tokens.size() - 1);
    m_playWordBtn->setEnabled(hasSel);
    
    if (!hasSel) {
        m_timeLabel->setText("-- / --");
    }
}

void PrecisionMode::onZoomChanged(int value) { m_canvas->setScale(value); }

void PrecisionMode::onAddWord() {
    if (!m_project || m_currentLineIdx < 0) return;
    auto& line = m_project->lyrics.lines[m_currentLineIdx];
    double start = line.tokens.empty() ? line.start_time : line.tokens.back().end_time;
    line.tokens.push_back({"new_word", static_cast<float>(start), static_cast<float>(start + 0.5)});
    m_canvas->setLine(&line);
    m_canvas->setSelectedIndex(line.tokens.size() - 1);
    onWordModified();
}

void PrecisionMode::onSplitWord() {
    if (!m_project || m_currentLineIdx < 0) return;
    int idx = m_canvas->selectedIndex();
    auto& line = m_project->lyrics.lines[m_currentLineIdx];
    if (idx < 0 || idx >= line.tokens.size()) return;
    
    auto& orig = line.tokens[idx];
    double midTime = (orig.start_time + orig.end_time) / 2.0;
    QString text = QString::fromStdString(orig.text);
    int midChar = qMax(1, text.length() / 2);
    
    core::LyricsToken word2{text.mid(midChar).toStdString(), static_cast<float>(midTime), orig.end_time};
    orig.text = text.left(midChar).toStdString();
    orig.end_time = static_cast<float>(midTime);
    
    line.tokens.insert(line.tokens.begin() + idx + 1, word2);
    m_canvas->setLine(&line);
    m_canvas->setSelectedIndex(idx + 1);
    onWordModified();
}

void PrecisionMode::onJoinWord() {
    if (!m_project || m_currentLineIdx < 0) return;
    int idx = m_canvas->selectedIndex();
    auto& line = m_project->lyrics.lines[m_currentLineIdx];
    if (idx < 0 || idx >= line.tokens.size() - 1) return;
    
    line.tokens[idx].text += " " + line.tokens[idx + 1].text;
    line.tokens[idx].end_time = line.tokens[idx + 1].end_time;
    line.tokens.erase(line.tokens.begin() + idx + 1);
    
    m_canvas->setLine(&line);
    m_canvas->setSelectedIndex(idx);
    onWordModified();
}

void PrecisionMode::onDeleteWord() {
    if (!m_project || m_currentLineIdx < 0) return;
    int idx = m_canvas->selectedIndex();
    auto& line = m_project->lyrics.lines[m_currentLineIdx];
    if (idx < 0 || idx >= line.tokens.size()) return;
    
    line.tokens.erase(line.tokens.begin() + idx);
    m_canvas->setLine(&line);
    m_canvas->setSelectedIndex(qMin<int>(idx, line.tokens.size() - 1));
    onWordModified();
}

void PrecisionMode::onPlayWord() {
    if (!m_project || m_currentLineIdx < 0) return;
    int idx = m_canvas->selectedIndex();
    auto& line = m_project->lyrics.lines[m_currentLineIdx];
    if (idx < 0 || idx >= line.tokens.size()) return;
    
    const auto& word = line.tokens[idx];
    m_autoStopAt = word.end_time;
    m_segmentStartTime = word.start_time;
    m_audioPlayer->seek(word.start_time);
    if (m_audioPlayer->player()->playbackState() != QMediaPlayer::PlayingState) {
        m_audioPlayer->player()->play();
    }
}

void PrecisionMode::onTrackSelectionChanged(int index) {
    if (!m_project) return;
    QString src;
    switch (index) {
        case 0: src = m_project->audio_file.has_value() ? QString::fromStdString(m_project->audio_file.value().string()) : ""; break;
        case 1: src = m_project->instrumental_file.has_value() ? QString::fromStdString(m_project->instrumental_file.value().string()) : ""; break;
        case 2: src = m_project->vocals_file.has_value() ? QString::fromStdString(m_project->vocals_file.value().string()) : ""; break;
    }
    if (!src.isEmpty()) m_audioPlayer->loadSource(src);
}

void PrecisionMode::onTimecodeChanged(double timeSeconds) {
    if (m_canvas) {
        m_canvas->setCurrentTime(timeSeconds);

        // Auto-stop isolation playback for a chosen segment
        if (m_autoStopAt > 0.0) {
            // Because QMediaPlayer processes seek() async, it can mistakenly emit positionChanged 
            // with the OLD position first. Therefore, we wait for timeSeconds to enter our expected segment window.
            if (m_segmentStartTime >= 0.0 && timeSeconds >= m_segmentStartTime && timeSeconds <= m_autoStopAt) {
                // Successfully playing inside the segment window
                m_segmentStartTime = -1.0;
            } else if (m_segmentStartTime < 0.0 && timeSeconds >= m_autoStopAt) {
                // Successfully reached the end of the segment!
                m_audioPlayer->player()->pause();
                m_autoStopAt = -1.0; // Reset
            }
        }
        
        // Text Auto-Scroll in real-time
        if (m_scrollArea) {
            auto* bar = m_scrollArea->verticalScrollBar();
            // Scroll to keep head around middle
            if (bar && m_audioPlayer->player()->playbackState() == QMediaPlayer::PlayingState) {
                int y = m_canvas->yAtTime(timeSeconds);
                int viewportHeight = m_scrollArea->viewport()->height();
                
                // Keep playhead reasonably centered
                bar->setValue(y - viewportHeight / 2);
            }
        }
    }
}

} // namespace ncktv
