/*
 * NC-KTV GUI — Preferences Dialog Implementation
 */

#include "preferences_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QCoreApplication>

namespace ncktv {

PreferencesDialog::PreferencesDialog(ConfigManager* config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle("Preferences");
    setMinimumSize(560, 460);
    setModal(true);
    setupUi();
    applyTheme();
}

void PreferencesDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(20, 16, 20, 16);

    auto* titleLabel = new QLabel("⚙  Preferences", this);
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#e0e0e0; border:none;");
    mainLayout->addWidget(titleLabel);

    // ── Tab Widget ───────────────────────────────────────────────────────
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createGeneralTab(), "General");
    m_tabs->addTab(createAudioTab(), "Audio");
    m_tabs->addTab(createAiTab(), "AI Models");
    m_tabs->addTab(createPathsTab(), "Paths");
    mainLayout->addWidget(m_tabs, 1);

    // ── Buttons ──────────────────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setFixedWidth(100);
    btnLayout->addWidget(m_cancelBtn);
    m_okBtn = new QPushButton("Save", this);
    m_okBtn->setFixedWidth(100);
    m_okBtn->setDefault(true);
    m_okBtn->setStyleSheet("background:#4f46e5; color:white; font-weight:bold; border:none; padding:8px 16px; border-radius:4px;");
    btnLayout->addWidget(m_okBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_okBtn, &QPushButton::clicked, this, &PreferencesDialog::onAccepted);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QWidget* PreferencesDialog::createGeneralTab() {
    auto* widget = new QWidget(this);
    auto* form = new QFormLayout(widget);
    form->setSpacing(12);
    form->setContentsMargins(16, 16, 16, 16);

    m_startModeCombo = new QComboBox(this);
    m_startModeCombo->addItems({"Wizard", "Editor"});
    form->addRow("Start Mode:", m_startModeCombo);

    m_graphicsApiCombo = new QComboBox(this);
    m_graphicsApiCombo->addItems({"Default", "Vulkan (High Perf)", "DirectX 11", "OpenGL", "Software"});
    QString currentApi = m_config ? m_config->get<QString>("gui.graphics_api", "Default") : "Default";
    m_graphicsApiCombo->setCurrentText(currentApi);
    form->addRow("Graphics Engine:", m_graphicsApiCombo);
    
    auto* graphicsNote = new QLabel("Note: Changing the graphics engine requires an application restart.", this);
    graphicsNote->setStyleSheet("color:#8a8a8a; font-size:10px; font-style:italic; border:none;");
    form->addRow("", graphicsNote);

    m_autoSaveCheck = new QCheckBox("Enable auto-save", this);
    m_autoSaveCheck->setChecked(true);
    form->addRow("", m_autoSaveCheck);

    m_autoSaveInterval = new QSpinBox(this);
    m_autoSaveInterval->setRange(30, 600);
    m_autoSaveInterval->setValue(120);
    m_autoSaveInterval->setSuffix(" seconds");
    form->addRow("Auto-save interval:", m_autoSaveInterval);

    return widget;
}

QWidget* PreferencesDialog::createAudioTab() {
    auto* widget = new QWidget(this);
    auto* form = new QFormLayout(widget);
    form->setSpacing(12);
    form->setContentsMargins(16, 16, 16, 16);

    m_sampleRateCombo = new QComboBox(this);
    m_sampleRateCombo->addItems({"44100 Hz", "48000 Hz", "96000 Hz"});
    m_sampleRateCombo->setCurrentIndex(1);
    form->addRow("Sample Rate:", m_sampleRateCombo);

    m_gpuCheck = new QCheckBox("Use GPU for vocal separation (CUDA/DirectML)", this);
    m_gpuCheck->setChecked(true);
    form->addRow("", m_gpuCheck);

    return widget;
}

