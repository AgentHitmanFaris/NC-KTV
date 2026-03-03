#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QHBoxLayout>

namespace ncktv {

class AudioPlayer : public QWidget {
    Q_OBJECT
public:
    explicit AudioPlayer(QWidget* parent = nullptr);

    void setTime(double timeSeconds, double totalSeconds);
    void setPlaying(bool isPlaying);

signals:
    void playPauseToggled();
    void stopRequested();
    void seekRequested(double timeSeconds);
    void volumeChanged(int volume);

private:
    void setupUi();
    QString formatTime(double seconds);

    QPushButton* m_playBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QLabel*      m_timeLabel = nullptr;
    QSlider*     m_volumeSlider = nullptr;
};

} // namespace ncktv
