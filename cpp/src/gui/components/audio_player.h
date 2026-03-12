#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QHBoxLayout>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTimer>

namespace ncktv {

class AudioPlayer : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY(AudioPlayer)

public:
    explicit AudioPlayer(QWidget* parent = nullptr);

    void loadSource(const QString& filePath);
    void setTime(double timeSeconds, double totalSeconds);
    void setPlaying(bool isPlaying);
    void seek(double timeSeconds);
    
    // Expose player so other components can attach video sinks
    QMediaPlayer* player() const { return m_player; }

signals:
    void positionChanged(double timeSeconds);
    void durationChanged(double totalSeconds);
    void mediaStatusChanged(QMediaPlayer::MediaStatus status);
    void playPauseToggled();
    void stopRequested();
    void seekRequested(double timeSeconds);
    void volumeChanged(int volume);

private:
    void setupUi();
    void setupPlayerConnections();
    QString formatTime(double seconds);

    QPushButton* m_playBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QLabel*      m_timeLabel = nullptr;
    QSlider*     m_volumeSlider = nullptr;

    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
    QTimer* m_syncTimer = nullptr;
    
    bool m_pendingPlay = false;
    double m_pendingSeek = -1.0;
};

} // namespace ncktv
