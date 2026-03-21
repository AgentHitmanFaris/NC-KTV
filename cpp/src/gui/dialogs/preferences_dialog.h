#pragma once
/*
 * NC-KTV GUI — Preferences Dialog
 * App settings: paths, GPU, model config, behaviour
 */

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include "config/config_manager.h"

namespace ncktv {

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(ConfigManager* config, QWidget* parent = nullptr);

private slots:
    void browseFfmpegPath();
    void browseModelPath();
    void onAccepted();

private:
    void setupUi();
    void applyTheme();
    QWidget* createGeneralTab();
    QWidget* createAudioTab();
    QWidget* createAiTab();
    QWidget* createPathsTab();

    ConfigManager* m_config = nullptr;
    QTabWidget*    m_tabs = nullptr;

    // General
    QComboBox* m_graphicsApiCombo = nullptr;
    QComboBox* m_startModeCombo = nullptr;
    QCheckBox* m_autoSaveCheck = nullptr;
    QSpinBox*  m_autoSaveInterval = nullptr;

    // Audio
    QComboBox* m_sampleRateCombo = nullptr;
    QCheckBox* m_gpuCheck = nullptr;

    // AI
    QComboBox* m_transcriptionEngineCombo = nullptr;
    QComboBox* m_defaultModelCombo = nullptr;
    QComboBox* m_defaultUvrModelCombo = nullptr;
    QComboBox* m_defaultLanguageCombo = nullptr;

    // Paths
    QLineEdit* m_ffmpegPathEdit = nullptr;
    QLineEdit* m_modelPathEdit = nullptr;

    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};

} // namespace ncktv
