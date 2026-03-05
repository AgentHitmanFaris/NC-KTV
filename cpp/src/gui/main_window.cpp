/*
 * NC-KTV GUI — Main Window Implementation
 * Port of main_window.py (360 lines)
 */

#include "main_window.h"

#include <QApplication>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QVBoxLayout>

#include "audio/ffmpeg_utils.h"
#include "system/gpu_detector.h"
#include "wizard/wizard_mode.h"
#include "editor/editor_mode.h"

namespace ncktv {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_config("config.yaml")
{
    setWindowTitle("NC-KTV — Professional Music Video Karaoke Maker");
    setMinimumSize(1280, 720);
    resize(1600, 900);

    // Initialize managers
    m_pluginManager = new PluginManager(this);
    m_themeManager  = new ThemeManager(this);

    createMenuBar();
    createStatusBar();
    checkPrerequisites();
    loadMode();

    // Load plugins
    m_pluginManager->loadAllPlugins();
}

// ─── Menu Bar ────────────────────────────────────────────────────────────────

void MainWindow::createMenuBar() {
    // ── File ─────────────────────────────────────────────────────────────
    QMenu* fileMenu = menuBar()->addMenu("&File");

    QAction* newAction = fileMenu->addAction("&New Project");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newProject);

    QAction* openAction = fileMenu->addAction("&Open Project...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openProject);

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    // ── Edit ─────────────────────────────────────────────────────────────
    QMenu* editMenu = menuBar()->addMenu("&Edit");
    QAction* prefsAction = editMenu->addAction("&Preferences...");
    connect(prefsAction, &QAction::triggered, this, &MainWindow::showPreferences);

    // ── Tools ────────────────────────────────────────────────────────────
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");

    QAction* themeAction = toolsMenu->addAction("&Theme Manager...");
    connect(themeAction, &QAction::triggered, this, &MainWindow::showThemeManager);

    QAction* pluginAction = toolsMenu->addAction("&Plugin Manager...");
    connect(pluginAction, &QAction::triggered, this, &MainWindow::showPluginManager);

    toolsMenu->addSeparator();

    QAction* wizardAction = toolsMenu->addAction("Switch to &Wizard Mode");
    connect(wizardAction, &QAction::triggered, this, [this]() { switchMode("wizard"); });

    QAction* editorAction = toolsMenu->addAction("Switch to &Editor Mode");
    connect(editorAction, &QAction::triggered, this, [this]() { switchMode("editor"); });

    // ── Help ─────────────────────────────────────────────────────────────
    QMenu* helpMenu = menuBar()->addMenu("&Help");

    QAction* shortcutsAction = helpMenu->addAction("&Keyboard Shortcuts");
    shortcutsAction->setShortcut(QKeySequence(Qt::Key_F1));
    connect(shortcutsAction, &QAction::triggered, this, &MainWindow::showShortcuts);

    helpMenu->addSeparator();
    QAction* aboutAction = helpMenu->addAction("&About NC-KTV");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

// ─── Status Bar ──────────────────────────────────────────────────────────────

void MainWindow::createStatusBar() {
    m_statusGpu  = new QLabel(this);
    m_statusMode = new QLabel(this);

    statusBar()->addPermanentWidget(m_statusMode);
    statusBar()->addPermanentWidget(m_statusGpu);

    // Detect GPU
    if (GPUDetector::isCudaAvailable()) {
        QString gpuName = GPUDetector::getGpuName();
        m_statusGpu->setText("🟢 GPU: " + gpuName);
    } else {
        m_statusGpu->setText("🔴 GPU: Not Available");
    }
}

// ─── Prerequisites ───────────────────────────────────────────────────────────

void MainWindow::checkPrerequisites() {
    if (!checkFfmpeg()) {
        QMessageBox::warning(this, "FFmpeg Not Found",
            "FFmpeg is required for NC-KTV.\n"
            "Please install FFmpeg and ensure it's in your PATH.");
    }
}

// ─── Mode Switching ──────────────────────────────────────────────────────────

void MainWindow::loadMode() {
    QString mode = m_config.get<QString>("gui.start_mode", "wizard");
    switchMode(mode);
}

void MainWindow::switchMode(const QString& mode, Project* project) {
    if (m_centralWidget) {
        QWidget* oldWidget = takeCentralWidget();
        if (oldWidget) {
            oldWidget->deleteLater();
        }
        m_centralWidget = nullptr;
    }

    m_currentMode = mode;
    m_statusMode->setText("Mode: " + mode.toUpper());

    if (mode == "wizard") {
        auto* wizard = new WizardMode(this);
        connect(wizard, &WizardMode::projectReady, this, [this](Project* project) {
            m_currentProject = project;
            switchMode("editor", project);
        });
        m_centralWidget = wizard;
    } else {
        auto* editor = new EditorMode(project ? project : m_currentProject, this);
        m_centralWidget = editor;
    }

    setCentralWidget(m_centralWidget);
}

// ─── Slots ───────────────────────────────────────────────────────────────────

void MainWindow::newProject() {
    if (!checkUnsavedChanges()) return;
    switchMode("wizard");
}

void MainWindow::openProject() {
    if (!checkUnsavedChanges()) return;

    QString filePath = QFileDialog::getOpenFileName(
        this, "Open Project", QString(),
        "NC-KTV Projects (*.nctv);;All Files (*)");

    if (filePath.isEmpty()) return;

    try {
        Project proj = Project::load(filePath);
        m_currentProject = new Project(std::move(proj));
        switchMode("editor", m_currentProject);
        statusBar()->showMessage("Project loaded: " + filePath, 5000);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", "Failed to open project:\n" + QString(e.what()));
    }
}

void MainWindow::showPreferences() {
    // TODO: PreferencesDialog
    QMessageBox::information(this, "Preferences", "Preferences dialog coming soon.");
}

void MainWindow::showThemeManager() {
    // TODO: ThemeManagerDialog
    QMessageBox::information(this, "Themes", "Theme manager coming soon.");
}

void MainWindow::showPluginManager() {
    // TODO: PluginManagerDialog
    QMessageBox::information(this, "Plugins", "Plugin manager coming soon.");
}

void MainWindow::showShortcuts() {
    QMessageBox::information(this, "Keyboard Shortcuts",
        "Space — Set line start time\n"
        "↑/↓ — Navigate lines\n"
        "J/K/L — Rewind/Pause/Forward\n"
        "Ctrl+S — Save project\n"
        "Ctrl+Z — Undo\n"
        "Ctrl+E — Export video\n"
        "F1 — Help");
}

void MainWindow::showAbout() {
    QMessageBox::about(this, "About NC-KTV",
        "<h2>NC-KTV</h2>"
        "<p>Professional Music Video Karaoke Maker</p>"
        "<p>Version 1.0.0 (C++ Native)</p>"
        "<p>© 2025-2026 NC-KTV Team</p>"
        "<p>Built with Qt6 + ONNX Runtime</p>");
}

bool MainWindow::checkUnsavedChanges() {
    if (!m_currentProject || !m_currentProject->isDirty)
        return true;

    auto result = QMessageBox::question(this, "Unsaved Changes",
        "You have unsaved changes. Do you want to save before continuing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (result == QMessageBox::Cancel) return false;
    if (result == QMessageBox::Save) {
        // TODO: Save current project
    }
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (checkUnsavedChanges()) {
        event->accept();
    } else {
        event->ignore();
    }
}

} // namespace ncktv
