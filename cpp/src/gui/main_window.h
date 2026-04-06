#pragma once
/*
 * NC-KTV GUI — Main Window
 * Phase 5 100% Native C++ UI implementation.
 */

#include <QMainWindow>
#include <memory>
#include "../core/project/ncktv_project.hpp"

class QStackedWidget;
class QMenu;

namespace ncktv {

class VocalSeparatorWorker;
class TranscriptionWorker;
class WizardMode;
class EditorMode;
class Project;
class ConfigManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void setEditorMenusEnabled(bool enabled);

private slots:
    void onActionNewProject();
    void onActionSaveProject();
    void onActionSaveProjectAs();
    void onActionOpenProject();
    void onWorkerProgress(int percent, const QString& message);
    void onWorkerError(const QString& errorMsg);
    void onVocalSeparationFinished(const QString& instPath, const QString& vocPath);
    void onTranscriptionFinished(const QString& resultJson);
    void onProjectReady(Project* project);

private:
    void setupApplicationUI();
    void setupWorkers();
    void saveProject(const QString& fileName);

    QStackedWidget* mainStack_ = nullptr;
    WizardMode* wizardMode_ = nullptr;
    EditorMode* editorMode_ = nullptr;
    
    std::shared_ptr<core::Project> activeProject_;
    std::shared_ptr<Project> legacyProject_; // For compatibility with WizardMode
    
    ConfigManager* m_config = nullptr;

    // Editor-only menus (disabled in wizard mode)
    QMenu* m_sequenceMenu = nullptr;
    QMenu* m_clipMenu = nullptr;
    QMenu* m_windowMenu = nullptr;
    
    // Workers and Threads for Multi-threaded AI
    VocalSeparatorWorker* vocalWorker_ = nullptr;
    QThread* vocalThread_ = nullptr;
    
    TranscriptionWorker* transcriptionWorker_ = nullptr;
    QThread* transcriptionThread_ = nullptr;
};


} // namespace ncktv
