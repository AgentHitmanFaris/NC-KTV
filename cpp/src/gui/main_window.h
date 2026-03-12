#pragma once
/*
 * NC-KTV GUI — Main Window
 * Phase 5 100% Native C++ UI implementation.
 */

#include <QMainWindow>
#include <memory>
#include "../core/project/ncktv_project.hpp"

class QStackedWidget;

namespace ncktv {

class VocalSeparatorWorker;
class TranscriptionWorker;
class WizardMode;
class EditorMode;
class Project;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onActionNewProject();
    void onWorkerProgress(int percent, const QString& message);
    void onWorkerError(const QString& errorMsg);
    void onVocalSeparationFinished(const QString& instPath, const QString& vocPath);
    void onTranscriptionFinished(const QString& resultJson);
    void onProjectReady(Project* project);

private:
    void setupApplicationUI();
    void setupWorkers();

    QStackedWidget* mainStack_ = nullptr;
    WizardMode* wizardMode_ = nullptr;
    EditorMode* editorMode_ = nullptr;
    
    std::shared_ptr<core::Project> activeProject_;
    std::shared_ptr<Project> legacyProject_; // For compatibility with WizardMode
    
    // Workers and Threads for Multi-threaded AI
    VocalSeparatorWorker* vocalWorker_ = nullptr;
    QThread* vocalThread_ = nullptr;
    
    TranscriptionWorker* transcriptionWorker_ = nullptr;
    QThread* transcriptionThread_ = nullptr;
};


} // namespace ncktv
