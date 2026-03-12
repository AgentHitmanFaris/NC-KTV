/*
 * NC-KTV GUI — Timing Calibration Implementation
 */

#include "timing_calibration.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

namespace ncktv {

TimingCalibration::TimingCalibration(QWidget* parent) : QWidget(parent) {
    setupUi();
    applyTheme();
}

void TimingCalibration::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    auto* titleLabel = new QLabel("🎯 Timing Calibration", this);
    titleLabel->setStyleSheet("font-size:14px; font-weight:bold; color:#ccc; border:none;");
    mainLayout->addWidget(titleLabel);

    // ── Master Offset ────────────────────────────────────────────────────
    auto* offsetGroup = new QGroupBox("Global Timing Offset", this);
    auto* offsetLayout = new QVBoxLayout(offsetGroup);
    offsetLayout->setContentsMargins(12, 16, 12, 10);

    m_offsetLabel = new QLabel("0.000 s", this);
    m_offsetLabel->setAlignment(Qt::AlignCenter);
    m_offsetLabel->setStyleSheet("font-size:24px; font-weight:bold; color:#22d3ee; border:none;");
    offsetLayout->addWidget(m_offsetLabel);

    m_offsetSlider = new QSlider(Qt::Horizontal, this);
    m_offsetSlider->setRange(-5000, 5000); // ±5.000 seconds in milliseconds
    m_offsetSlider->setValue(0);
    m_offsetSlider->setTickInterval(1000);
    m_offsetSlider->setTickPosition(QSlider::TicksBelow);
    offsetLayout->addWidget(m_offsetSlider);

    auto* spinLayout = new QHBoxLayout();
    m_offsetSpin = new QDoubleSpinBox(this);
    m_offsetSpin->setRange(-5.0, 5.0);
    m_offsetSpin->setDecimals(3);
    m_offsetSpin->setSingleStep(0.001);
    m_offsetSpin->setSuffix(" s");
    spinLayout->addWidget(m_offsetSpin);

    m_resetBtn = new QPushButton("Reset", this);
    m_resetBtn->setFixedWidth(70);
    spinLayout->addWidget(m_resetBtn);
    offsetLayout->addLayout(spinLayout);

    mainLayout->addWidget(offsetGroup);

    // ── Per-Source Latency ────────────────────────────────────────────────
    auto* latencyGroup = new QGroupBox("Latency Compensation", this);
    auto* latencyForm = new QFormLayout(latencyGroup);
    latencyForm->setSpacing(8);
    latencyForm->setContentsMargins(12, 16, 12, 10);

    m_uvrLatency = new QDoubleSpinBox(this);
    m_uvrLatency->setRange(-2.0, 2.0);
    m_uvrLatency->setDecimals(3);
    m_uvrLatency->setSuffix(" s");
    latencyForm->addRow("UVR Separation:", m_uvrLatency);

    m_transcriptionLatency = new QDoubleSpinBox(this);
    m_transcriptionLatency->setRange(-2.0, 2.0);
    m_transcriptionLatency->setDecimals(3);
    m_transcriptionLatency->setSuffix(" s");
    latencyForm->addRow("Transcription:", m_transcriptionLatency);

    m_playbackLatency = new QDoubleSpinBox(this);
    m_playbackLatency->setRange(-2.0, 2.0);
    m_playbackLatency->setDecimals(3);
    m_playbackLatency->setSuffix(" s");
    latencyForm->addRow("Playback:", m_playbackLatency);

    auto* noteLabel = new QLabel("Positive values = shift lyrics later.\nNegative values = shift lyrics earlier.", this);
    noteLabel->setStyleSheet("color:#888; font-size:10px; border:none;");
    noteLabel->setWordWrap(true);
    latencyForm->addRow("", noteLabel);

    mainLayout->addWidget(latencyGroup);
    mainLayout->addStretch();

    // ── Connections ──────────────────────────────────────────────────────
    connect(m_offsetSlider, &QSlider::valueChanged, this, &TimingCalibration::onSliderChanged);
    connect(m_offsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TimingCalibration::onSpinChanged);
    connect(m_resetBtn, &QPushButton::clicked, this, &TimingCalibration::onReset);

    connect(m_uvrLatency, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) { emit latencyChanged("uvr", val); });
    connect(m_transcriptionLatency, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) { emit latencyChanged("transcription", val); });
    connect(m_playbackLatency, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) { emit latencyChanged("playback", val); });
}

void TimingCalibration::applyTheme() {
    setStyleSheet(R"(
        QWidget { background:#1e1e1e; color:#d4d4d4; }
        QGroupBox { border:1px solid #3f3f46; border-radius:6px; padding-top:16px; margin-top:6px; font-weight:bold; color:#ccc; }
        QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }
        QDoubleSpinBox { background:#2d2d30; border:1px solid #3f3f46; border-radius:4px; padding:4px 6px; color:#fff; }
        QSlider::groove:horizontal { background:#3f3f46; height:6px; border-radius:3px; }
        QSlider::handle:horizontal { background:#22d3ee; width:16px; margin:-5px 0; border-radius:8px; }
        QSlider::sub-page:horizontal { background:#22d3ee; border-radius:3px; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:5px 10px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QLabel { border:none; }
    )");
}

void TimingCalibration::setAudioClock(AudioClock* clock) {
    m_clock = clock;
    if (clock) {
        m_offsetSpin->blockSignals(true);
        m_offsetSlider->blockSignals(true);
        double offset = clock->masterOffset();
        m_offsetSpin->setValue(offset);
        m_offsetSlider->setValue(static_cast<int>(offset * 1000.0));
        m_offsetLabel->setText(QString::number(offset, 'f', 3) + " s");
        m_offsetSpin->blockSignals(false);
        m_offsetSlider->blockSignals(false);

        m_uvrLatency->setValue(clock->getLatency("uvr"));
        m_transcriptionLatency->setValue(clock->getLatency("transcription"));
        m_playbackLatency->setValue(clock->getLatency("playback"));
    }
}

void TimingCalibration::onSliderChanged(int value) {
    double seconds = value / 1000.0;
    m_offsetSpin->blockSignals(true);
    m_offsetSpin->setValue(seconds);
    m_offsetSpin->blockSignals(false);
    m_offsetLabel->setText(QString::number(seconds, 'f', 3) + " s");
    if (m_clock) m_clock->setMasterOffset(seconds);
    emit offsetChanged(seconds);
}

void TimingCalibration::onSpinChanged(double value) {
    m_offsetSlider->blockSignals(true);
    m_offsetSlider->setValue(static_cast<int>(value * 1000.0));
    m_offsetSlider->blockSignals(false);
    m_offsetLabel->setText(QString::number(value, 'f', 3) + " s");
    if (m_clock) m_clock->setMasterOffset(value);
    emit offsetChanged(value);
}

void TimingCalibration::onReset() {
    m_offsetSlider->setValue(0);
    m_uvrLatency->setValue(0);
    m_transcriptionLatency->setValue(0);
    m_playbackLatency->setValue(0);
}

} // namespace ncktv
