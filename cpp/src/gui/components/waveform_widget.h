#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector>

namespace ncktv {

class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget* parent = nullptr);

    void loadWaveformData(const QVector<float>& minData, const QVector<float>& maxData, double sampleRate, int samplesPerPixel);
    void updateCursor(double timeSeconds);
    void setPixelsPerSecond(double pps);

    // Markers
    void addMarker(double timeSeconds, const QColor& color, const QString& label = "");
    void clearMarkers();

signals:
    void seekRequested(double timeSeconds);
    void markerClicked(int markerIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawWaveform(QPainter& painter);
    void drawMarkers(QPainter& painter);
    void drawPlayhead(QPainter& painter);

    // Audio Data
    QVector<float> m_minData;
    QVector<float> m_maxData;
    double m_audioDuration = 0.0;
    
    // View state
    double m_pixelsPerSecond = 100.0;
    double m_scrollOffsetX = 0.0;
    double m_currentTime = 0.0;

    struct Marker {
        double time;
        QColor color;
        QString label;
    };
    QVector<Marker> m_markers;
};
} // namespace ncktv
