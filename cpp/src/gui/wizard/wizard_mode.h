#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include "../../core/config/config_manager.h"

#include "project/project.h"
#include "workers/vocal_separator_worker.h"
#include "workers/transcription_worker.h"

namespace ncktv {

class WizardMode : public QWidget {
    Q_OBJECT

public:
    explicit WizardMode(ConfigManager* config, QWidget* parent = nullptr);
    void reset();

signals:
    void projectReady(Project* project);
    void requestOpen();
    void requestNew();
    void requestPreferences();

private slots:
    void onBrowseFile();
    void onStartClicked();
    void onSeparationFinished(const QString& instrumentalPath, const QString& vocalsPath);
    void onSeparationError(const QString& error);
    void onTranscriptionFinished(const QString& resultJson);
    void onTranscriptionError(const QString& error);

private:
    ConfigManager* m_config = nullptr;
    void setupUi();
    void createWelcomePage();
    void createProcessingPage();

    QStackedWidget* m_pages;
    
    // Page 1: Welcome / Selection (Pro Dashboard)
    QLineEdit* m_filePathEdit;
    QPushButton* m_startBtn;
    QCheckBox* m_transcribeCheck;
    QComboBox* m_langCombo;
    QComboBox* m_modelCombo = nullptr;

    // Pro Dashboard widgets
    QLabel* m_heroTitle;
    QWidget* m_dropZone;
    QLabel* m_dropTitle;
    QLabel* m_dropSubtitle;

    // Page 2: Processing (Pro Processor)
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QLabel* m_percentLabel;
    QLabel* m_taskStatusLabel;

    QString m_selectedFile;
    QString m_onlineLyrics;
    Project* m_project = nullptr;
};

} // namespace ncktv
