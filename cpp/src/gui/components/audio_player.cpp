#include "audio_player.h"

namespace ncktv {

AudioPlayer::AudioPlayer(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void AudioPlayer::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);

    m_playBtn = new QPushButton("▶ Play", this);
    m_stopBtn = new QPushButton("■ Stop", this);
    m_timeLabel = new QLabel("00:00.0 / 00:00.0", this);
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setMaximumWidth(150);

    layout->addWidget(m_playBtn);
    layout->addWidget(m_stopBtn);
    layout->addWidget(m_timeLabel);
    layout->addStretch();
    layout->addWidget(new QLabel("Volume:", this));
    layout->addWidget(m_volumeSlider);

    // Connections
    connect(m_playBtn, &QPushButton::clicked, this, &AudioPlayer::playPauseToggled);
    connect(m_stopBtn, &QPushButton::clicked, this, &AudioPlayer::stopRequested);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &AudioPlayer::volumeChanged);
}

void AudioPlayer::setTime(double timeSeconds, double totalSeconds) {
    m_timeLabel->setText(formatTime(timeSeconds) + " / " + formatTime(totalSeconds));
}

void AudioPlayer::setPlaying(bool isPlaying) {
    if (isPlaying) {
        m_playBtn->setText("⏸ Pause");
    } else {
        m_playBtn->setText("▶ Play");
    }
}

QString AudioPlayer::formatTime(double seconds) {
    int mins = static_cast<int>(seconds) / 60;
    double secs = std::fmod(seconds, 60.0);
    return QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 4, 'f', 1, QChar('0'));
}

} // namespace ncktv

