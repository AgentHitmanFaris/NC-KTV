#include "wizard_mode.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QHBoxLayout>
#include "workers/vocal_separator_worker.h"
#include <QFileInfo>

namespace ncktv {

WizardMode::WizardMode(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void WizardMode::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);

    m_pages = new QStackedWidget(this);
    mainLayout->addWidget(m_pages);

    createWelcomePage();
    createProcessingPage();
    
    m_pages->setCurrentIndex(0);
}

void WizardMode::createWelcomePage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    QLabel* title = new QLabel("Welcome to NC-KTV", this);
    title->setStyleSheet("font-size: 32px; font-weight: bold; color: #6366f1;");
    layout->addWidget(title, 0, Qt::AlignCenter);

    QLabel* sub = new QLabel("Select a video or audio file to start your karaoke project.", this);
    sub->setStyleSheet("font-size: 16px; color: #64748b;");
    layout->addWidget(sub, 0, Qt::AlignCenter);

    auto* fileLayout = new QHBoxLayout();
    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setPlaceholderText("Path to media file...");
    m_filePathEdit->setFixedWidth(400);
    m_filePathEdit->setStyleSheet("padding: 10px; border-radius: 5px; border: 1px solid #cbd5e1;");
    
    QPushButton* browseBtn = new QPushButton("Browse...", this);
    browseBtn->setStyleSheet("padding: 10px 20px; background-color: #f1f5f9; border-radius: 5px; color: black;");
    connect(browseBtn, &QPushButton::clicked, this, &WizardMode::onBrowseFile);

    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(browseBtn);
    layout->addLayout(fileLayout);

    auto* transcribeLayout = new QHBoxLayout();
    transcribeLayout->setAlignment(Qt::AlignCenter);

    m_transcribeCheck = new QCheckBox("Auto-transcribe (Whisper): ", this);
    m_transcribeCheck->setChecked(true);
    m_transcribeCheck->setStyleSheet("color: #475569; font-size: 14px; margin-top: 10px;");
    transcribeLayout->addWidget(m_transcribeCheck);
    
    m_langCombo = new QComboBox(this);
    m_langCombo->addItems({"Auto", "English (en)", "Malay (ms)", "Indonesian (id)", "Japanese (ja)", "Korean (ko)", "Chinese (zh)"});
    m_langCombo->setStyleSheet("margin-top: 10px; padding: 4px;");
    transcribeLayout->addWidget(m_langCombo);
    
    layout->addLayout(transcribeLayout);

    m_startBtn = new QPushButton("🚀 Start Magic", this);
    m_startBtn->setEnabled(false);
    m_startBtn->setFixedWidth(200);
    m_startBtn->setStyleSheet("padding: 12px; font-weight: bold; background-color: #6366f1; color: white; border-radius: 8px; font-size: 16px;");
    connect(m_startBtn, &QPushButton::clicked, this, &WizardMode::onStartClicked);
    layout->addWidget(m_startBtn, 0, Qt::AlignCenter);

    m_pages->addWidget(page);
}

void WizardMode::createProcessingPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(30);

    m_statusLabel = new QLabel("Initializing separation model...", this);
    m_statusLabel->setStyleSheet("font-size: 18px; color: #475569;");
    layout->addWidget(m_statusLabel, 0, Qt::AlignCenter);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedWidth(500);
    m_progressBar->setStyleSheet("QProgressBar { border: 2px solid grey; border-radius: 5px; height: 25px; text-align: center; } QProgressBar::chunk { background-color: #6366f1; width: 20px; }");
    layout->addWidget(m_progressBar);

    m_pages->addWidget(page);
}

void WizardMode::onBrowseFile() {
    QString path = QFileDialog::getOpenFileName(this, "Select Media", "", "Media Files (*.mp4 *.mkv *.mp3 *.wav *.flac);;All Files (*)");
    if (!path.isEmpty()) {
        m_filePathEdit->setText(path);
        m_selectedFile = path;
        m_startBtn->setEnabled(true);
    }
}

