/*
 * NC-KTV GUI — Word Editor (Waveform Drag UX)
 */

#include "word_editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QScrollBar>
#include <qmath.h>

namespace ncktv {

// ═════════════════════════════════════════════════════════════════════════════
// WordCanvas
// ═════════════════════════════════════════════════════════════════════════════

WordCanvas::WordCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumHeight(160);
    setCursor(Qt::IBeamCursor);
}

void WordCanvas::setLine(core::LyricsLine* line) {
    m_line = line;
    updateCanvasSize();
    update();
}

void WordCanvas::setScale(int pixelsPerSecond) {
    m_pixelsPerSecond = qMax(50, pixelsPerSecond);
    updateCanvasSize();
    update();
}

void WordCanvas::setSelectedIndex(int index) {
    if (m_selectedIndex != index) {
        m_selectedIndex = index;
        emit selectionChanged(index);
        update();
    }
}

void WordCanvas::setCurrentTime(double timeSeconds) {
    if (m_currentTime != timeSeconds) {
        m_currentTime = timeSeconds;
        update();
    }
}

void WordCanvas::updateCanvasSize() {
    if (!m_line || m_line->tokens.empty()) {
        m_baseTime = 0.0;
        m_maxTime = 5.0;
        setMinimumHeight(400);
        return;
    }
    m_baseTime = qMax(0.0, m_line->start_time - 1.0);
    m_maxTime = m_line->end_time + 1.0;
    int h = static_cast<int>((m_maxTime - m_baseTime) * m_pixelsPerSecond) + m_paddingY * 2;
    setMinimumHeight(h);
    // Force a reasonable width if not constrained
    setMinimumWidth(400);
    generateMockWaveform();
}

void WordCanvas::generateMockWaveform() {
    m_mockWaveform.clear();
    // Waveform is now vertical, so height matters
    int samples = height() / 2;
    if (samples <= 0) return;
    
    // Pseudo-random deterministic waveform shape
    m_mockWaveform.resize(samples);
    int seed = m_line ? qHash(QString::fromStdString(m_line->text)) : 12345;
    double phase = 0;
    for (int i = 0; i < samples; ++i) {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        double noise = (seed % 100) / 100.0;
        phase += 0.1;
        double env = qAbs(qSin(phase)) * 0.8 + 0.2;
        int amp = static_cast<int>(env * noise * 40); // 40px width max amp
        m_mockWaveform[i] = qMax(2, amp);
    }
}

double WordCanvas::timeAtY(int y) const {
    return m_baseTime + static_cast<double>(y - m_paddingY) / m_pixelsPerSecond;
}

int WordCanvas::yAtTime(double t) const {
    return m_paddingY + static_cast<int>((t - m_baseTime) * m_pixelsPerSecond);
}

QRect WordCanvas::rectForWord(int index) const {
    if (!m_line || index < 0 || index >= m_line->tokens.size()) return QRect();
    int y1 = yAtTime(m_line->tokens[index].start_time);
    int y2 = yAtTime(m_line->tokens[index].end_time);
    int h = qMax(y2 - y1, 4);
    
    // Karaoke Builder Style: cascading blocks and waveform on left
    // Waveform width is ~90 px (0 to 90)
    int leftMargin = 100;
    int blockWidth = 50; // width of each orange block
    int stagger = 0; // "full vertical" -> cascade to the right removed
    
    int x1 = leftMargin + (index * stagger);
    return QRect(x1, y1, blockWidth, h);
}

void WordCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), QColor("#dddddd")); // Lighter background

    if (!m_line) return;

    int H = height();
    int W = width();

    // Draw Mock Waveform background (black) on the left
    int waveWidth = 90;
    p.fillRect(0, 0, waveWidth, H, Qt::black);

    // Draw Mock Waveform (Green, like Karaoke Builder Studio)
    p.setPen(QColor("#00ff00"));
    for (int i = 0; i < m_mockWaveform.size(); ++i) {
        int y = i * 2;
        int amp = m_mockWaveform[i];
        p.drawLine(waveWidth / 2 - amp, y, waveWidth / 2 + amp, y);
    }

    // Ruler text color
    p.setPen(QPen(QColor("#666"), 1));

    // Timelines
    for (double t = std::floor(m_baseTime); t <= m_maxTime; t += 0.5) {
        int py = yAtTime(t);
        p.drawLine(waveWidth, py, W, py);
        if (t - std::floor(t) < 0.1) {
            p.drawText(waveWidth + 4, py - 2, QString::number(t, 'f', 1) + "s");
        }
    }

    // Draw shaded background block for all lyrics
    if (!m_line->tokens.empty()) {
        int firstY = yAtTime(m_line->tokens.front().start_time);
        int lastY = yAtTime(m_line->tokens.back().end_time);
        int cascadeTotalWidth = (m_line->tokens.size() * 20) + 120; // Estimate
        p.fillRect(waveWidth + 10, firstY, cascadeTotalWidth, lastY - firstY, QColor(0,0,0, 20)); // slight grey
    }

    // Word Blocks
    for (int i = 0; i < m_line->tokens.size(); ++i) {
        QRect r = rectForWord(i);
        bool selected = (i == m_selectedIndex);

        // Fill
        QColor blockColor = QColor("#f97316"); // KBS orange
        if (selected) blockColor = blockColor.lighter(130);
        //blockColor.setAlpha(190); 
        p.setPen(Qt::NoPen);
        p.setBrush(blockColor);
        p.drawRect(r);

        // Draw text to the left or right? Karaoke Builder draws it next to it or on it.
        p.setPen(Qt::black);
        p.setFont(QFont("Segoe UI", 9));
        // text is usually placed aligned to the block
        p.drawText(r.right() + 4, r.top() + 12, QString::fromStdString(m_line->tokens[i].text));

        // Border
        p.setPen(QPen(selected ? Qt::white : QColor("#c2410c"), selected ? 2 : 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);

        // Drag handles if selected
        if (selected) {
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::white);
            p.drawRect(r.left() + r.width()/2 - 10, r.top() - 2, 20, 4);
            p.drawRect(r.left() + r.width()/2 - 10, r.bottom() - 2, 20, 4);
        }

        // Warning for negative/zero duration
        if (m_line->tokens[i].end_time <= m_line->tokens[i].start_time) {
            p.setPen(QPen(QColor("#ef4444"), 2));
            p.drawRect(r.adjusted(-1, -1, 1, 1));
        }
    }

    // Playhead line (Red horizontal)
    if (m_currentTime >= 0.0) {
        int cy = yAtTime(m_currentTime);
        if (cy >= 0 && cy <= H) {
            p.setPen(QPen(QColor("#ef4444"), 1)); // Red playhead
            p.drawLine(0, cy, W, cy);
        }
    }
}

void WordCanvas::mousePressEvent(QMouseEvent* event) {
    if (!m_line) return;
    int y = event->pos().y();
    int x = event->pos().x();

    int hitIndex = -1;
    DragMode mode = None;

    // Check handles first (edges) - now top/bottom
    for (int i = 0; i < m_line->tokens.size(); ++i) {
        QRect r = rectForWord(i);
        // expand X hit area slightly
        QRect rTop(r.left() - 10, r.top() - 6, r.width() + 20, 12);
        QRect rBot(r.left() - 10, r.bottom() - 6, r.width() + 20, 12);
        
        if (rTop.contains(event->pos())) { hitIndex = i; mode = DragStart; break; }
        if (rBot.contains(event->pos())) { hitIndex = i; mode = DragEnd; break; }
    }

    // Then body
    if (hitIndex == -1) {
        for (int i = 0; i < m_line->tokens.size(); ++i) {
            QRect r = rectForWord(i);
            // also consider the text area right of the block
            QRect rHit = r;
            rHit.setWidth(r.width() + 100); 
            if (rHit.contains(event->pos())) {
                hitIndex = i; mode = DragBody; break;
            }
        }
    }

    setSelectedIndex(hitIndex);
    m_dragIndex = hitIndex;
    m_dragMode = mode;

    if (hitIndex != -1) {
        m_dragOriginalStart = m_line->tokens[hitIndex].start_time;
        m_dragOriginalEnd   = m_line->tokens[hitIndex].end_time;
        m_dragMouseStartY   = y;
        if (mode == DragBody) setCursor(Qt::ClosedHandCursor);
    } else {
        // Did not click a word block -> manual seek inside WordCanvas
        emit seekRequested(std::max(0.0, timeAtY(y)));
    }
}

