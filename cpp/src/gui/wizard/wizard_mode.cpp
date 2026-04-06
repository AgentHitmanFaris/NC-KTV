#include "wizard_mode.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include "workers/vocal_separator_worker.h"
#include "dialogs/lyrics_search_dialog.h"
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QFrame>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QThread>

namespace ncktv {

WizardMode::WizardMode(ConfigManager* config, QWidget* parent)
    : QWidget(parent)
    , m_config(config)
{
    setupUi();
}

void WizardMode::reset() {
    m_pages->setCurrentIndex(0);
    m_selectedFile.clear();
    m_onlineLyrics.clear();
    if (m_filePathEdit) m_filePathEdit->clear();
    if (m_startBtn) m_startBtn->setEnabled(false);
    if (m_dropTitle) m_dropTitle->setText("Upload Source");
    if (m_dropSubtitle) m_dropSubtitle->setText("Drag and drop MKV, MP4, or WAV files");
    // Drop zone default style is handled by QSS (QWidget#dropZone)
    // Clear any inline override set during file selection/drag
    if (m_dropZone) m_dropZone->setStyleSheet(QString());
}

void WizardMode::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_pages = new QStackedWidget(this);
    mainLayout->addWidget(m_pages);

    createWelcomePage();
    createProcessingPage();

    m_pages->setCurrentIndex(0);
}

// ════════════════════════════════════════════════════════════════════════════
// PAGE 1: PREMIERE PRO-STYLE START SCREEN
// Root: QHBoxLayout — Sidebar (260px) + Main content area (flex)
// ════════════════════════════════════════════════════════════════════════════