void WizardMode::onStartClicked() {
    m_pages->setCurrentIndex(1);
    
    m_project = new Project(m_selectedFile);
    
    m_statusLabel->setText("Separating vocals & instruments...");
    m_progressBar->setValue(10);
    
    // Start vocal separation worker in a background thread
    auto* thread = new QThread(this);
    auto* worker = new VocalSeparatorWorker(); // No parent, will be moved
    worker->moveToThread(thread);
    
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    connect(worker, &VocalSeparatorWorker::separationComplete, this, &WizardMode::onSeparationFinished);
    connect(worker, &VocalSeparatorWorker::error, this, &WizardMode::onSeparationError);
    connect(worker, &VocalSeparatorWorker::progress, this, [this](int val, const QString& msg){
        m_progressBar->setValue(val);
        m_statusLabel->setText(msg);
    });

    // Cleanup thread when work is done
    connect(worker, &VocalSeparatorWorker::separationComplete, thread, &QThread::quit);
    connect(worker, &VocalSeparatorWorker::error, thread, &QThread::quit);

    thread->start();
    
    QMetaObject::invokeMethod(worker, "startSeparation", Qt::QueuedConnection,
                              Q_ARG(QString, m_selectedFile),
                              Q_ARG(QString, "UVR_MDXNET_KARA_2.onnx"),
                              Q_ARG(QString, "output"));
}

void WizardMode::onSeparationFinished(const QString& instrumentalPath, const QString& vocalsPath) {
    m_progressBar->setValue(100);
    m_statusLabel->setText("Success! Loading project...");
    
    // Store results in project data
    m_project->instrumentalPath = instrumentalPath;
    m_project->vocalsPath = vocalsPath;
    m_project->originalAudioPath = m_selectedFile;
    
    // Removed AudioProcessor usage due to C++17 native migration
    double duration = 180.0; // Dummy duration for now until QMediaPlayer/DSPEngine is linked
    
    // Add audio track to the timeline for the instrumental
    auto& track = m_project->timeline.addTrack("inst_track", TrackType::Audio, "Instrumental");
    
    Clip instClip;
    instClip.clipId = "inst_clip_0";
    instClip.trackId = "inst_track";
    instClip.sourceFile = instrumentalPath;
    instClip.startTime = 0.0;
    instClip.duration = duration;
    
    track.addClip(instClip);

    qDebug() << "Separation Finished! Inst:" << instrumentalPath << "Vocals:" << vocalsPath << "Duration:" << duration;
    
    if (m_transcribeCheck->isChecked() && !vocalsPath.isEmpty()) {
        m_progressBar->setValue(0);
        m_statusLabel->setText("Transcribing vocals (this may take a minute)...");
        
        // Start transcription worker in a background thread
        auto* tThread = new QThread(this);
        auto* tWorker = new TranscriptionWorker(); // No parent, will be moved
        tWorker->moveToThread(tThread);
        
        connect(tThread, &QThread::finished, tWorker, &QObject::deleteLater);
        connect(tThread, &QThread::finished, tThread, &QObject::deleteLater);

        connect(tWorker, &TranscriptionWorker::transcriptionComplete, this, &WizardMode::onTranscriptionFinished);
        connect(tWorker, &TranscriptionWorker::error, this, &WizardMode::onTranscriptionError);
        connect(tWorker, &TranscriptionWorker::progressUpdated, this, [this](const QString& msg){
            m_progressBar->setValue(50);
            m_statusLabel->setText(msg);
        });
        
        // Cleanup thread when work is done
        connect(tWorker, &TranscriptionWorker::transcriptionComplete, tThread, &QThread::quit);
        connect(tWorker, &TranscriptionWorker::error, tThread, &QThread::quit);

        QString langStr = m_langCombo->currentText();
        QString langCode = "auto";
        if (langStr.contains("(")) {
            langCode = langStr.split("(").last().replace(")", "").trimmed();
        }
        
        tThread->start();
        
        QMetaObject::invokeMethod(tWorker, "startTranscription", Qt::QueuedConnection,
                                  Q_ARG(QString, vocalsPath),
                                  Q_ARG(QString, "base"),
                                  Q_ARG(QString, langCode));
    } else {
        emit projectReady(m_project);
    }
}

void WizardMode::onTranscriptionFinished(const QString& resultJson) {
    m_progressBar->setValue(100);
    m_statusLabel->setText("Transcription Complete! Loading project...");
    
    m_project->lyrics.importFromWhisperJson(resultJson);
    
    emit projectReady(m_project);
}

void WizardMode::onTranscriptionError(const QString& error) {
    QMessageBox::warning(this, "Transcription Failed", "Could not transcribe vocals:\n" + error + "\n\nProceeding without lyrics.");
    emit projectReady(m_project);
}

void WizardMode::onSeparationError(const QString& error) {
    QMessageBox::critical(this, "Separation Error", "Failed to separate audio stems:\n" + error);
    m_pages->setCurrentIndex(0);
}

} // namespace ncktv