void WordCanvas::mouseMoveEvent(QMouseEvent* event) {
    int y = event->pos().y();
    int x = event->pos().x();

    // Handle cursor icons
    if (m_dragMode == None && m_line) {
        bool onEdge = false;
        bool onBody = false;
        for (int i = 0; i < m_line->tokens.size(); ++i) {
            QRect r = rectForWord(i);
            QRect rTop(r.left() - 10, r.top() - 6, r.width() + 20, 12);
            QRect rBot(r.left() - 10, r.bottom() - 6, r.width() + 20, 12);
            if (rTop.contains(event->pos()) || rBot.contains(event->pos())) {
                onEdge = true; break;
            }
            QRect rHit = r; rHit.setWidth(r.width() + 100); 
            if (rHit.contains(event->pos())) {
                onBody = true; break;
            }
        }
        if (onEdge) setCursor(Qt::SizeVerCursor);
        else if (onBody) setCursor(Qt::OpenHandCursor);
        else setCursor(Qt::IBeamCursor);
        
        // Manual seek drag on background
        if (event->buttons() & Qt::LeftButton && !onBody && !onEdge) {
            emit seekRequested(std::max(0.0, timeAtY(y)));
        }
        return;
    }

    // Actively dragging
    if (m_dragIndex < 0 || m_dragIndex >= m_line->tokens.size()) return;

    double timeDelta = timeAtY(y) - timeAtY(m_dragMouseStartY);
    core::LyricsToken& w = m_line->tokens[m_dragIndex];

    if (m_dragMode == DragBody) {
        w.start_time = m_dragOriginalStart + timeDelta;
        w.end_time = m_dragOriginalEnd + timeDelta;
    } else if (m_dragMode == DragStart) {
        double newStart = m_dragOriginalStart + timeDelta;
        if (newStart > w.end_time - 0.01) newStart = w.end_time - 0.01; // prevent negative duration
        w.start_time = newStart;
        if (m_sticky && m_dragIndex > 0) {
            m_line->tokens[m_dragIndex - 1].end_time = newStart; // Keep borders together
        }
    } else if (m_dragMode == DragEnd) {
        double newEnd = m_dragOriginalEnd + timeDelta;
        if (newEnd < w.start_time + 0.01) newEnd = w.start_time + 0.01;
        w.end_time = newEnd;
        if (m_sticky && m_dragIndex < m_line->tokens.size() - 1) {
            m_line->tokens[m_dragIndex + 1].start_time = newEnd; // Keep borders together
        }
    }

    emit wordModified();
    update();
}

void WordCanvas::mouseReleaseEvent(QMouseEvent*) {
    m_dragMode = None;
    int y = mapFromGlobal(QCursor::pos()).y();
    mouseMoveEvent(new QMouseEvent(QEvent::MouseMove, QPointF(0, y), Qt::NoButton, Qt::NoButton, Qt::NoModifier)); // reset cursor
}

void WordCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    // Add word block at double click
    if (!m_line || m_dragMode != None) return;
    double t = timeAtY(event->pos().y());
    
    // Don't add if already inside a word
    for (const auto& w : m_line->tokens) {
        if (t >= w.start_time && t <= w.end_time) return;
    }

    core::LyricsToken newWord{"new", static_cast<float>(t), static_cast<float>(t + 0.5)};
    
    // Find insertion index
    int idx = 0;
    while (idx < m_line->tokens.size() && m_line->tokens[idx].start_time < t) {
        idx++;
    }
    m_line->tokens.insert(m_line->tokens.begin() + idx, newWord);
    
    emit wordModified();
    updateCanvasSize();
    setSelectedIndex(idx);
    update();
}

void WordCanvas::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        int zoom = m_pixelsPerSecond + (event->angleDelta().y() > 0 ? 50 : -50);
        setScale(zoom);
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}


// ═════════════════════════════════════════════════════════════════════════════
// WordEditor Dialog
// ═════════════════════════════════════════════════════════════════════════════

WordEditor::WordEditor(core::LyricsLine* line, QWidget* parent)
    : QDialog(parent), m_line(line)
{
    setWindowTitle("Karaoke Timeline Editor — " + (line ? QString::fromStdString(line->text) : ""));
    setMinimumSize(900, 360);
    setModal(true);
    setupUi();
    applyTheme();
    updateButtons();
}