QWidget* PreferencesDialog::createAiTab() {
    auto* widget = new QWidget(this);
    auto* form = new QFormLayout(widget);
    form->setSpacing(12);
    form->setContentsMargins(16, 16, 16, 16);

    // ── Transcription Engine ────────────────────────────────────────────
    m_transcriptionEngineCombo = new QComboBox(this);
    m_transcriptionEngineCombo->addItems({"WhisperX (Recommended)", "Whisper (Legacy)"});
    QString currentEngine = m_config ? m_config->get<QString>("ai.transcription_engine", "whisperx") : "whisperx";
    if (currentEngine == "whisper") {
        m_transcriptionEngineCombo->setCurrentIndex(1);
    } else {
        m_transcriptionEngineCombo->setCurrentIndex(0);
    }
    form->addRow("Transcription Engine:", m_transcriptionEngineCombo);

    auto* engineNote = new QLabel(
        "<b>WhisperX</b>: Uses Wav2Vec2 forced alignment for ±30ms word precision. "
        "Ideal for karaoke wipe effects.<br>"
        "<b>Whisper</b>: Legacy mode with attention-based timestamps (~±500ms).",
        this);
    engineNote->setWordWrap(true);
    engineNote->setStyleSheet("color:#64748b; font-size:10px; border:none; margin-bottom:6px;");
    form->addRow("", engineNote);

    // ── Whisper Model ───────────────────────────────────────────────────
    m_defaultModelCombo = new QComboBox(this);
    m_defaultModelCombo->addItems({"base", "small", "medium", "large-v2", "large-v3", "turbo"});
    QString currentWhisper = m_config ? m_config->get<QString>("ai.whisper_model", "medium") : "medium";
    m_defaultModelCombo->setCurrentText(currentWhisper);
    form->addRow("Default Whisper Model:", m_defaultModelCombo);

    // ── UVR Model ───────────────────────────────────────────────────────
    m_defaultUvrModelCombo = new QComboBox(this);
    QStringList uvrPaths = {
        QCoreApplication::applicationDirPath() + "/models/uvr",
        QDir::currentPath() + "/models/uvr"
    };
    QStringList uvrModels;
    for (const auto& path : uvrPaths) {
        QDir dir(path);
        if (dir.exists()) {
            uvrModels << dir.entryList({"*.onnx", "*.pth"}, QDir::Files);
        }
    }
    uvrModels.removeDuplicates();
    if (uvrModels.isEmpty()) uvrModels << "UVR_MDXNET_KARA_2.onnx";
    m_defaultUvrModelCombo->addItems(uvrModels);
    
    QString currentUvr = m_config ? m_config->get<QString>("ai.uvr_model", "UVR_MDXNET_KARA_2.onnx") : "UVR_MDXNET_KARA_2.onnx";
    m_defaultUvrModelCombo->setCurrentText(currentUvr);
    form->addRow("Default UVR Model:", m_defaultUvrModelCombo);

    // ── Language ────────────────────────────────────────────────────────
    m_defaultLanguageCombo = new QComboBox(this);
    m_defaultLanguageCombo->addItems({"Auto", "en", "ms", "id", "ja", "ko", "zh"});
    QString currentLang = m_config ? m_config->get<QString>("ai.language", "Auto") : "Auto";
    m_defaultLanguageCombo->setCurrentText(currentLang);
    form->addRow("Default Language:", m_defaultLanguageCombo);

    auto* noteLabel = new QLabel("Larger models produce better results but require more RAM and processing time.", this);
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet("color:#888; font-size:11px; border:none;");
    form->addRow("", noteLabel);

    return widget;
}

QWidget* PreferencesDialog::createPathsTab() {
    auto* widget = new QWidget(this);
    auto* form = new QFormLayout(widget);
    form->setSpacing(12);
    form->setContentsMargins(16, 16, 16, 16);

    auto* ffmpegLayout = new QHBoxLayout();
    m_ffmpegPathEdit = new QLineEdit(this);
    m_ffmpegPathEdit->setPlaceholderText("Auto-detected");
    auto* ffmpegBtn = new QPushButton("Browse...", this);
    ffmpegBtn->setFixedWidth(80);
    ffmpegLayout->addWidget(m_ffmpegPathEdit, 1);
    ffmpegLayout->addWidget(ffmpegBtn);
    form->addRow("FFmpeg Path:", ffmpegLayout);
    connect(ffmpegBtn, &QPushButton::clicked, this, &PreferencesDialog::browseFfmpegPath);

    auto* modelLayout = new QHBoxLayout();
    m_modelPathEdit = new QLineEdit(this);
    m_modelPathEdit->setPlaceholderText("models/whisper");
    auto* modelBtn = new QPushButton("Browse...", this);
    modelBtn->setFixedWidth(80);
    modelLayout->addWidget(m_modelPathEdit, 1);
    modelLayout->addWidget(modelBtn);
    form->addRow("Model Directory:", modelLayout);
    connect(modelBtn, &QPushButton::clicked, this, &PreferencesDialog::browseModelPath);

    return widget;
}

void PreferencesDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QTabWidget::pane { border:1px solid #3f3f46; border-radius:4px; background:#1e1e1e; }
        QTabBar::tab { background:#252526; color:#ccc; padding:8px 16px; border:1px solid #3f3f46; border-bottom:none; border-top-left-radius:4px; border-top-right-radius:4px; }
        QTabBar::tab:selected { background:#1e1e1e; color:#fff; }
        QTabBar::tab:hover { background:#333; }
        QLineEdit, QComboBox, QSpinBox { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:6px 8px; color:#fff; }
        QComboBox::drop-down { border:none; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QCheckBox { color:#ccc; }
        QLabel { color:#ccc; border:none; }
    )");
}

void PreferencesDialog::browseFfmpegPath() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select FFmpeg Directory");
    if (!dir.isEmpty()) m_ffmpegPathEdit->setText(dir);
}

void PreferencesDialog::browseModelPath() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Model Directory");
    if (!dir.isEmpty()) m_modelPathEdit->setText(dir);
}

void PreferencesDialog::onAccepted() {
    if (m_config) {
        m_config->set<QString>("gui.start_mode", m_startModeCombo->currentText().toLower());
        m_config->set<QString>("gui.graphics_api", m_graphicsApiCombo->currentText());
        // Transcription engine: index 0 = whisperx, index 1 = whisper
        m_config->set<QString>("ai.transcription_engine",
            m_transcriptionEngineCombo->currentIndex() == 0 ? "whisperx" : "whisper");
        m_config->set<QString>("ai.whisper_model", m_defaultModelCombo->currentText());
        m_config->set<QString>("ai.uvr_model", m_defaultUvrModelCombo->currentText());
        m_config->set<QString>("ai.language", m_defaultLanguageCombo->currentText());
        m_config->save();
    }
    accept();
}

} // namespace ncktv
