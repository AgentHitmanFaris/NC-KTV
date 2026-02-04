"""
Modern UI Stylesheet for NC-KTV
Stunning dark theme with gradients, glassmorphism, and premium effects
"""

MODERN_DARK_THEME = """
/* ============================================
   GLOBAL APPLICATION STYLING
   ============================================ */

QWidget {
    background-color: #1a1a2e;
    color: #e6e6e6;
    font-family: 'Segoe UI', 'San Francisco', Arial, sans-serif;
    font-size: 13px;
}

QMainWindow {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #1a1a2e, stop:1 #16213e);
}

/* ============================================
   BUTTONS - Modern Gradient Style
   ============================================ */

/* Standard Button */
QPushButton {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #667eea, stop:1 #764ba2);
    color: white;
    border: 2px solid rgba(102, 126, 234, 0.4);
    border-radius: 4px;
    padding: 8px 16px;
    font-weight: 600;
    font-size: 13px;
    min-height: 32px; /* Retained from original */
}

QPushButton:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #f093fb, stop:1 #f5576c);
    border: 2px solid rgba(249, 147, 251, 0.6);
}

QPushButton:pressed {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 rgba(88, 100, 200, 1.0),
        stop:1 rgba(98, 60, 140, 1.0));
    padding-top: 10px;
    padding-bottom: 6px;
}

QPushButton:checked {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 rgba(255, 107, 129, 0.9),
        stop:1 rgba(245, 87, 108, 0.9));
    border: 2px solid rgba(255, 255, 255, 0.6);
    font-weight: bold;
}

QPushButton:disabled {
    background: rgba(60, 60, 70, 0.5);
    color: rgba(150, 150, 150, 0.5);
    border: 1px solid rgba(100, 100, 110, 0.3);
}

/* Primary Action Button (Export, Save, etc.) */
QPushButton[class="primary"] {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #E91E63, stop:1 #F06292);
    min-height: 36px;
    font-size: 14px;
    font-weight: bold;
}

QPushButton[class="primary"]:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #F06292, stop:1 #E91E63);
}

/* Success Button */
QPushButton[class="success"] {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #4CAF50, stop:1 #388E3C);
}

/* ============================================
   TEXT INPUTS - Glassmorphism Style
   ============================================ */

QLineEdit, QTextEdit, QPlainTextEdit {
    background: rgba(30, 30, 45, 0.7);
    border: 2px solid rgba(102, 126, 234, 0.3);
    border-radius: 4px;
    padding: 8px 12px;
    color: #e6e6e6;
    selection-background-color: rgba(102, 126, 234, 0.5);
}

QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border: 2px solid rgba(102, 126, 234, 0.8);
    background: rgba(30, 30, 50, 0.9);
}

QLineEdit:hover, QTextEdit:hover, QPlainTextEdit:hover {
    border: 2px solid rgba(102, 126, 234, 0.5);
}

/* ============================================
   TABLES - Modern Grid Style
   ============================================ */

QTableWidget {
    background: rgba(20, 20, 30, 0.8);
    alternate-background-color: rgba(30, 30, 45, 0.6);
    gridline-color: rgba(102, 126, 234, 0.2);
    border: 1px solid rgba(102, 126, 234, 0.3);
    border-radius: 4px;
    selection-background-color: rgba(102, 126, 234, 0.4);
}

QTableWidget::item {
    padding: 8px;
    border: none;
}

QTableWidget::item:selected {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 rgba(102, 126, 234, 0.5),
        stop:1 rgba(118, 75, 162, 0.5));
    color: white;
}

QTableWidget::item:hover {
    background: rgba(102, 126, 234, 0.2);
}

QHeaderView::section {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 rgba(102, 126, 234, 0.3),
        stop:1 rgba(118, 75, 162, 0.3));
    color: white;
    font-weight: bold;
    padding: 10px;
    border: none;
    border-right: 1px solid rgba(255, 255, 255, 0.1);
    border-bottom: 2px solid rgba(102, 126, 234, 0.5);
}

QHeaderView::section:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 rgba(102, 126, 234, 0.5),
        stop:1 rgba(118, 75, 162, 0.5));
}

/* ============================================
   SCROLLBARS - Sleek Modern Design
   ============================================ */

QScrollBar:vertical {
    background: rgba(20, 20, 30, 0.5);
    width: 12px;
    border-radius: 6px;
    margin: 0;
}

QScrollBar::handle:vertical {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 rgba(102, 126, 234, 0.6),
        stop:1 rgba(118, 75, 162, 0.6));
    border-radius: 6px;
    min-height: 30px;
}

QScrollBar::handle:vertical:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 rgba(240, 147, 251, 0.8),
        stop:1 rgba(245, 87, 108, 0.8));
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}

QScrollBar:horizontal {
    background: rgba(20, 20, 30, 0.5);
    height: 12px;
    border-radius: 6px;
    margin: 0;
}

QScrollBar::handle:horizontal {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 rgba(102, 126, 234, 0.6),
        stop:1 rgba(118, 75, 162, 0.6));
    border-radius: 6px;
    min-width: 30px;
}

QScrollBar::handle:horizontal:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 rgba(240, 147, 251, 0.8),
        stop:1 rgba(245, 87, 108, 0.8));
}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0px;
}

/* ============================================
   TABS - Modern Tab Design
   ============================================ */

QTabWidget::pane {
    border: 2px solid rgba(102, 126, 234, 0.3);
    border-radius: 4px;
    background: rgba(20, 20, 30, 0.6);
    top: -2px;
}

QTabBar::tab {
    background: rgba(30, 30, 45, 0.6);
    color: #a8b2d1;
    padding: 10px 20px;
    margin-right: 4px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    font-weight: 600;
}

QTabBar::tab:selected {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 rgba(102, 126, 234, 0.8),
        stop:1 rgba(118, 75, 162, 0.8));
    color: white;
}

QTabBar::tab:hover:!selected {
    background: rgba(102, 126, 234, 0.3);
}

/* ============================================
   SPLITTERS - Subtle Handles
   ============================================ */

QSplitter::handle {
    background: rgba(102, 126, 234, 0.2);
}

QSplitter::handle:hover {
    background: rgba(102, 126, 234, 0.5);
}

QSplitter::handle:horizontal {
    width: 3px;
}

QSplitter::handle:vertical {
    height: 3px;
}

/* ============================================
   SLIDERS - Modern Gradient Handles
   ============================================ */

QSlider::groove:horizontal {
    background: rgba(50, 50, 65, 0.6);
    height: 6px;
    border-radius: 3px;
}

QSlider::handle:horizontal {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #667eea, stop:1 #764ba2);
    width: 18px;
    height: 18px;
    margin: -6px 0;
    border-radius: 9px;
    border: 2px solid white;
}

QSlider::handle:horizontal:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #f093fb, stop:1 #f5576c);
}

QSlider::groove:vertical {
    background: rgba(50, 50, 65, 0.6);
    width: 6px;
    border-radius: 3px;
}

QSlider::handle:vertical {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #667eea, stop:1 #764ba2);
    width: 18px;
    height: 18px;
    margin: 0 -6px;
    border-radius: 9px;
    border: 2px solid white;
}

QSlider::handle:vertical:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #f093fb, stop:1 #f5576c);
}

/* ============================================
   MENU BAR - Sleek Top Menu
   ============================================ */

QMenuBar {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 rgba(26, 26, 46, 0.95),
        stop:1 rgba(22, 33, 62, 0.95));
    border-bottom: 2px solid rgba(102, 126, 234, 0.3);
    padding: 4px;
}

QMenuBar::item {
    background: transparent;
    color: #e6e6e6;
    padding: 8px 12px;
    border-radius: 4px;
}

QMenuBar::item:selected {
    background: rgba(102, 126, 234, 0.3);
}

QMenuBar::item:pressed {
    background: rgba(102, 126, 234, 0.5);
}

QMenu {
    background: rgba(30, 30, 45, 0.95);
    border: 1px solid rgba(102, 126, 234, 0.4);
    border-radius: 4px;
    padding: 8px;
}

QMenu::item {
    padding: 8px 30px 8px 30px;
    border-radius: 4px;
    color: #e6e6e6;
}

QMenu::item:selected {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 rgba(102, 126, 234, 0.5),
        stop:1 rgba(118, 75, 162, 0.5));
}

QMenu::separator {
    height: 1px;
    background: rgba(102, 126, 234, 0.3);
    margin: 4px 16px;
}

/* ============================================
   STATUS BAR - Bottom Info Bar
   ============================================ */

QStatusBar {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 rgba(26, 26, 46, 0.9),
        stop:1 rgba(22, 33, 62, 0.9));
    border-top: 2px solid rgba(102, 126, 234, 0.3);
    color: #a8b2d1;
    padding: 4px;
}

/* ============================================
   PROGRESS BARS - Gradient Fill
   ============================================ */

QProgressBar {
    background: rgba(30, 30, 45, 0.6);
    border: 2px solid rgba(102, 126, 234, 0.3);
    border-radius: 4px;
    text-align: center;
    color: white;
    font-weight: bold;
    height: 24px;
}

QProgressBar::chunk {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #667eea, stop:1 #764ba2);
    border-radius: 3px;
}

/* ============================================
   COMBO BOX - Dropdown Style
   ============================================ */

QComboBox {
    background: rgba(30, 30, 45, 0.7);
    border: 2px solid rgba(102, 126, 234, 0.3);
    border-radius: 4px;
    padding: 6px 12px;
    min-height: 28px;
}

QComboBox:hover {
    border: 2px solid rgba(102, 126, 234, 0.6);
}

QComboBox::drop-down {
    border: none;
    width: 30px;
}

QComboBox::down-arrow {
    image: none;
    border-left: 5px solid transparent;
    border-right: 5px solid transparent;
    border-top: 5px solid #667eea;
    margin-right: 8px;
}

QComboBox QAbstractItemView {
    background: rgba(30, 30, 45, 0.95);
    border: 2px solid rgba(102, 126, 234, 0.5);
    border-radius: 4px;
    selection-background-color: rgba(102, 126, 234, 0.5);
}

/* ============================================
   CHECKBOXES & RADIO BUTTONS
   ============================================ */

QCheckBox, QRadioButton {
    spacing: 8px;
    color: #e6e6e6;
}

QCheckBox::indicator, QRadioButton::indicator {
    width: 18px;
    height: 18px;
    border: 2px solid rgba(102, 126, 234, 0.5);
    border-radius: 4px;
    background: rgba(30, 30, 45, 0.6);
}

QCheckBox::indicator:checked {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #667eea, stop:1 #764ba2);
}

QRadioButton::indicator {
    border-radius: 9px;
}

/* ============================================
   LABELS - Enhanced Typography
   ============================================ */

QLabel {
    color: #e6e6e6;
    background: transparent;
}

QPushButton#primary {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #E91E63, stop:1 #F06292);
    min-height: 36px;
    font-size: 14px;
}

QPushButton#success {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
        stop:0 #4CAF50, stop:1 #388E3C);
}

/* ============================================
   TOOLTIPS - Floating Info Boxes
   ============================================ */

QToolTip {
    background: rgba(30, 30, 45, 0.98);
    color: white;
    border: 2px solid rgba(102, 126, 234, 0.6);
    border-radius: 4px;
    padding: 8px 12px;
    font-size: 12px;
}
"""

def get_modern_stylesheet():
    """Get the modern dark theme stylesheet"""
    return MODERN_DARK_THEME