void WordEditor::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(16, 14, 16, 14);

    // Toolbar
    auto* toolbar = new QHBoxLayout();
    
    m_stickyCheck = new QCheckBox("🧲 Sticky Borders", this);
    m_stickyCheck->setChecked(true);
    m_stickyCheck->setToolTip("When dragging a word edge, automatically snap the adjacent word's edge to match.");
    toolbar->addWidget(m_stickyCheck);
    toolbar->addSpacing(20);

    m_addBtn = new QPushButton("＋ Add Word", this);
    m_splitBtn = new QPushButton("✂ Split", this);
    m_joinBtn = new QPushButton("⫘ Join Next", this);
    m_deleteBtn = new QPushButton("✕ Delete", this);
    
    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_splitBtn);
    toolbar->addWidget(m_joinBtn);
    toolbar->addWidget(m_deleteBtn);
    toolbar->addStretch();

    // Zoom
    toolbar->addWidget(new QLabel("Zoom: ", this));
    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(50, 1000);
    m_zoomSlider->setValue(300);
    m_zoomSlider->setFixedWidth(150);
    toolbar->addWidget(m_zoomSlider);

    mainLayout->addLayout(toolbar);

    // Canvas inside ScrollArea
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border:1px solid #3f3f46; border-radius:4px; }");
    
    m_canvas = new WordCanvas(m_scrollArea);
    if (m_line) m_canvas->setLine(m_line);
    m_scrollArea->setWidget(m_canvas);
    mainLayout->addWidget(m_scrollArea, 1);

    // Edit Bar
    auto* editLayout = new QHBoxLayout();
    editLayout->addWidget(new QLabel("Selected Word Text:", this));
    m_textEdit = new QLineEdit(this);
    m_textEdit->setEnabled(false);
    m_textEdit->setStyleSheet("QLineEdit { background:#252526; border:1px solid #4f46e5; padding:6px; color:white; font-size:14px; font-weight:bold; }");
    editLayout->addWidget(m_textEdit, 1);

    m_timeLabel = new QLabel("-- / --", this);
    m_timeLabel->setFixedWidth(120);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setStyleSheet("color:#888; font-weight:bold;");
    editLayout->addWidget(m_timeLabel);
    
    auto* okBtn = new QPushButton("Apply Tracking", this);
    okBtn->setDefault(true);
    okBtn->setStyleSheet("background:#4f46e5; color:white; font-weight:bold; padding:8px 24px; border:none; border-radius:4px;");
    editLayout->addWidget(okBtn);

    mainLayout->addLayout(editLayout);

    // Listeners
    connect(m_stickyCheck, &QCheckBox::toggled, m_canvas, &WordCanvas::setSticky);
    connect(m_canvas, &WordCanvas::selectionChanged, this, &WordEditor::onSelectionChanged);
    connect(m_canvas, &WordCanvas::wordModified, this, &WordEditor::onWordModified);
    connect(m_zoomSlider, &QSlider::valueChanged, this, &WordEditor::onZoomChanged);
    
    connect(m_addBtn, &QPushButton::clicked, this, &WordEditor::onAddWord);
    connect(m_splitBtn, &QPushButton::clicked, this, &WordEditor::onSplitWord);
    connect(m_joinBtn, &QPushButton::clicked, this, &WordEditor::onJoinWord);
    connect(m_deleteBtn, &QPushButton::clicked, this, &WordEditor::onDeleteWord);
    
    connect(m_textEdit, &QLineEdit::textEdited, this, &WordEditor::onTextChanged);
    connect(okBtn, &QPushButton::clicked, this, &WordEditor::onAccepted);

    if (m_line && !m_line->tokens.empty()) {
        m_canvas->setSelectedIndex(0);
    }
}

