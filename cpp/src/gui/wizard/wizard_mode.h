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

#include "project/project.h"
#include "workers/vocal_separator_worker.h"
#include "workers/transcription_worker.h"

namespace ncktv {

class WizardMode : public QWidget {
    Q_OBJECT

public:
    explicit WizardMode(QWidget* parent = nullptr);

signals:
    void projectReady(Project* project);

private slots:
    void onBrowseFile();
    void onStartClicked();
    void onSeparationFinished(const QString& instrumentalPath, const QString& vocalsPath);
    void onSeparationError(const QString& error);
    void onTranscriptionFinished(const QString& resultJson);
    void onTranscriptionError(const QString& error);

private:
    void setupUi();
    void createWelcomePage();
    void createProcessingPage();

    QStackedWidget* m_pages;
    
    // Page 1: Welcome / Selection
    QLineEdit* m_filePathEdit;
    QPushButton* m_startBtn;
    QCheckBox* m_transcribeCheck;
    QComboBox* m_langCombo;
    QComboBox* m_modelCombo = nullptr;   // UVR model selector

    // Page 2: Processing
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;

    QString m_selectedFile;
    Project* m_project = nullptr;
};

} // namespace ncktv
