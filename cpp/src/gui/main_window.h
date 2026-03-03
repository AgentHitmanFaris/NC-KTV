#pragma once
/*
 * NC-KTV GUI — Main Window
 * Port of main_window.py — Mode switching between Wizard and Editor
 */

#include <QMainWindow>
#include <QStackedWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>

#include "config/config_manager.h"
#include "plugins/plugin_manager.h"
#include "themes/theme_manager.h"
#include "project/project.h"

namespace ncktv {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newProject();
    void openProject();
    void showPreferences();
    void showThemeManager();
    void showPluginManager();
    void showAbout();
    void showShortcuts();

private:
    void createMenuBar();
    void createStatusBar();
    void checkPrerequisites();
    void switchMode(const QString& mode, Project* project = nullptr);
    void loadMode();
    bool checkUnsavedChanges();

    ConfigManager   m_config;
    PluginManager*  m_pluginManager;
    ThemeManager*   m_themeManager;

    QWidget* m_centralWidget = nullptr;
    Project* m_currentProject = nullptr;
    QString  m_currentMode = "wizard";

    QLabel*  m_statusGpu = nullptr;
    QLabel*  m_statusMode = nullptr;
};

} // namespace ncktv