void WordEditor::applyTheme() {
    setStyleSheet(R"(
        QDialog { background:#1e1e1e; color:#d4d4d4; }
        QPushButton { background:#333; border:1px solid #555; border-radius:4px; padding:6px 14px; color:#ccc; }
        QPushButton:hover { background:#404040; }
        QPushButton:disabled { color:#555; border-color:#444; }
        QCheckBox { color:#ccc; }
        QLabel { color:#ccc; border:none; }
    )");
}

void WordEditor::updateButtons() {
    int idx = m_canvas->selectedIndex();
    bool hasSelection = (idx >= 0 && idx < (m_line ? m_line->tokens.size() : 0));
    
    m_splitBtn->setEnabled(hasSelection);
    m_deleteBtn->setEnabled(hasSelection);
    m_joinBtn->setEnabled(hasSelection && idx < m_line->tokens.size() - 1);
    
    m_textEdit->setEnabled(hasSelection);
    if (!hasSelection) {
        m_textEdit->clear();
        m_timeLabel->setText("-- / --");
    }
}

void WordEditor::onSelectionChanged(int index) {
    updateButtons();
    if (m_line && index >= 0 && index < m_line->tokens.size()) {
        m_textEdit->setText(QString::fromStdString(m_line->tokens[index].text));
        m_textEdit->setFocus();
        m_textEdit->selectAll();
        onWordModified(); // update time label
    }
}

void WordEditor::onWordModified() {
    m_modified = true;
    int idx = m_canvas->selectedIndex();
    if (m_line && idx >= 0 && idx < m_line->tokens.size()) {
        const auto& w = m_line->tokens[idx];
        m_timeLabel->setText(QString("%1s - %2s\nDur: %3s")
            .arg(w.start_time, 0, 'f', 2).arg(w.end_time, 0, 'f', 2).arg(w.end_time - w.start_time, 0, 'f', 2));
    }
}

void WordEditor::onTextChanged(const QString& text) {
    int idx = m_canvas->selectedIndex();
    if (m_line && idx >= 0 && idx < m_line->tokens.size()) {
        m_line->tokens[idx].text = text.toStdString();
        m_modified = true;
        m_canvas->update();
    }
}

void WordEditor::onZoomChanged(int value) {
    m_canvas->setScale(value);
}

void WordEditor::onAddWord() {
    if (!m_line) return;
    double start = m_line->tokens.empty() ? m_line->start_time : m_line->tokens.back().end_time;
    m_line->tokens.push_back({"new_word", static_cast<float>(start), static_cast<float>(start + 0.5)});
    m_modified = true;
    m_canvas->setLine(m_line);
    m_canvas->setSelectedIndex(m_line->tokens.size() - 1);
}

void WordEditor::onSplitWord() {
    int idx = m_canvas->selectedIndex();
    if (!m_line || idx < 0 || idx >= m_line->tokens.size()) return;
    
    auto& orig = m_line->tokens[idx];
    double midTime = (orig.start_time + orig.end_time) / 2.0;
    QString text = QString::fromStdString(orig.text);
    int midChar = qMax(1, text.length() / 2);
    
    core::LyricsToken word2{text.mid(midChar).toStdString(), static_cast<float>(midTime), orig.end_time};
    orig.text = text.left(midChar).toStdString();
    orig.end_time = static_cast<float>(midTime);
    
    m_line->tokens.insert(m_line->tokens.begin() + idx + 1, word2);
    m_modified = true;
    m_canvas->setLine(m_line);
    m_canvas->setSelectedIndex(idx + 1);
}

void WordEditor::onJoinWord() {
    int idx = m_canvas->selectedIndex();
    if (!m_line || idx < 0 || idx >= m_line->tokens.size() - 1) return;
    
    m_line->tokens[idx].text = (QString::fromStdString(m_line->tokens[idx].text) + " " + QString::fromStdString(m_line->tokens[idx + 1].text)).toStdString();
    m_line->tokens[idx].end_time = m_line->tokens[idx + 1].end_time;
    m_line->tokens.erase(m_line->tokens.begin() + idx + 1);
    
    m_modified = true;
    m_canvas->setLine(m_line);
    m_canvas->setSelectedIndex(idx);
}

void WordEditor::onDeleteWord() {
    int idx = m_canvas->selectedIndex();
    if (!m_line || idx < 0 || idx >= m_line->tokens.size()) return;
    
    m_line->tokens.erase(m_line->tokens.begin() + idx);
    m_modified = true;
    m_canvas->setLine(m_line);
    m_canvas->setSelectedIndex(qMin(idx, static_cast<int>(m_line->tokens.size()) - 1));
}

void WordEditor::onAccepted() {
    if (m_line && m_modified) {
        // Rebuild full line text
        QStringList parts;
        for (const auto& w : m_line->tokens) {
            if (!QString::fromStdString(w.text).isEmpty()) parts << QString::fromStdString(w.text);
        }
        m_line->text = parts.join(" ").toStdString();
        
        // Adjust bounds
        if (!m_line->tokens.empty()) {
            m_line->start_time = m_line->tokens.front().start_time;
            m_line->end_time = m_line->tokens.back().end_time;
        }
    }
    accept();
}

} // namespace ncktv
