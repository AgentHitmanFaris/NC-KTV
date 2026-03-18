/*
 * NC-KTV GUI — Export Dialog Implementation
 */

#include "export_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>

namespace ncktv {

ExportDialog::ExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Export");
    setMinimumSize(540, 480);
    setModal(true);
    setupUi();
    applyTheme();
}

void ExportDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(24, 20, 24, 20);

    // ── Title ────────────────────────────────────────────────────────────
    auto* titleLabel = new QLabel("🎬  Export Project", this);
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#e0e0e0; border:none;");
    mainLayout->addWidget(titleLabel);

    // ── Preset Group ─────────────────────────────────────────────────────
    auto* presetGroup = new QGroupBox("Export Preset", this);
    auto* presetLayout = new QFormLayout(presetGroup);
    presetLayout->setSpacing(10);
    presetLayout->setContentsMargins(16, 20, 16, 12);

    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItems({
        "Karaoke Video (with video + lyrics)",
        "Lyrics Video (lyrics over black)",
        "Karaoke Audio Only (no video)",
        "Subtitles Only (ASS/SRT)"
    });
    presetLayout->addRow("Preset:", m_presetCombo);
    mainLayout->addWidget(presetGroup);

    // ── Format & Quality Group ───────────────────────────────────────────
    auto* formatGroup = new QGroupBox("Format && Quality", this);
    auto* formatForm = new QFormLayout(formatGroup);
    formatForm->setSpacing(10);
    formatForm->setContentsMargins(16, 20, 16, 12);

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItems({"MP4 (H.264)", "MKV (H.265)", "WebM (VP9)", "MP3 (Audio Only)", "ASS Subtitles", "SRT Subtitles"});
    formatForm->addRow("Output Format:", m_formatCombo);

    m_resolutionCombo = new QComboBox(this);
    m_resolutionCombo->addItems({"1920×1080 (Full HD)", "1280×720 (HD)", "3840×2160 (4K)", "Custom..."});
    formatForm->addRow("Resolution:", m_resolutionCombo);

    m_audioSourceCombo = new QComboBox(this);
    m_audioSourceCombo->addItems({"Instrumental", "Original (Mixed)", "Vocals Only"});
    formatForm->addRow("Audio Source:", m_audioSourceCombo);

    mainLayout->addWidget(formatGroup);

    // ── Lyrics Options Group ─────────────────────────────────────────────
    auto* lyricsGroup = new QGroupBox("Lyrics Options", this);
    auto* lyricsForm = new QFormLayout(lyricsGroup);
    lyricsForm->setSpacing(10);
    lyricsForm->setContentsMargins(16, 20, 16, 12);

    m_lyricsStyleCombo = new QComboBox(this);
    m_lyricsStyleCombo->addItems({
        "Neon Gold",
        "Classic Blue",
        "Modern Clean",
        "Fire Red",
        "Match Preview"
    });
    lyricsForm->addRow("Lyrics Style:", m_lyricsStyleCombo);

    m_burnSubsCheck = new QCheckBox("Burn subtitles into video (hardcoded)", this);
    m_burnSubsCheck->setChecked(true);
    lyricsForm->addRow("", m_burnSubsCheck);

    mainLayout->addWidget(lyricsGroup);

    // ── Output Path ──────────────────────────────────────────────────────
    auto* outputGroup = new QGroupBox("Output", this);
    auto* outputLayout = new QHBoxLayout(outputGroup);
    outputLayout->setContentsMargins(16, 20, 16, 12);

    m_outputEdit = new QLineEdit(this);
    m_outputEdit->setPlaceholderText("Select output location...");
    m_outputEdit->setReadOnly(true);
    m_browseBtn = new QPushButton("Browse...", this);
    m_browseBtn->setFixedWidth(90);
    outputLayout->addWidget(m_outputEdit, 1);
    outputLayout->addWidget(m_browseBtn);
    mainLayout->addWidget(outputGroup);

    // ── Buttons ──────────────────────────────────────────────────────────
    mainLayout->addStretch();

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setFixedWidth(100);
    buttonLayout->addWidget(m_cancelBtn);

    m_exportBtn = new QPushButton("Export", this);
    m_exportBtn->setFixedWidth(130);
    m_exportBtn->setDefault(true);
    m_exportBtn->setStyleSheet("background:#16a34a; color:white; font-weight:bold; border:none; padding:8px 16px; border-radius:4px;");
    buttonLayout->addWidget(m_exportBtn);

    mainLayout->addLayout(buttonLayout);

    // ── Connections ──────────────────────────────────────────────────────
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportDialog::browseOutput);
    connect(m_exportBtn, &QPushButton::clicked, this, &ExportDialog::onAccepted);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportDialog::onPresetChanged);
}

void ExportDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QGroupBox { border:1px solid #3f3f46; border-radius:6px; padding-top:16px; margin-top:8px; font-weight:bold; color:#ccc; }
        QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 6px; }
        QLineEdit { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:6px 8px; color:#fff; }
        QComboBox { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:6px 8px; color:#fff; }
        QComboBox::drop-down { border:none; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QCheckBox { color:#ccc; }
        QLabel { color:#ccc; border:none; }
    )");
}

void ExportDialog::browseOutput() {
    QString fmt = m_formatCombo->currentText();
    QString ext = fmt.contains("ASS")  ? "ass"
                : fmt.contains("SRT")  ? "srt"
                : fmt.contains("MKV")  ? "mkv"
                : fmt.contains("WebM") ? "webm"
                : fmt.contains("MP3")  ? "mp3"
                : "mp4";

    QString filter = QString("Output File (*.%1);;All Files (*)").arg(ext);
    QString path = QFileDialog::getSaveFileName(this, "Save Output As", "", filter);
    if (!path.isEmpty()) {
        m_outputEdit->setText(path);
    }
}

void ExportDialog::onPresetChanged(int index) {
    // Adjust available options based on preset
    bool isVideoExport = (index <= 1);
    bool isSubOnly     = (index == 3);
    bool isAudioOnly   = (index == 2); // "Karaoke Audio Only"

    m_resolutionCombo->setEnabled(isVideoExport);
    m_burnSubsCheck->setEnabled(isVideoExport);
    m_audioSourceCombo->setEnabled(!isSubOnly);

    if (isSubOnly) {
        m_formatCombo->setCurrentText("ASS Subtitles");
    } else if (isAudioOnly) {
        m_formatCombo->setCurrentText("MP3 (Audio Only)");
    }
}

void ExportDialog::onAccepted() {
    if (m_outputEdit->text().isEmpty()) {
        QMessageBox::warning(this, "No Output", "Please select an output file path.");
        return;
    }
    accept();
}

QString ExportDialog::outputPath()   const { return m_outputEdit->text(); }
QString ExportDialog::format()       const { return m_formatCombo->currentText(); }
QString ExportDialog::preset()       const { return m_presetCombo->currentText(); }
QString ExportDialog::lyricsStyle()  const { return m_lyricsStyleCombo->currentText(); }
QString ExportDialog::audioSource()  const { return m_audioSourceCombo->currentText(); }

int ExportDialog::videoWidth() const {
    QString res = m_resolutionCombo->currentText();
    if (res.contains("3840")) return 3840;
    if (res.contains("1280")) return 1280;
    return 1920;
}

int ExportDialog::videoHeight() const {
    QString res = m_resolutionCombo->currentText();
    if (res.contains("2160")) return 2160;
    if (res.contains("720"))  return 720;
    return 1080;
}

bool ExportDialog::burnSubtitles() const { return m_burnSubsCheck->isChecked(); }

} // namespace ncktv