void WizardMode::createWelcomePage() {
    auto* page = new QWidget();
    auto* rootLayout = new QHBoxLayout(page);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── LEFT SIDEBAR ─────────────────────────────────────────────────────────
    auto* sidebar = new QWidget(page);
    sidebar->setObjectName("wizardSidebar");
    sidebar->setFixedWidth(260);
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(16);

    // Branding label
    auto* brandLabel = new QLabel("NC-KTV PRO", sidebar);
    brandLabel->setStyleSheet(
        "font-size: 16px; font-weight: 700; color: #e0e0e0;"
        "padding: 20px 16px 4px 16px; background: transparent; border: none;"
    );
    sideLayout->addWidget(brandLabel);

    // Separator
    auto* sep1 = new QFrame(sidebar);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("color: #3a3a3a; background: #3a3a3a; border: none; max-height: 1px;");
    sideLayout->addWidget(sep1);

    // New Project button
    auto* newBtn = new QPushButton("📁  New Project", sidebar);
    newBtn->setObjectName("navBtn");
    newBtn->setStyleSheet(
        "QPushButton#navBtn {"
        "  text-align: left; padding: 8px 16px;"
        "  background: #2d2d2d; border: 1px solid #3a3a3a;"
        "  color: #c8c8c8; border-radius: 0px;"
        "}"
        "QPushButton#navBtn:hover { background: #383838; }"
    );
    newBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(newBtn, &QPushButton::clicked, this, [this] { emit requestNew(); });
    sideLayout->addWidget(newBtn);

    // Open Project button
    auto* openBtn = new QPushButton("📂  Open Project", sidebar);
    openBtn->setObjectName("navBtn");
    openBtn->setStyleSheet(newBtn->styleSheet());
    openBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(openBtn, &QPushButton::clicked, this, [this] { emit requestOpen(); });
    sideLayout->addWidget(openBtn);

    // Separator
    auto* sep2 = new QFrame(sidebar);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("color: #3a3a3a; background: #3a3a3a; border: none; max-height: 1px;");
    sideLayout->addWidget(sep2);

    // Recent Projects section label
    auto* recentLabel = new QLabel("RECENT PROJECTS", sidebar);
    recentLabel->setStyleSheet(
        "font-size: 10px; color: #8a8a8a; font-weight: 700;"
        "letter-spacing: 1px; padding: 0px 16px; background: transparent; border: none;"
    );
    sideLayout->addWidget(recentLabel);

    // Recent files list (stretch, no border, #252525 bg)
    auto* recentList = new QListWidget(sidebar);
    recentList->setStyleSheet(
        "QListWidget { background: #252525; border: none; color: #c8c8c8; font-size: 12px; }"
        "QListWidget::item { padding: 6px 16px; }"
        "QListWidget::item:hover { background: rgba(255,255,255,0.05); }"
        "QListWidget::item:selected { background: rgba(74,158,255,0.15); color: #e0e0e0; }"
    );
    sideLayout->addWidget(recentList, 1);

    rootLayout->addWidget(sidebar);

    // ── RIGHT: MAIN CONTENT AREA ──────────────────────────────────────────────
    auto* contentArea = new QWidget(page);
    auto* contentLayout = new QVBoxLayout(contentArea);
    contentLayout->setContentsMargins(32, 32, 32, 32);
    contentLayout->setSpacing(24);
    contentLayout->setAlignment(Qt::AlignTop);

    // Pipeline Card
    auto* pipelineCard = new QWidget(contentArea);
    pipelineCard->setObjectName("pipelineCard");
    auto* pipeLayout = new QVBoxLayout(pipelineCard);
    pipeLayout->setContentsMargins(20, 20, 20, 20);
    pipeLayout->setSpacing(16);

    // Card header
    auto* cardHeader = new QLabel("Media Pipeline", pipelineCard);
    cardHeader->setStyleSheet(
        "font-size: 16px; font-weight: 700; color: #e0e0e0;"
        "background: transparent; border: none;"
    );
    pipeLayout->addWidget(cardHeader);

    // Drop Zone — objectName "dropZone", QSS handles default style
    m_dropZone = new QWidget(pipelineCard);
    m_dropZone->setObjectName("dropZone");
    m_dropZone->setAcceptDrops(true);
    m_dropZone->setMinimumHeight(140);
    m_dropZone->installEventFilter(this);

    auto* dropLayout = new QVBoxLayout(m_dropZone);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setContentsMargins(16, 20, 16, 20);
    dropLayout->setSpacing(8);

    auto* uploadIcon = new QLabel("↑", m_dropZone);
    uploadIcon->setAlignment(Qt::AlignCenter);
    uploadIcon->setStyleSheet(
        "font-size: 22px; color: #4a9eff; background: transparent; border: none;"
    );
    dropLayout->addWidget(uploadIcon, 0, Qt::AlignCenter);

    m_dropTitle = new QLabel("Upload Source", m_dropZone);
    m_dropTitle->setAlignment(Qt::AlignCenter);
    m_dropTitle->setStyleSheet(
        "font-weight: 600; font-size: 14px; color: #e0e0e0;"
        "background: transparent; border: none;"
    );
    dropLayout->addWidget(m_dropTitle);

    m_dropSubtitle = new QLabel("Drag and drop MKV, MP4, or WAV files", m_dropZone);
    m_dropSubtitle->setAlignment(Qt::AlignCenter);
    m_dropSubtitle->setWordWrap(true);
    m_dropSubtitle->setStyleSheet(
        "font-size: 11px; color: #8a8a8a; background: transparent; border: none;"
    );
    dropLayout->addWidget(m_dropSubtitle);
    dropLayout->addSpacing(4);

    // Browse + Search row
    auto* browseRow = new QHBoxLayout();
    browseRow->setAlignment(Qt::AlignCenter);
    browseRow->setSpacing(8);

    auto* browseBtn = new QPushButton("Browse Files", m_dropZone);
    connect(browseBtn, &QPushButton::clicked, this, &WizardMode::onBrowseFile);
    browseRow->addWidget(browseBtn);

    auto* searchBtn = new QPushButton("🔍 Search Lyrics", m_dropZone);
    connect(searchBtn, &QPushButton::clicked, this, [this]() {
        LyricsSearchDialog dialog(this);
        if (!m_filePathEdit->text().isEmpty()) {
            QFileInfo fi(m_filePathEdit->text());
            dialog.setInitialSearch(fi.baseName());
        }
        if (dialog.exec() == QDialog::Accepted) {
            m_onlineLyrics = dialog.syncedLyrics();
            if (m_onlineLyrics.isEmpty()) m_onlineLyrics = dialog.plainLyrics();
            if (!m_onlineLyrics.isEmpty()) {
                QMessageBox::information(this, "Lyrics Found",
                    "Synced lyrics found online! These will be used instead of AI transcription.");
                m_transcribeCheck->setChecked(false);
            }
        }
    });
    browseRow->addWidget(searchBtn);
    dropLayout->addLayout(browseRow);

    pipeLayout->addWidget(m_dropZone);

    // Hidden file path edit (logic only)
    m_filePathEdit = new QLineEdit(pipelineCard);
    m_filePathEdit->setVisible(false);
    pipeLayout->addWidget(m_filePathEdit);

    // Processing Profile section label
    auto* profileLabel = new QLabel("PROCESSING PROFILE", pipelineCard);
    profileLabel->setStyleSheet(
        "font-size: 10px; font-weight: 700; color: #8a8a8a;"
        "letter-spacing: 1px; background: transparent; border: none;"
    );
    pipeLayout->addWidget(profileLabel);

    // Options row: transcribe checkbox + language combo
    auto* optionsRow = new QHBoxLayout();
    m_transcribeCheck = new QCheckBox("Auto-generate Subs", pipelineCard);
    m_transcribeCheck->setChecked(true);
    optionsRow->addWidget(m_transcribeCheck);

    m_langCombo = new QComboBox(pipelineCard);
    m_langCombo->addItems({"Auto", "English (en)", "Malay (ms)", "Indonesian (id)",
                           "Japanese (ja)", "Korean (ko)", "Chinese (zh)"});
    optionsRow->addWidget(m_langCombo);
    pipeLayout->addLayout(optionsRow);

    // UVR model selector
    m_modelCombo = new QComboBox(pipelineCard);
    QStringList modelDirs = {
        QCoreApplication::applicationDirPath() + "/models/uvr",
        QDir::currentPath() + "/models/uvr"
    };
    QStringList found;
    for (const QString& dirPath : modelDirs) {
        QDir dir(dirPath);
        if (dir.exists()) {
            const auto entries = dir.entryList({"*.onnx", "*.pth"}, QDir::Files);
            for (const QString& f : entries) {
                if (!found.contains(f)) found << f;
            }
        }
    }
    if (found.isEmpty())
        found << "UVR_MDXNET_KARA_2.onnx" << "UVR-MDX-NET-Inst_HQ_3.onnx";
    m_modelCombo->addItems(found);

    if (m_config) {
        QString defUvr = m_config->get<QString>("ai.uvr_model", "UVR_MDXNET_KARA_2.onnx");
        int idx = m_modelCombo->findText(defUvr);
        if (idx != -1) m_modelCombo->setCurrentIndex(idx);
        else m_modelCombo->setCurrentText(defUvr);

        QString defLang = m_config->get<QString>("ai.language", "Auto");
        int lidx = m_langCombo->findText(defLang, Qt::MatchContains);
        if (lidx != -1) m_langCombo->setCurrentIndex(lidx);
    }
    pipeLayout->addWidget(m_modelCombo);

    // Start Engine button
    m_startBtn = new QPushButton("🚀 Start Engine", pipelineCard);
    m_startBtn->setObjectName("startEngineBtn");
    m_startBtn->setEnabled(false);
    m_startBtn->setMinimumHeight(40);
    connect(m_startBtn, &QPushButton::clicked, this, &WizardMode::onStartClicked);
    pipeLayout->addWidget(m_startBtn);

    contentLayout->addWidget(pipelineCard);
    contentLayout->addStretch();

    rootLayout->addWidget(contentArea, 1);

    m_pages->addWidget(page);
}

