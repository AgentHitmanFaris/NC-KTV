#include "wizard_mode.h"
#include "ui_wizard_mode.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QThread>
#include "workers/vocal_separator_worker.h"
#include "workers/transcription_worker.h"
#include "dialogs/lyrics_search_dialog.h"

namespace ncktv {

WizardMode::WizardMode(ConfigManager* config, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::WizardMode)
    , m_config(config)
{
    ui->setupUi(this);
    setupUi();
}

WizardMode::~WizardMode() {
    delete ui;
}

void WizardMode::reset() {
    ui->m_pages->setCurrentIndex(0);
    m_selectedFile.clear();
    m_onlineLyrics.clear();
    ui->m_filePathEdit->clear();
    ui->m_startBtn->setEnabled(false);
    ui->m_dropTitle->setText("Upload Source");
    ui->m_dropSubtitle->setText("Drag and drop MKV, MP4, or WAV files");
    ui->m_dropZone->setStyleSheet(QString());
}

void WizardMode::setupUi() {
    // Connect signals
    connect(ui->newBtn, &QPushButton::clicked, this, [this] { emit requestNew(); });
    connect(ui->openBtn, &QPushButton::clicked, this, [this] { emit requestOpen(); });
    connect(ui->browseBtn, &QPushButton::clicked, this, &WizardMode::onBrowseFile);
    connect(ui->m_startBtn, &QPushButton::clicked, this, &WizardMode::onStartClicked);

    connect(ui->searchBtn, &QPushButton::clicked, this, [this]() {
        LyricsSearchDialog dialog(this);
        if (!ui->m_filePathEdit->text().isEmpty()) {
            QFileInfo fi(ui->m_filePathEdit->text());
            dialog.setInitialSearch(fi.baseName());
        }
        if (dialog.exec() == QDialog::Accepted) {
            m_onlineLyrics = dialog.syncedLyrics();
            if (m_onlineLyrics.isEmpty()) m_onlineLyrics = dialog.plainLyrics();
            if (!m_onlineLyrics.isEmpty()) {
                QMessageBox::information(this, "Lyrics Found",
                    "Synced lyrics found online! These will be used instead of AI transcription.");
                ui->m_transcribeCheck->setChecked(false);
            }
        }
    });

    ui->m_dropZone->installEventFilter(this);

    // Initial Lang items
    ui->m_langCombo->addItems({"Auto", "English (en)", "Malay (ms)", "Indonesian (id)",
                           "Japanese (ja)", "Korean (ko)", "Chinese (zh)"});

    // UVR model selector
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
    ui->m_modelCombo->addItems(found);

    if (m_config) {
        QString defUvr = m_config->get<QString>("ai.uvr_model", "UVR_MDXNET_KARA_2.onnx");
        int idx = ui->m_modelCombo->findText(defUvr);
        if (idx != -1) ui->m_modelCombo->setCurrentIndex(idx);
        else ui->m_modelCombo->setCurrentText(defUvr);

        QString defLang = m_config->get<QString>("ai.language", "Auto");
        int lidx = ui->m_langCombo->findText(defLang, Qt::MatchContains);
        if (lidx != -1) ui->m_langCombo->setCurrentIndex(lidx);
    }
}

bool WizardMode::eventFilter(QObject* obj, QEvent* event) {
    if (obj == ui->m_dropZone) {
        if (event->type() == QEvent::DragEnter) {
            auto* de = static_cast<QDragEnterEvent*>(event);
            if (de->mimeData()->hasUrls()) {
                de->acceptProposedAction();
                ui->m_dropZone->setStyleSheet(
                    "QWidget#m_dropZone {"
                    "  background-color: #1e1e1e;"
                    "  border: 2px dashed #4a9eff;"
                    "  border-radius: 4px;"
                    "}"
                );
                return true;
            }
        } else if (event->type() == QEvent::DragLeave) {
            ui->m_dropZone->setStyleSheet(QString());
            return true;
        } else if (event->type() == QEvent::Drop) {
            auto* de = static_cast<QDropEvent*>(event);
            const QList<QUrl> urls = de->mimeData()->urls();
            if (!urls.isEmpty()) {
                QString path = urls.first().toLocalFile();
                if (!path.isEmpty()) {
                    ui->m_filePathEdit->setText(path);
                    m_selectedFile = path;
                    m_onlineLyrics.clear();
                    ui->m_startBtn->setEnabled(true);

                    QFileInfo fi(path);
                    ui->m_dropTitle->setText("Selected File:");
                    ui->m_dropSubtitle->setText(fi.fileName());

                    ui->m_dropZone->setStyleSheet(
                        "QWidget#m_dropZone {"
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

void WizardMode::onBrowseFile() {
    QString path = QFileDialog::getOpenFileName(this, "Select Media", "",
        "Media Files (*.mp4 *.mkv *.mp3 *.wav *.flac);;All Files (*)");
    if (!path.isEmpty()) {
        ui->m_filePathEdit->setText(path);
        m_selectedFile = path;
        m_onlineLyrics.clear();
        ui->m_startBtn->setEnabled(true);

        QFileInfo fi(path);
        ui->m_dropTitle->setText("Selected File:");
        ui->m_dropSubtitle->setText(fi.fileName());
        ui->m_dropZone->setStyleSheet(
            "QWidget#m_dropZone {"
            "  background-color: #1e1e1e;"
            "  border: 2px solid rgba(74,158,255,0.4);"
            "  border-radius: 4px;"
            "}"
        );
    }
}

void WizardMode::onStartClicked() {
    if (m_selectedFile.isEmpty()) return;

    ui->m_pages->setCurrentIndex(1);
    ui->m_progressBar->setValue(0);
    ui->m_percentLabel->setText("0%");
    ui->m_taskStatusLabel->setText("Separating Audio...");
    ui->m_statusLabel->setText("Extracting vocals and instruments");

    // Initialize project
    m_project = new Project();
    m_project->setProjectName(QFileInfo(m_selectedFile).baseName());
    m_project->sourceFile = m_selectedFile;
    m_project->settings.uvrModel = ui->m_modelCombo->currentText();
    m_project->settings.useGpu = true; 
    
    QString langText = ui->m_langCombo->currentText();
    if (langText.contains("(")) {
        m_project->settings.transcriptionLang = langText.mid(langText.indexOf("(") + 1, 2);
    } else {
        m_project->settings.transcriptionLang = "auto";
    }

    // Start separation
    auto* worker = new VocalSeparatorWorker();
    QThread* thread = new QThread(this);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [this, worker, thread]() {
        worker->startSeparation(m_selectedFile, m_project->settings.uvrModel, "output");
    });
    connect(worker, &VocalSeparatorWorker::progress, this, [this](int p, const QString& msg){
        ui->m_progressBar->setValue(p);
        ui->m_percentLabel->setText(QString::number(p) + "%");
    });
    connect(worker, &VocalSeparatorWorker::separationComplete, this, &WizardMode::onSeparationFinished);
    connect(worker, &VocalSeparatorWorker::error, this, &WizardMode::onSeparationError);
    connect(worker, &VocalSeparatorWorker::finished, thread, &QThread::quit);
    connect(worker, &VocalSeparatorWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void WizardMode::onSeparationFinished(const QString& inst, const QString& voc) {
    m_project->instrumentalPath = inst;
    m_project->vocalsPath = voc;
    m_project->originalAudioPath = m_selectedFile;

    if (!m_onlineLyrics.isEmpty()) {
        m_project->rawLyrics = m_onlineLyrics;
        emit projectReady(m_project);
        return;
    }

    if (ui->m_transcribeCheck->isChecked()) {
        ui->m_taskStatusLabel->setText("Generating Subtitles...");
        ui->m_statusLabel->setText("Running AI Transcription (WhisperX)");
        ui->m_progressBar->setValue(0);
        ui->m_percentLabel->setText("0%");

        auto* tw = new TranscriptionWorker();
        QThread* thread = new QThread(this);
        tw->moveToThread(thread);

        connect(thread, &QThread::started, tw, [this, tw, voc]() {
            tw->startTranscription(voc, 
                                 m_config ? m_config->get<QString>("ai.whisper_model", "base") : "base",
                                 m_project->settings.transcriptionLang,
                                 TranscriptionEngine::WhisperX);
        });
        connect(tw, &TranscriptionWorker::progressUpdated, this, [this](const QString& msg){
             ui->m_statusLabel->setText(msg);
        });
        connect(tw, &TranscriptionWorker::transcriptionComplete, this, &WizardMode::onTranscriptionFinished);
        connect(tw, &TranscriptionWorker::error, this, &WizardMode::onTranscriptionError);
        connect(tw, &TranscriptionWorker::finished, thread, &QThread::quit);
        connect(tw, &TranscriptionWorker::finished, tw, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
    } else {
        emit projectReady(m_project);
    }
}

void WizardMode::onSeparationError(const QString& err) {
    QMessageBox::critical(this, "Separation Error", err);
    ui->m_pages->setCurrentIndex(0);
}

void WizardMode::onTranscriptionFinished(const QString& json) {
    m_project->transcriptionJson = json;
    emit projectReady(m_project);
}

void WizardMode::onTranscriptionError(const QString& err) {
    QMessageBox::warning(this, "Transcription Error", "Transcription failed, but project will open.\n" + err);
    emit projectReady(m_project);
}

} // namespace ncktv
