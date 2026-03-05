#include "wizard_mode.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QHBoxLayout>
#include "workers/vocal_separator_worker.h"
#include "audio/audio_processor.h"
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
    
    // Start vocal separation worker
    auto* worker = new VocalSeparatorWorker(this);
    connect(worker, &VocalSeparatorWorker::separationComplete, this, &WizardMode::onSeparationFinished);
    connect(worker, &VocalSeparatorWorker::error, this, &WizardMode::onSeparationError);
    connect(worker, &VocalSeparatorWorker::progress, this, [this](int val, const QString& msg){
        m_progressBar->setValue(val);
        m_statusLabel->setText(msg);
    });

    worker->startSeparation(m_selectedFile, "6_HP-Karaoke-UVR.pth", "output");
}

void WizardMode::onSeparationFinished(const QString& instrumentalPath, const QString& vocalsPath) {
    m_progressBar->setValue(100);
    m_statusLabel->setText("Success! Loading project...");
    
    // Store results in project data
    m_project->instrumentalPath = instrumentalPath;
    m_project->vocalsPath = vocalsPath;
    m_project->originalAudioPath = m_selectedFile;
    
    // Use AudioProcessor to get duration for the timeline clip
    AudioProcessor ap("temp/audio");
    double duration = ap.getDuration(instrumentalPath);
    
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
        
        auto* tWorker = new TranscriptionWorker(this);
        connect(tWorker, &TranscriptionWorker::transcriptionComplete, this, &WizardMode::onTranscriptionFinished);
        connect(tWorker, &TranscriptionWorker::error, this, &WizardMode::onTranscriptionError);
        connect(tWorker, &TranscriptionWorker::progress, this, [this](int val, const QString& msg){
            m_progressBar->setValue(val);
            m_statusLabel->setText(msg);
        });
        
        QString langStr = m_langCombo->currentText();
        QString langCode = "auto";
        if (langStr.contains("(")) {
            langCode = langStr.split("(").last().replace(")", "").trimmed();
        }
        
        tWorker->startTranscription(vocalsPath, "base", langCode);
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
