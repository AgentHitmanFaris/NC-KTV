#pragma once
/*
 * NC-KTV GUI — Curve Editor
 * Visual Bézier animation curve editing widget
 */

#include <QWidget>
#include <QPair>
#include <QVector>
#include "timeline/timeline_data.h"

namespace ncktv {

class CurveEditor : public QWidget {
    Q_OBJECT

public:
    explicit CurveEditor(QWidget* parent = nullptr);

    void setCurve(EasingCurve curve);
    void setCustomPoints(const QVector<QPair<double,double>>& points);
    EasingCurve curve() const { return m_curve; }
    QVector<QPair<double,double>> customPoints() const { return m_controlPoints; }

signals:
    void curveChanged(EasingCurve curve);
    void controlPointsChanged(const QVector<QPair<double,double>>& points);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    double evaluateCurve(double t) const;
    QPointF toWidget(double x, double y) const;
    QPointF fromWidget(const QPointF& pos) const;

    EasingCurve m_curve = EasingCurve::Linear;
    QVector<QPair<double,double>> m_controlPoints;
    int m_dragIndex = -1;
    QMargins m_padding = {40, 20, 20, 40};
};

} // namespace ncktv
