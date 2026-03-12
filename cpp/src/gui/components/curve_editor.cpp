/*
 * NC-KTV GUI — Curve Editor Implementation
 * Visual Bézier curve editor with drag-and-drop control points
 */

#include "curve_editor.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <cmath>

namespace ncktv {

CurveEditor::CurveEditor(QWidget* parent) : QWidget(parent) {
    setMinimumSize(200, 180);
    setMouseTracking(true);
    // Default cubic Bézier control points
    m_controlPoints = {{0.25, 0.1}, {0.75, 0.9}};
}

void CurveEditor::setCurve(EasingCurve curve) {
    m_curve = curve;
    // Set default control points based on curve type
    switch (curve) {
        case EasingCurve::Linear:      m_controlPoints = {{0.0, 0.0}, {1.0, 1.0}}; break;
        case EasingCurve::EaseIn:      m_controlPoints = {{0.42, 0.0}, {1.0, 1.0}}; break;
        case EasingCurve::EaseOut:     m_controlPoints = {{0.0, 0.0}, {0.58, 1.0}}; break;
        case EasingCurve::EaseInOut:   m_controlPoints = {{0.42, 0.0}, {0.58, 1.0}}; break;
        case EasingCurve::CustomBezier: break; // Keep existing
    }
    update();
    emit curveChanged(curve);
}

void CurveEditor::setCustomPoints(const QVector<QPair<double,double>>& points) {
    m_controlPoints = points;
    m_curve = EasingCurve::CustomBezier;
    update();
}

double CurveEditor::evaluateCurve(double t) const {
    // Cubic Bézier: B(t) = (1-t)³·P0 + 3(1-t)²t·P1 + 3(1-t)t²·P2 + t³·P3
    if (m_controlPoints.size() < 2) return t;
    double p1y = m_controlPoints[0].second;
    double p2y = m_controlPoints[1].second;
    double u = 1.0 - t;
    return 3.0 * u * u * t * p1y + 3.0 * u * t * t * p2y + t * t * t;
}

QPointF CurveEditor::toWidget(double x, double y) const {
    double w = width()  - m_padding.left() - m_padding.right();
    double h = height() - m_padding.top()  - m_padding.bottom();
    return QPointF(m_padding.left() + x * w, m_padding.top() + (1.0 - y) * h);
}

QPointF CurveEditor::fromWidget(const QPointF& pos) const {
    double w = width()  - m_padding.left() - m_padding.right();
    double h = height() - m_padding.top()  - m_padding.bottom();
    double x = (pos.x() - m_padding.left()) / w;
    double y = 1.0 - (pos.y() - m_padding.top()) / h;
    return QPointF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0));
}

void CurveEditor::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // ── Background ───────────────────────────────────────────────────────
    p.fillRect(rect(), QColor("#1e1e1e"));

    // ── Grid ─────────────────────────────────────────────────────────────
    p.setPen(QPen(QColor("#2d2d30"), 1));
    for (int i = 0; i <= 4; ++i) {
        double t = i / 4.0;
        QPointF left  = toWidget(0, t);
        QPointF right = toWidget(1, t);
        p.drawLine(left, right);
        QPointF top    = toWidget(t, 0);
        QPointF bottom = toWidget(t, 1);
        p.drawLine(top, bottom);
    }

    // ── Diagonal reference (linear) ──────────────────────────────────────
    p.setPen(QPen(QColor("#3f3f46"), 1, Qt::DashLine));
    p.drawLine(toWidget(0, 0), toWidget(1, 1));

    // ── Control point handles ────────────────────────────────────────────
    if (m_controlPoints.size() >= 2) {
        QPointF cp1 = toWidget(m_controlPoints[0].first, m_controlPoints[0].second);
        QPointF cp2 = toWidget(m_controlPoints[1].first, m_controlPoints[1].second);

        // Handle lines
        p.setPen(QPen(QColor("#4f46e5"), 1, Qt::DashLine));
        p.drawLine(toWidget(0, 0), cp1);
        p.drawLine(toWidget(1, 1), cp2);

        // Handle dots
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#4f46e5"));
        p.drawEllipse(cp1, 6, 6);
        p.drawEllipse(cp2, 6, 6);
    }

    // ── Curve ────────────────────────────────────────────────────────────
    QPainterPath curvePath;
    curvePath.moveTo(toWidget(0, 0));
    constexpr int STEPS = 60;
    for (int i = 1; i <= STEPS; ++i) {
        double t = i / double(STEPS);
        double y = evaluateCurve(t);
        curvePath.lineTo(toWidget(t, y));
    }
    p.setPen(QPen(QColor("#22d3ee"), 2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(curvePath);

    // ── Axis Labels ──────────────────────────────────────────────────────
    p.setPen(QColor("#888"));
    p.setFont(QFont("Segoe UI", 9));
    p.drawText(QPointF(m_padding.left() - 5, height() - 8), "0");
    p.drawText(toWidget(1, 0) + QPointF(-5, 16), "1");
    p.drawText(toWidget(0, 1) + QPointF(-20, 4), "1");
}

void CurveEditor::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    m_dragIndex = -1;
    for (int i = 0; i < m_controlPoints.size(); ++i) {
        QPointF cp = toWidget(m_controlPoints[i].first, m_controlPoints[i].second);
        if ((event->pos() - cp).manhattanLength() < 12) {
            m_dragIndex = i;
            break;
        }
    }
}

void CurveEditor::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragIndex < 0 || m_dragIndex >= m_controlPoints.size()) return;

    QPointF norm = fromWidget(event->pos());
    m_controlPoints[m_dragIndex] = {norm.x(), norm.y()};
    m_curve = EasingCurve::CustomBezier;
    update();
    emit controlPointsChanged(m_controlPoints);
}

void CurveEditor::mouseReleaseEvent(QMouseEvent*) {
    m_dragIndex = -1;
}

} // namespace ncktv
