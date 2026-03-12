#pragma once
/*
 * NC-KTV GUI — Word Editor Dialog (Karaoke Builder Studio Style)
 * Waveform-style timeline editor for word-by-word syncing.
 */

#include <QDialog>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include "../../core/timeline/ncktv_core_data.hpp"

namespace ncktv {

// ── Visual Canvas for Waveform and Word Blocks ───────────────────────────────
class WordCanvas : public QWidget {
    Q_OBJECT
public:
    explicit WordCanvas(QWidget* parent = nullptr);
    void setLine(core::LyricsLine* line);
    void setSticky(bool sticky) { m_sticky = sticky; update(); }
    void setScale(int pixelsPerSecond);
    void setCurrentTime(double timeSeconds);
    
    double timeAtY(int y) const;
    int yAtTime(double t) const;

    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);

signals:
    void selectionChanged(int index);
    void wordModified();
    void seekRequested(double timeSeconds);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QRect rectForWord(int index) const;
    void updateCanvasSize();
    void generateMockWaveform();

    enum DragMode { None, DragStart, DragEnd, DragBody };

    core::LyricsLine* m_line = nullptr;
    int m_selectedIndex = -1;
    bool m_sticky = true;
    int m_pixelsPerSecond = 300;
    double m_baseTime = 0.0;
    double m_maxTime = 0.0;
    double m_currentTime = -1.0;
    const int m_paddingY = 50;
    QVector<int> m_mockWaveform;

    int m_dragIndex = -1;
    DragMode m_dragMode = None;
    double m_dragOriginalStart = 0;
    double m_dragOriginalEnd = 0;
    int m_dragMouseStartY = 0;
};

// ── Main Dialog ──────────────────────────────────────────────────────────────
class WordEditor : public QDialog {
    Q_OBJECT

public:
    explicit WordEditor(core::LyricsLine* line, QWidget* parent = nullptr);
    bool wasModified() const { return m_modified; }

private slots:
    void onSelectionChanged(int index);
    void onWordModified();
    void onSplitWord();
    void onJoinWord();
    void onDeleteWord();
    void onAddWord();
    void onTextChanged(const QString& text);
    void onZoomChanged(int value);
    void onAccepted();

private:
    void setupUi();
    void applyTheme();
    void updateButtons();

    core::LyricsLine* m_line = nullptr;
    bool m_modified = false;

    WordCanvas* m_canvas = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QSlider* m_zoomSlider = nullptr;

    QLineEdit* m_textEdit = nullptr;
    QCheckBox* m_stickyCheck = nullptr;

    QPushButton* m_splitBtn = nullptr;
    QPushButton* m_joinBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_addBtn = nullptr;
    QLabel* m_timeLabel = nullptr;
};

} // namespace ncktv