// ════════════════════════════════════════════════════════════════════════════
// Drag-and-drop event filter for the drop zone widget
// ════════════════════════════════════════════════════════════════════════════

bool WizardMode::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_dropZone) {
        if (event->type() == QEvent::DragEnter) {
            auto* de = static_cast<QDragEnterEvent*>(event);
            if (de->mimeData()->hasUrls()) {
                de->acceptProposedAction();
                // Drag-over: bright dashed blue border
                m_dropZone->setStyleSheet(
                    "QWidget#dropZone {"
                    "  background-color: #1e1e1e;"
                    "  border: 2px dashed #4a9eff;"
                    "  border-radius: 4px;"
                    "}"
                );
                return true;
            }
        } else if (event->type() == QEvent::DragLeave) {
            // Restore QSS default
            m_dropZone->setStyleSheet(QString());
            return true;
        } else if (event->type() == QEvent::Drop) {
            auto* de = static_cast<QDropEvent*>(event);
            const QList<QUrl> urls = de->mimeData()->urls();
            if (!urls.isEmpty()) {
                QString path = urls.first().toLocalFile();
                if (!path.isEmpty()) {
                    m_filePathEdit->setText(path);
                    m_selectedFile = path;
                    m_onlineLyrics.clear();
                    m_startBtn->setEnabled(true);

                    QFileInfo fi(path);
                    m_dropTitle->setText("Selected File:");
                    m_dropSubtitle->setText(fi.fileName());

                    // File dropped: solid accent border at 40% opacity
                    m_dropZone->setStyleSheet(
                        "QWidget#dropZone {"
                        "  background-color: #1e1e1e;"
                        "  border: 2px solid rgba(74,158,255,0.4);"
                        "  border-radius: 4px;"
                        "}"
                    );
                }
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ════════════════════════════════════════════════════════════════════════════
// PAGE 2: PREMIERE PRO-STYLE PROCESSING PAGE
// Centered QWidget#progressCard (480x220) on full #1e1e1e background
// ════════════════════════════════════════════════════════════════════════════

void WizardMode::createProcessingPage() {
    auto* page = new QWidget();
    auto* outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Centered progress card
    auto* progressCard = new QWidget(page);
    progressCard->setObjectName("progressCard");
    progressCard->setFixedSize(480, 220);
    auto* pcLayout = new QVBoxLayout(progressCard);
    pcLayout->setContentsMargins(28, 20, 28, 20);
    pcLayout->setSpacing(12);

    // Status row: blue dot + "ACTIVE PROCESSING"
    auto* statusRow = new QHBoxLayout();
    auto* dot = new QLabel("●", progressCard);
    dot->setStyleSheet("color: #4a9eff; font-size: 10px; background: transparent; border: none;");
    statusRow->addWidget(dot);
    auto* activeLabel = new QLabel("ACTIVE PROCESSING", progressCard);
    activeLabel->setStyleSheet(
        "color: #4a9eff; font-size: 11px; font-weight: 700;"
        "letter-spacing: 1px; background: transparent; border: none;"
    );
    statusRow->addWidget(activeLabel);
    statusRow->addStretch();
    pcLayout->addLayout(statusRow);

    // Task status label (18px bold, #e0e0e0)
    m_taskStatusLabel = new QLabel("Separating Audio...", progressCard);
    m_taskStatusLabel->setStyleSheet(
        "font-size: 18px; font-weight: 700; color: #e0e0e0;"
        "background: transparent; border: none;"
    );
    pcLayout->addWidget(m_taskStatusLabel);

    // Status detail label (13px, #8a8a8a)
    m_statusLabel = new QLabel("Extracting vocals and instruments", progressCard);
    m_statusLabel->setStyleSheet(
        "font-size: 13px; color: #8a8a8a; background: transparent; border: none;"
    );
    pcLayout->addWidget(m_statusLabel);

    // Progress bar — objectName "processingBar", QSS handles all styling
    m_progressBar = new QProgressBar(progressCard);
    m_progressBar->setObjectName("processingBar");
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(false);
    pcLayout->addWidget(m_progressBar);

    // Percent label (42px bold, #4a9eff)
    m_percentLabel = new QLabel("0%", progressCard);
    m_percentLabel->setStyleSheet(
        "font-size: 42px; font-weight: 700; color: #4a9eff;"
        "background: transparent; border: none;"
    );
    m_percentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pcLayout->addWidget(m_percentLabel);

    outerLayout->addStretch();
    outerLayout->addWidget(progressCard, 0, Qt::AlignCenter);
    outerLayout->addStretch();

    m_pages->addWidget(page);
}

// ════════════════════════════════════════════════════════════════════════════
// INTERACTIVITY — all existing logic preserved unchanged
// ════════════════════════════════════════════════════════════════════════════

void WizardMode::onBrowseFile() {
    QString path = QFileDialog::getOpenFileName(this, "Select Media", "",
        "Media Files (*.mp4 *.mkv *.mp3 *.wav *.flac);;All Files (*)");
    if (!path.isEmpty()) {
        m_filePathEdit->setText(path);
        m_selectedFile = path;
        m_onlineLyrics.clear();
        m_startBtn->setEnabled(true);

        QFileInfo fi(path);
        m_dropTitle->setText("Selected File:");
        m_dropSubtitle->setText(fi.fileName());
        // File selected via browse: solid accent border (same as drop)
        m_dropZone->setStyleSheet(
            "QWidget#dropZone {"
            "  background-color: #1e1e1e;"
            "  border: 2px solid rgba(74,158,255,0.4);"
            "  border-radius: 4px;"
            "}"
        );
    }
}

void WizardMode::onStartClicked() {
    m_pages->setCurrentIndex(1);

    m_project = new Project(m_selectedFile);

    m_taskStatusLabel->setText("Separating Audio...");
    m_statusLabel->setText("Extracting vocals and instruments from source media");
    m_progressBar->setValue(10);
    m_percentLabel->setText("10%");

    // Start vocal separation worker in a background thread
    auto* thread = new QThread(this);
    auto* worker = new VocalSeparatorWorker();
    worker->moveToThread(thread);

    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    connect(worker, &VocalSeparatorWorker::separationComplete, this, &WizardMode::onSeparationFinished);
    connect(worker, &VocalSeparatorWorker::error, this, &WizardMode::onSeparationError);
    connect(worker, &VocalSeparatorWorker::progress, this, [this](int val, const QString& msg) {
        m_progressBar->setValue(val);
        m_percentLabel->setText(QString("%1%").arg(val));
        m_statusLabel->setText(msg);
    });

    connect(worker, &VocalSeparatorWorker::separationComplete, thread, &QThread::quit);
    connect(worker, &VocalSeparatorWorker::error, thread, &QThread::quit);

    thread->start();

    QString selectedModel = m_modelCombo ? m_modelCombo->currentText() : "UVR_MDXNET_KARA_2.onnx";
    if (selectedModel.isEmpty()) selectedModel = "UVR_MDXNET_KARA_2.onnx";

    QMetaObject::invokeMethod(worker, "startSeparation", Qt::QueuedConnection,
                              Q_ARG(QString, m_selectedFile),
                              Q_ARG(QString, selectedModel),
                              Q_ARG(QString, "output"));
}

void WizardMode::onSeparationFinished(const QString& instrumentalPath, const QString& vocalsPath) {
    m_progressBar->setValue(100);
    m_percentLabel->setText("100%");
    m_taskStatusLabel->setText("Processing Complete");
    m_statusLabel->setText("Loading project...");

    m_project->instrumentalPath = instrumentalPath;
    m_project->vocalsPath = vocalsPath;
    m_project->originalAudioPath = m_selectedFile;

    double duration = 180.0;

    auto& track = m_project->timeline.addTrack("inst_track", TrackType::Audio, "Instrumental");

    Clip instClip;
    instClip.clipId = "inst_clip_0";
    instClip.trackId = "inst_track";
    instClip.sourceFile = instrumentalPath;
    instClip.startTime = 0.0;
    instClip.duration = duration;

    track.addClip(instClip);

    qDebug() << "Separation Finished! Inst:" << instrumentalPath << "Vocals:" << vocalsPath << "Duration:" << duration;

    if (!m_onlineLyrics.isEmpty()) {
        m_project->lyrics.importFromLrc(m_onlineLyrics);
        emit projectReady(m_project);
    } else if (m_transcribeCheck->isChecked() && !vocalsPath.isEmpty()) {
        m_progressBar->setValue(0);
        m_percentLabel->setText("0%");
        m_taskStatusLabel->setText("Transcribing Vocals...");

        QString engineStr = m_config ? m_config->get<QString>("ai.transcription_engine", "whisperx") : "whisperx";
        TranscriptionEngine engine = (engineStr == "whisper")
            ? TranscriptionEngine::Whisper
            : TranscriptionEngine::WhisperX;
        QString engineLabel = (engine == TranscriptionEngine::WhisperX) ? "WhisperX" : "Whisper";

        m_statusLabel->setText(QString("AI lyrics transcription in progress (%1)").arg(engineLabel));

        auto* tThread = new QThread(this);
        auto* tWorker = new TranscriptionWorker();
        tWorker->moveToThread(tThread);

        connect(tThread, &QThread::finished, tWorker, &QObject::deleteLater);
        connect(tThread, &QThread::finished, tThread, &QObject::deleteLater);

        connect(tWorker, &TranscriptionWorker::transcriptionComplete, this, &WizardMode::onTranscriptionFinished);
        connect(tWorker, &TranscriptionWorker::error, this, &WizardMode::onTranscriptionError);
        connect(tWorker, &TranscriptionWorker::progressUpdated, this, [this](const QString& msg) {
            m_progressBar->setValue(50);
            m_percentLabel->setText("50%");
            m_statusLabel->setText(msg);
        });

        connect(tWorker, &TranscriptionWorker::transcriptionComplete, tThread, &QThread::quit);
        connect(tWorker, &TranscriptionWorker::error, tThread, &QThread::quit);

        QString langStr = m_langCombo->currentText();
        QString langCode = "auto";
        if (langStr.contains("(")) {
            langCode = langStr.split("(").last().replace(")", "").trimmed();
        }

        tThread->start();

        QString whisperModel = m_config ? m_config->get<QString>("ai.whisper_model", "medium") : "medium";

        QMetaObject::invokeMethod(tWorker, "startTranscription", Qt::QueuedConnection,
                                  Q_ARG(QString, vocalsPath),
                                  Q_ARG(QString, whisperModel),
                                  Q_ARG(QString, langCode),
                                  Q_ARG(ncktv::TranscriptionEngine, engine));
    } else {
        emit projectReady(m_project);
    }
}

void WizardMode::onTranscriptionFinished(const QString& resultJson) {
    m_progressBar->setValue(100);
    m_percentLabel->setText("100%");
    m_taskStatusLabel->setText("Complete!");
    m_statusLabel->setText("Launching Editor...");

    m_project->lyrics.importFromWhisperJson(resultJson);

    emit projectReady(m_project);
}

void WizardMode::onTranscriptionError(const QString& error) {
    QMessageBox::warning(this, "Transcription Failed",
        "Could not transcribe vocals:\n" + error + "\n\nProceeding without lyrics.");
    emit projectReady(m_project);
}

void WizardMode::onSeparationError(const QString& error) {
    QMessageBox::critical(this, "Separation Error",
        "Failed to separate audio stems:\n" + error);
    m_pages->setCurrentIndex(0);
}

} // namespace ncktv
