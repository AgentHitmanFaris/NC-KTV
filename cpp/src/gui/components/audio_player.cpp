#include "audio_player.h"
#include <cmath>
#include <QFile>
#include <QUrl>
#include <QDebug>

namespace ncktv {

AudioPlayer::AudioPlayer(QWidget* parent)
    : QWidget(parent)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    setupUi();
    setupPlayerConnections();
}

void AudioPlayer::setupPlayerConnections() {
    // Capture pointers explicitly to avoid any 'this' confusion in lambdas if possible
    auto* player = m_player;

    QObject::connect(player, &QMediaPlayer::positionChanged, this, [this, player](qint64 pos) {
        double seconds = pos / 1000.0;
        double total = player->duration() / 1000.0;
        this->setTime(seconds, total);
        emit this->positionChanged(seconds);
    });

    QObject::connect(player, &QMediaPlayer::durationChanged, this, [this](qint64 dur) {
        emit this->durationChanged(dur / 1000.0);
    });

    QObject::connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        // Only act when media is freshly loaded — NOT on subsequent BufferedMedia
        // signals that Qt can fire redundantly. Consuming the flags here exactly once
        // prevents the 3rd-toggle reset bug.
        if (status == QMediaPlayer::LoadedMedia) {
            if (this->m_pendingSeek >= 0.0) {
                const double target = this->m_pendingSeek;
                this->m_pendingSeek = -1.0;
                this->m_player->setPosition(static_cast<qint64>(target * 1000.0));
            }
            if (this->m_pendingPlay) {
                this->m_pendingPlay = false;
                this->m_player->play();
            }
        }
        emit this->mediaStatusChanged(status);
    });
}

void AudioPlayer::loadSource(const QString& filePath) {
    if (QFile::exists(filePath)) {
        m_pendingSeek = m_player->position() / 1000.0;
        m_pendingPlay = (m_player->playbackState() == QMediaPlayer::PlayingState);

        // Explicitly set position to 0 right before source change avoids UI jumps
        m_player->setSource(QUrl::fromLocalFile(filePath));
        
        qDebug() << "AudioPlayer: Loaded source" << filePath;
    } else {
        qDebug() << "AudioPlayer: Source not found" << filePath;
    }
}

void AudioPlayer::seek(double timeSeconds) {
    m_player->setPosition(static_cast<qint64>(timeSeconds * 1000.0));
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
    m_audioOutput->setVolume(0.8);
    m_volumeSlider->setMaximumWidth(150);

    layout->addWidget(m_playBtn);
    layout->addWidget(m_stopBtn);
    layout->addWidget(m_timeLabel);
    layout->addStretch();
    layout->addWidget(new QLabel("Volume:", this));
    layout->addWidget(m_volumeSlider);

    // Connections
    QObject::connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        if (this->m_player->playbackState() == QMediaPlayer::PlayingState) {
            this->m_player->pause();
        } else {
            this->m_player->play();
        }
        emit this->playPauseToggled();
    });

    QObject::connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        this->setPlaying(state == QMediaPlayer::PlayingState);
    });

    QObject::connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        this->m_player->stop();
        emit this->stopRequested();
    });

    QObject::connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int vol) {
        this->m_audioOutput->setVolume(vol / 100.0);
        emit this->volumeChanged(vol);
    });
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
