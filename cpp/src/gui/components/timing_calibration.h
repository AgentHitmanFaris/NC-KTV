#pragma once
/*
 * NC-KTV GUI — Timing Calibration Widget
 * Visual drift correction tool for audio sync
 */

#include <QWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include "audio/audio_clock.h"

namespace ncktv {

class TimingCalibration : public QWidget {
    Q_OBJECT

public:
    explicit TimingCalibration(QWidget* parent = nullptr);

    void setAudioClock(AudioClock* clock);

signals:
    void offsetChanged(double offsetSeconds);
    void latencyChanged(const QString& source, double latency);

private slots:
    void onSliderChanged(int value);
    void onSpinChanged(double value);
    void onReset();

private:
    void setupUi();
    void applyTheme();

    AudioClock*     m_clock = nullptr;
    QSlider*        m_offsetSlider = nullptr;
    QDoubleSpinBox* m_offsetSpin = nullptr;
    QLabel*         m_offsetLabel = nullptr;
    QDoubleSpinBox* m_uvrLatency = nullptr;
    QDoubleSpinBox* m_transcriptionLatency = nullptr;
    QDoubleSpinBox* m_playbackLatency = nullptr;
    QPushButton*    m_resetBtn = nullptr;
};

} // namespace ncktv
