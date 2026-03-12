/*
 * NC-KTV GUI — Video Options Dialog Implementation
 */

#include "video_options_dialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>

namespace ncktv {

VideoOptionsDialog::VideoOptionsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Video Options");
    setMinimumSize(460, 380);
    setModal(true);
    setupUi();
    applyTheme();
}

void VideoOptionsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(24, 20, 24, 20);

    auto* titleLabel = new QLabel("🎥  Video Options", this);
    titleLabel->setStyleSheet("font-size:18px; font-weight:bold; color:#e0e0e0; border:none;");
    mainLayout->addWidget(titleLabel);

    // ── Resolution Group ─────────────────────────────────────────────────
    auto* resGroup = new QGroupBox("Resolution", this);
    auto* resForm = new QFormLayout(resGroup);
    resForm->setSpacing(10);
    resForm->setContentsMargins(16, 20, 16, 12);

    m_resolutionCombo = new QComboBox(this);
    m_resolutionCombo->addItems({"1920×1080 (Full HD)", "1280×720 (HD)", "3840×2160 (4K)", "Custom"});
    resForm->addRow("Preset:", m_resolutionCombo);

    auto* customRes = new QHBoxLayout();
    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(320, 7680);
    m_widthSpin->setValue(1920);
    m_widthSpin->setEnabled(false);
    auto* xLabel = new QLabel("×", this);
    xLabel->setStyleSheet("border:none;");
    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(240, 4320);
    m_heightSpin->setValue(1080);
    m_heightSpin->setEnabled(false);
    customRes->addWidget(m_widthSpin);
    customRes->addWidget(xLabel);
    customRes->addWidget(m_heightSpin);
    customRes->addStretch();
    resForm->addRow("Custom:", customRes);

    mainLayout->addWidget(resGroup);

    // ── Encoding Group ───────────────────────────────────────────────────
    auto* encGroup = new QGroupBox("Encoding", this);
    auto* encForm = new QFormLayout(encGroup);
    encForm->setSpacing(10);
    encForm->setContentsMargins(16, 20, 16, 12);

    m_codecCombo = new QComboBox(this);
    m_codecCombo->addItems({"H.264 (libx264)", "H.265 (libx265)", "VP9", "AV1 (libaom)"});
    encForm->addRow("Codec:", m_codecCombo);

    m_fpsSpinBox = new QSpinBox(this);
    m_fpsSpinBox->setRange(15, 120);
    m_fpsSpinBox->setValue(30);
    m_fpsSpinBox->setSuffix(" fps");
    encForm->addRow("Frame Rate:", m_fpsSpinBox);

    auto* bitrateLayout = new QHBoxLayout();
    m_bitrateSlider = new QSlider(Qt::Horizontal, this);
    m_bitrateSlider->setRange(1, 50);
    m_bitrateSlider->setValue(8);
    m_bitrateLabel = new QLabel("8 Mbps", this);
    m_bitrateLabel->setFixedWidth(70);
    m_bitrateLabel->setStyleSheet("border:none;");
    bitrateLayout->addWidget(m_bitrateSlider, 1);
    bitrateLayout->addWidget(m_bitrateLabel);
    encForm->addRow("Bitrate:", bitrateLayout);

    m_hwAccelCheck = new QCheckBox("Use hardware encoding (NVENC/QSV)", this);
    m_hwAccelCheck->setChecked(true);
    encForm->addRow("", m_hwAccelCheck);

    mainLayout->addWidget(encGroup);

    // ── Buttons ──────────────────────────────────────────────────────────
    mainLayout->addStretch();
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setFixedWidth(100);
    btnLayout->addWidget(m_cancelBtn);
    m_okBtn = new QPushButton("OK", this);
    m_okBtn->setFixedWidth(100);
    m_okBtn->setDefault(true);
    m_okBtn->setStyleSheet("background:#4f46e5; color:white; font-weight:bold; border:none; padding:8px 16px; border-radius:4px;");
    btnLayout->addWidget(m_okBtn);
    mainLayout->addLayout(btnLayout);

    // ── Connections ──────────────────────────────────────────────────────
    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VideoOptionsDialog::onResolutionChanged);
    connect(m_bitrateSlider, &QSlider::valueChanged, this, [this](int val) {
        m_bitrateLabel->setText(QString("%1 Mbps").arg(val));
    });
}

void VideoOptionsDialog::applyTheme() {
    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QGroupBox { border:1px solid #3f3f46; border-radius:6px; padding-top:16px; margin-top:8px; font-weight:bold; color:#ccc; }
        QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 6px; }
        QComboBox, QSpinBox { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:6px 8px; color:#fff; }
        QComboBox::drop-down { border:none; }
        QSlider::groove:horizontal { background:#3f3f46; height:6px; border-radius:3px; }
        QSlider::handle:horizontal { background:#4f46e5; width:16px; margin:-5px 0; border-radius:8px; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QCheckBox { color:#ccc; }
        QLabel { color:#ccc; border:none; }
    )");
}

void VideoOptionsDialog::onResolutionChanged(int index) {
    bool isCustom = (index == 3);
    m_widthSpin->setEnabled(isCustom);
    m_heightSpin->setEnabled(isCustom);

    if (!isCustom) {
        switch (index) {
            case 0: m_widthSpin->setValue(1920); m_heightSpin->setValue(1080); break;
            case 1: m_widthSpin->setValue(1280); m_heightSpin->setValue(720);  break;
            case 2: m_widthSpin->setValue(3840); m_heightSpin->setValue(2160); break;
        }
    }
}

int     VideoOptionsDialog::width()       const { return m_widthSpin->value(); }
int     VideoOptionsDialog::height()      const { return m_heightSpin->value(); }
int     VideoOptionsDialog::fps()         const { return m_fpsSpinBox->value(); }
int     VideoOptionsDialog::bitrateMbps() const { return m_bitrateSlider->value(); }
QString VideoOptionsDialog::codec()       const { return m_codecCombo->currentText(); }
bool    VideoOptionsDialog::hwAccel()     const { return m_hwAccelCheck->isChecked(); }

} // namespace ncktv
