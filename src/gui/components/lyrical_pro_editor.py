"""
Lyrical Pro Editor Widget
A stunning teleprompter-mode lyrics editor inspired by the Lyrical Pro design.
Features a warm cream background, large scrolling lyrics, and floating panels.
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QTextEdit, QFrame, QSizePolicy, QSlider, QScrollArea,
    QGraphicsDropShadowEffect, QApplication
)
from PyQt6.QtCore import (
    Qt, QTimer, QRectF, QPointF, QPropertyAnimation,
    QEasingCurve, pyqtSignal, QTime
)
from PyQt6.QtGui import (
    QPainter, QFont, QColor, QPen, QBrush, QPainterPath,
    QFontDatabase, QLinearGradient, QRadialGradient, QPalette
)
import math
import logging

logger = logging.getLogger(__name__)

# ─── Color Palette (Lyrical Pro) ───────────────────────────────────────────────
CREAM_BG        = "#f5ede0"   # Warm cream background
ACTIVE_RUST     = "#c0522a"   # Terracotta / rust-red for active lyric
ACTIVE_BOX_BG   = "#f7ebe0"   # Slightly lighter cream for active box bg
ACTIVE_BOX_BORD = "#e8c5aa"   # Warm border for active box
PREV_GRAY       = "#c8bfb5"   # Faded gray for previous lines
NEXT_GRAY       = "#8a7e74"   # Medium gray for next line
TOP_BAR_BG      = "#f0e6d3"   # Slightly darker cream for top bar
TOP_BAR_BORDER  = "#e0d0bc"   # Top bar border
PANEL_BG        = "#faf5ee"   # Floating panel background
PANEL_BORDER    = "#e8ddd0"   # Panel border
ACCENT_RUST     = "#c0522a"   # Accent color
BTN_DARK        = "#8b7355"   # Dark tan button
TEXT_PRIMARY    = "#3d2f22"   # Dark brown text
TEXT_SECONDARY  = "#8a7e74"   # Muted text
STATUS_GREEN    = "#4a9e6b"   # Active status indicator


class TeleprompterLyricsWidget(QWidget):
    """
    The main lyrics display area – shows prev/current/next lines in
    gorgeous teleprompter style.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self.prev_line_text  = ""
        self.prev2_line_text = ""   # two lines back
        self.current_text    = "NEON LIGHTS SHIMMERING"
        self.next_line_text  = "In the deep of the night"
        self.next2_line_text = "Waiting for the pulse to ignite"

        # Font setup
        self._load_fonts()

        self.setMinimumHeight(400)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.setAttribute(Qt.WidgetAttribute.WA_OpaquePaintEvent)

    def _load_fonts(self):
        self.big_font    = QFont("Arial Black", 72, QFont.Weight.Black)
        self.big_font.setItalic(True)
        self.big_font.setStyleHint(QFont.StyleHint.SansSerif)

        self.med_font    = QFont("Arial", 36, QFont.Weight.Bold)
        self.med_font.setStyleHint(QFont.StyleHint.SansSerif)

        self.small_font  = QFont("Arial", 28, QFont.Weight.Normal)
        self.small_font.setStyleHint(QFont.StyleHint.SansSerif)

    def set_lines(self, prev2: str, prev: str, current: str, next1: str, next2: str):
        self.prev2_line_text = prev2
        self.prev_line_text  = prev
        self.current_text    = current
        self.next_line_text  = next1
        self.next2_line_text = next2
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)

        rect = self.rect()
        w = rect.width()
        h = rect.height()

        # ── Background ──────────────────────────────────────────────────────
        painter.fillRect(rect, QColor(CREAM_BG))

        # ── Layout zones ─────────────────────────────────────────────────────
        # prev2: top ~10%
        # prev:  ~22%
        # active: 35%–65% (center area)
        # next:  ~72%
        # next2: ~84%

        # ── Draw prev2 (very faint) ──────────────────────────────────────────
        if self.prev2_line_text:
            painter.setFont(self.small_font)
            painter.setPen(QColor(PREV_GRAY))
            painter.setOpacity(0.50)
            y2 = int(h * 0.08)
            painter.drawText(
                QRectF(40, y2, w - 80, h * 0.12),
                Qt.AlignmentFlag.AlignCenter | Qt.AlignmentFlag.AlignVCenter,
                self.prev2_line_text
            )
            painter.setOpacity(1.0)

        # ── Draw prev line ───────────────────────────────────────────────────
        if self.prev_line_text:
            painter.setFont(self.med_font)
            painter.setPen(QColor(PREV_GRAY))
            painter.setOpacity(0.70)
            y1 = int(h * 0.20)
            painter.drawText(
                QRectF(40, y1, w - 80, h * 0.14),
                Qt.AlignmentFlag.AlignCenter | Qt.AlignmentFlag.AlignVCenter,
                self.prev_line_text
            )
            painter.setOpacity(1.0)

        # ── Draw active line box ─────────────────────────────────────────────
        box_margin  = 60
        box_top     = int(h * 0.34)
        box_height  = int(h * 0.32)
        box_rect    = QRectF(box_margin, box_top, w - 2 * box_margin, box_height)

        # Box fill
        box_bg = QColor(ACTIVE_BOX_BG)
        painter.setBrush(QBrush(box_bg))
        painter.setPen(QPen(QColor(ACTIVE_BOX_BORD), 2))
        painter.drawRoundedRect(box_rect, 24, 24)

        # Active text inside box
        if self.current_text:
            active_font = QFont("Arial Black", 64, QFont.Weight.Black)
            active_font.setItalic(True)
            active_font.setStyleHint(QFont.StyleHint.SansSerif)

            painter.setFont(active_font)
            painter.setPen(QColor(ACTIVE_RUST))
            painter.setBrush(Qt.BrushStyle.NoBrush)

            # Scale down to fit
            fm = painter.fontMetrics()
            text_w = fm.horizontalAdvance(self.current_text)
            avail_w = box_rect.width() - 60
            if text_w > avail_w:
                scale = avail_w / text_w
                sz = max(24, int(64 * scale))
                active_font.setPointSize(sz)
                painter.setFont(active_font)

            painter.drawText(box_rect, Qt.AlignmentFlag.AlignCenter, self.current_text)

        # ── Draw next line ───────────────────────────────────────────────────
        if self.next_line_text:
            painter.setFont(self.med_font)
            painter.setPen(QColor(NEXT_GRAY))
            y3 = int(h * 0.68)
            painter.drawText(
                QRectF(40, y3, w - 80, h * 0.14),
                Qt.AlignmentFlag.AlignCenter | Qt.AlignmentFlag.AlignVCenter,
                self.next_line_text
            )

        # ── Draw next2 line (faded italic) ──────────────────────────────────
        if self.next2_line_text:
            f2 = QFont("Arial", 26, QFont.Weight.Normal)
            f2.setItalic(True)
            painter.setFont(f2)
            painter.setPen(QColor(PREV_GRAY))
            painter.setOpacity(0.55)
            y4 = int(h * 0.83)
            painter.drawText(
                QRectF(40, y4, w - 80, h * 0.14),
                Qt.AlignmentFlag.AlignCenter | Qt.AlignmentFlag.AlignVCenter,
                self.next2_line_text
            )
            painter.setOpacity(1.0)


class FloatingPanel(QFrame):
    """Base class for the floating overlay panels."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("FloatingPanel")
        self.setStyleSheet(f"""
            #FloatingPanel {{
                background-color: {PANEL_BG};
                border: 1.5px solid {PANEL_BORDER};
                border-radius: 14px;
            }}
        """)
        shadow = QGraphicsDropShadowEffect()
        shadow.setBlurRadius(24)
        shadow.setOffset(0, 4)
        shadow.setColor(QColor(0, 0, 0, 40))
        self.setGraphicsEffect(shadow)


class VocalFxPanel(FloatingPanel):
    """Vocal FX Rack floating panel (bottom-left)."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(260, 170)
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 10, 14, 12)
        layout.setSpacing(6)

        # Header
        header = QHBoxLayout()
        title_lbl = QLabel("VOCAL FX RACK")
        title_lbl.setStyleSheet(f"""
            font-family: 'Arial';
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1.5px;
            color: {TEXT_SECONDARY};
        """)
        mic_lbl = QLabel("🎤")
        mic_lbl.setStyleSheet("font-size: 14px;")
        header.addWidget(title_lbl)
        header.addStretch()
        header.addWidget(mic_lbl)
        layout.addLayout(header)

        # Separator
        sep = QFrame()
        sep.setFrameShape(QFrame.Shape.HLine)
        sep.setStyleSheet(f"background: {PANEL_BORDER}; max-height: 1px;")
        layout.addWidget(sep)

        # Auto-Tune row
        at_layout = QHBoxLayout()
        at_lbl = QLabel("AUTO-TUNE")
        at_lbl.setStyleSheet(f"font-size: 10px; font-weight: 600; color: {TEXT_SECONDARY}; letter-spacing: 0.8px;")
        at_pct = QLabel("85%")
        at_pct.setStyleSheet(f"font-size: 10px; font-weight: 700; color: {ACTIVE_RUST};")
        at_layout.addWidget(at_lbl)
        at_layout.addStretch()
        at_layout.addWidget(at_pct)
        layout.addLayout(at_layout)

        at_slider = QSlider(Qt.Orientation.Horizontal)
        at_slider.setRange(0, 100)
        at_slider.setValue(85)
        at_slider.setStyleSheet(self._slider_style(ACTIVE_RUST))
        layout.addWidget(at_slider)

        # Space Echo row
        se_layout = QHBoxLayout()
        se_lbl = QLabel("SPACE ECHO")
        se_lbl.setStyleSheet(f"font-size: 10px; font-weight: 600; color: {TEXT_SECONDARY}; letter-spacing: 0.8px;")
        se_active = QLabel("ACTIVE")
        se_active.setStyleSheet(f"font-size: 9px; font-weight: 700; color: {STATUS_GREEN};")
        se_layout.addWidget(se_lbl)
        se_layout.addStretch()
        se_layout.addWidget(se_active)
        layout.addLayout(se_layout)

        se_slider = QSlider(Qt.Orientation.Horizontal)
        se_slider.setRange(0, 100)
        se_slider.setValue(45)
        se_slider.setStyleSheet(self._slider_style(ACTIVE_RUST))
        layout.addWidget(se_slider)

        # Expand button
        expand_btn = QPushButton("EXPAND FX CHAIN")
        expand_btn.setFixedHeight(28)
        expand_btn.setStyleSheet(f"""
            QPushButton {{
                background: transparent;
                border: 1.5px solid {PANEL_BORDER};
                border-radius: 6px;
                font-size: 9px;
                font-weight: 700;
                letter-spacing: 1px;
                color: {TEXT_SECONDARY};
                padding: 0 8px;
            }}
            QPushButton:hover {{
                background: {ACTIVE_BOX_BG};
                border-color: {ACTIVE_RUST};
                color: {ACTIVE_RUST};
            }}
        """)
        layout.addWidget(expand_btn)

    @staticmethod
    def _slider_style(accent: str) -> str:
        return f"""
            QSlider::groove:horizontal {{
                background: #e5d8c8;
                height: 4px;
                border-radius: 2px;
            }}
            QSlider::handle:horizontal {{
                background: {accent};
                width: 12px;
                height: 12px;
                margin: -4px 0;
                border-radius: 6px;
            }}
            QSlider::sub-page:horizontal {{
                background: {accent};
                border-radius: 2px;
            }}
        """


class PlaybackBar(FloatingPanel):
    """Playback controls bar (bottom-center)."""

    play_pause_clicked = pyqtSignal()
    rewind_clicked     = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(220, 70)
        self._is_playing = False
        self._build_ui()

    def _build_ui(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(20, 12, 20, 12)
        layout.setSpacing(16)

        # Rewind button
        rw_btn = QPushButton("⏪")
        rw_btn.setFixedSize(36, 36)
        rw_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        rw_btn.setStyleSheet(self._icon_btn_style())
        rw_btn.clicked.connect(self.rewind_clicked)
        layout.addWidget(rw_btn)

        # Play/Pause button (big circle)
        self.pp_btn = QPushButton("⏸" if self._is_playing else "▶")
        self.pp_btn.setFixedSize(46, 46)
        self.pp_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.pp_btn.setStyleSheet(f"""
            QPushButton {{
                background: {BTN_DARK};
                border: none;
                border-radius: 23px;
                font-size: 16px;
                color: white;
            }}
            QPushButton:hover {{
                background: {ACTIVE_RUST};
            }}
            QPushButton:pressed {{
                background: #9a3d1a;
            }}
        """)
        self.pp_btn.clicked.connect(self._on_play_pause)
        layout.addWidget(self.pp_btn)

        # Record/status dot
        dot_lbl = QLabel("●")
        dot_lbl.setStyleSheet(f"font-size: 14px; color: {ACTIVE_RUST};")
        layout.addWidget(dot_lbl)

    def _on_play_pause(self):
        self._is_playing = not self._is_playing
        self.pp_btn.setText("⏸" if self._is_playing else "▶")
        self.play_pause_clicked.emit()

    def set_playing(self, playing: bool):
        self._is_playing = playing
        self.pp_btn.setText("⏸" if playing else "▶")

    @staticmethod
    def _icon_btn_style() -> str:
        return f"""
            QPushButton {{
                background: transparent;
                border: none;
                border-radius: 18px;
                font-size: 18px;
                color: {TEXT_SECONDARY};
            }}
            QPushButton:hover {{
                color: {ACTIVE_RUST};
                background: {ACTIVE_BOX_BG};
            }}
        """


class LyricEditorPanel(FloatingPanel):
    """Lyric editor panel (bottom-right) – show current verse editable."""

    end_session_clicked = pyqtSignal()
    text_changed        = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(250, 260)
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 10, 14, 14)
        layout.setSpacing(6)

        # Header row
        header = QHBoxLayout()
        icon_lbl = QLabel("⚡")
        icon_lbl.setStyleSheet("font-size: 12px;")
        title_lbl = QLabel("LYRIC EDITOR PANEL")
        title_lbl.setStyleSheet(f"""
            font-size: 9px;
            font-weight: 700;
            letter-spacing: 1.5px;
            color: {TEXT_SECONDARY};
        """)
        tag_lbl = QLabel("ACTIVE VERSE")
        tag_lbl.setStyleSheet(f"""
            font-size: 8px;
            font-weight: 700;
            letter-spacing: 1px;
            color: {ACTIVE_RUST};
        """)
        header.addWidget(icon_lbl)
        header.addWidget(title_lbl)
        header.addStretch()
        header.addWidget(tag_lbl)
        layout.addLayout(header)

        # Separator
        sep = QFrame()
        sep.setFrameShape(QFrame.Shape.HLine)
        sep.setStyleSheet(f"background: {PANEL_BORDER}; max-height: 1px;")
        layout.addWidget(sep)

        # Active verse text (orange / rust title)
        self.verse_title = QLabel("NEON LIGHTS SHIMMERING")
        self.verse_title.setStyleSheet(f"""
            font-family: 'Arial Black';
            font-size: 12px;
            font-weight: 900;
            color: {ACTIVE_RUST};
            letter-spacing: 0.5px;
        """)
        self.verse_title.setWordWrap(True)
        layout.addWidget(self.verse_title)

        # Separator
        sep2 = QFrame()
        sep2.setFrameShape(QFrame.Shape.HLine)
        sep2.setStyleSheet(f"background: {PANEL_BORDER}; max-height: 1px;")
        layout.addWidget(sep2)

        # Text edit area
        self.text_edit = QTextEdit()
        self.text_edit.setPlaceholderText("Edit lyrics here...")
        self.text_edit.textChanged.connect(lambda: self.text_changed.emit(self.text_edit.toPlainText()))
        self.text_edit.setStyleSheet(f"""
            QTextEdit {{
                background: transparent;
                border: none;
                font-family: 'Arial';
                font-size: 12px;
                color: {TEXT_PRIMARY};
                padding: 2px;
            }}
        """)
        self.text_edit.setMinimumHeight(60)
        layout.addWidget(self.text_edit)

        # Formatting row + autosave
        fmt_layout = QHBoxLayout()
        b_btn = QPushButton("B")
        b_btn.setFixedSize(26, 22)
        b_btn.setStyleSheet(self._fmt_btn_style(bold=True))
        i_btn = QPushButton("I")
        i_btn.setFixedSize(26, 22)
        i_btn.setStyleSheet(self._fmt_btn_style(italic=True))
        autosave = QLabel("AUTOSAVED 12:04")
        autosave.setStyleSheet(f"font-size: 8px; color: {TEXT_SECONDARY}; letter-spacing: 0.5px;")
        fmt_layout.addWidget(b_btn)
        fmt_layout.addWidget(i_btn)
        fmt_layout.addStretch()
        fmt_layout.addWidget(autosave)
        layout.addLayout(fmt_layout)

        # End session button
        end_btn = QPushButton("END SESSION")
        end_btn.setFixedHeight(36)
        end_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        end_btn.setStyleSheet(f"""
            QPushButton {{
                background: {BTN_DARK};
                border: none;
                border-radius: 8px;
                font-size: 10px;
                font-weight: 700;
                letter-spacing: 1.5px;
                color: white;
            }}
            QPushButton:hover {{
                background: {ACTIVE_RUST};
            }}
            QPushButton:pressed {{
                background: #9a3d1a;
            }}
        """)
        end_btn.clicked.connect(self.end_session_clicked)
        layout.addWidget(end_btn)

    @staticmethod
    def _fmt_btn_style(bold=False, italic=False) -> str:
        return f"""
            QPushButton {{
                background: transparent;
                border: 1.5px solid {PANEL_BORDER};
                border-radius: 4px;
                font-weight: {'700' if bold else '400'};
                font-style: {'italic' if italic else 'normal'};
                font-size: 11px;
                color: {TEXT_SECONDARY};
            }}
            QPushButton:hover {{
                background: {ACTIVE_BOX_BG};
                border-color: {ACTIVE_RUST};
                color: {ACTIVE_RUST};
            }}
        """

    def set_verse(self, title: str, text: str):
        self.verse_title.setText(title.upper())
        self.text_edit.setPlainText(text)


class TopBar(QWidget):
    """Top navigation bar: Logo, time, BPM, key."""

    back_clicked = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedHeight(58)
        self.setStyleSheet(f"""
            QWidget {{
                background-color: {TOP_BAR_BG};
                border-bottom: 1.5px solid {TOP_BAR_BORDER};
            }}
        """)
        self._position_ms = 0
        self._duration_ms = 222000  # default 3:42
        self._bpm         = 128
        self._key         = "F# MINOR"
        self._build_ui()

        # Tick timer
        self._tick = QTimer(self)
        self._tick.setInterval(500)
        self._tick.timeout.connect(self._update_time)
        self._tick.start()

    def _build_ui(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(16, 0, 16, 0)
        layout.setSpacing(12)

        # Logo block
        logo_widget = QWidget()
        logo_widget.setStyleSheet("background: transparent; border: none;")
        logo_layout = QVBoxLayout(logo_widget)
        logo_layout.setContentsMargins(0, 0, 0, 0)
        logo_layout.setSpacing(0)

        logo_top = QHBoxLayout()
        logo_icon = QLabel("▣")
        logo_icon.setStyleSheet(f"font-size: 18px; color: {BTN_DARK};")
        logo_name = QLabel("LYRICAL PRO")
        logo_name.setStyleSheet(f"""
            font-family: 'Arial Black';
            font-size: 13px;
            font-weight: 900;
            color: {TEXT_PRIMARY};
            letter-spacing: 1px;
            background: transparent;
        """)
        logo_top.addWidget(logo_icon)
        logo_top.addWidget(logo_name)
        logo_layout.addLayout(logo_top)

        logo_sub = QLabel("TELEPROMPTER MODE")
        logo_sub.setStyleSheet(f"""
            font-size: 8px;
            font-weight: 600;
            letter-spacing: 2px;
            color: {TEXT_SECONDARY};
            background: transparent;
        """)
        logo_layout.addWidget(logo_sub)
        layout.addWidget(logo_widget)
        layout.addStretch()

        # Time display
        self.time_lbl = QLabel("03:42.15")
        self.time_lbl.setStyleSheet(f"""
            font-family: 'Courier New', monospace;
            font-size: 15px;
            font-weight: 700;
            color: {TEXT_PRIMARY};
            background: transparent;
        """)
        layout.addWidget(self._make_pill(self.time_lbl, has_dot=True))

        # BPM pill
        bpm_lbl = QLabel(f"{self._bpm} BPM")
        bpm_lbl.setStyleSheet(f"""
            font-size: 12px;
            font-weight: 600;
            color: {TEXT_PRIMARY};
            background: transparent;
        """)
        layout.addWidget(self._make_pill(bpm_lbl))

        # Key pill
        key_lbl = QLabel(self._key)
        key_lbl.setStyleSheet(f"""
            font-size: 12px;
            font-weight: 600;
            color: {TEXT_PRIMARY};
            background: transparent;
        """)
        layout.addWidget(self._make_pill(key_lbl))

        layout.addStretch()

        # Back button
        back_btn = QPushButton("✕ Exit Teleprompter")
        back_btn.setStyleSheet(f"""
            QPushButton {{
                background: transparent;
                border: 1.5px solid {PANEL_BORDER};
                border-radius: 8px;
                font-size: 11px;
                font-weight: 600;
                color: {TEXT_SECONDARY};
                padding: 6px 14px;
            }}
            QPushButton:hover {{
                background: {ACTIVE_BOX_BG};
                border-color: {ACTIVE_RUST};
                color: {ACTIVE_RUST};
            }}
        """)
        back_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        back_btn.clicked.connect(self.back_clicked)
        layout.addWidget(back_btn)

    def _make_pill(self, label_widget: QLabel, has_dot: bool = False) -> QWidget:
        container = QWidget()
        container.setStyleSheet(f"""
            QWidget {{
                background: {PANEL_BG};
                border: 1.5px solid {PANEL_BORDER};
                border-radius: 20px;
            }}
        """)
        cl = QHBoxLayout(container)
        cl.setContentsMargins(12, 6, 12, 6)
        cl.setSpacing(6)
        if has_dot:
            dot = QLabel("●")
            dot.setStyleSheet(f"color: {ACTIVE_RUST}; font-size: 10px; background: transparent;")
            cl.addWidget(dot)
        cl.addWidget(label_widget)
        return container

    def _update_time(self):
        pass  # Time is updated externally via set_position

    def set_position(self, ms: int):
        self._position_ms = ms
        total_s = ms // 1000
        mins    = total_s // 60
        secs    = total_s % 60
        frac    = (ms % 1000) // 10
        self.time_lbl.setText(f"{mins:02}:{secs:02}.{frac:02}")

    def set_bpm(self, bpm: int):
        self._bpm = bpm

    def set_key(self, key: str):
        self._key = key


class LyricalProEditorWidget(QWidget):
    """
    Full-screen Lyrical Pro teleprompter editor.
    Designed to exactly match the reference screenshot.
    """

    close_session = pyqtSignal()  # Emitted when user exits teleprompter mode

    def __init__(self, player=None, lyrics_data=None, project=None, parent=None):
        super().__init__(parent)
        self.player      = player
        self.lyrics_data = lyrics_data
        self.project     = project

        self._current_line_idx = -1

        self.setStyleSheet(f"background-color: {CREAM_BG};")
        self._build_ui()
        self._connect_signals()

        # Refresh timer for line sync
        self._sync_timer = QTimer(self)
        self._sync_timer.setInterval(100)
        self._sync_timer.timeout.connect(self._sync_lyrics_position)
        self._sync_timer.start()

        # Fake some demo data if no lyrics
        if not lyrics_data or not lyrics_data.lines:
            self._load_demo_content()

    # ─── UI Construction ─────────────────────────────────────────────────────

    def _build_ui(self):
        root_layout = QVBoxLayout(self)
        root_layout.setContentsMargins(0, 0, 0, 0)
        root_layout.setSpacing(0)

        # ── Top bar ──────────────────────────────────────────────────────────
        self.top_bar = TopBar(self)
        root_layout.addWidget(self.top_bar)

        # ── Content area (lyrics + floating panels) ──────────────────────────
        self.content_area = QWidget(self)
        self.content_area.setStyleSheet(f"background-color: {CREAM_BG};")
        self.content_layout = QVBoxLayout(self.content_area)
        self.content_layout.setContentsMargins(0, 0, 0, 0)
        self.content_layout.setSpacing(0)

        # Lyrics display
        self.lyrics_widget = TeleprompterLyricsWidget(self.content_area)
        self.content_layout.addWidget(self.lyrics_widget, 1)

        root_layout.addWidget(self.content_area, 1)

        # ── Floating panels (overlaid via absolute positioning after show) ───
        self.fx_panel     = VocalFxPanel(self)
        self.playback_bar = PlaybackBar(self)
        self.editor_panel = LyricEditorPanel(self)

        # Position them; will be re-positioned on resize too
        self._reposition_panels()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._reposition_panels()

    def _reposition_panels(self):
        w = self.width()
        h = self.height()
        if w == 0 or h == 0:
            return

        bottom_y = h - 200  # panels sit ~200px from bottom

        # FX rack – bottom-left
        self.fx_panel.move(30, bottom_y)
        self.fx_panel.raise_()

        # Playback bar – bottom-center
        pb_x = (w - self.playback_bar.width()) // 2
        pb_y = h - self.playback_bar.height() - 30
        self.playback_bar.move(pb_x, pb_y)
        self.playback_bar.raise_()

        # Lyric editor panel – bottom-right
        ep_x = w - self.editor_panel.width() - 30
        ep_y = h - self.editor_panel.height() - 30
        self.editor_panel.move(ep_x, ep_y)
        self.editor_panel.raise_()

    # ─── Signals ─────────────────────────────────────────────────────────────

    def _connect_signals(self):
        self.top_bar.back_clicked.connect(self.close_session)
        self.editor_panel.end_session_clicked.connect(self.close_session)
        self.playback_bar.play_pause_clicked.connect(self._toggle_playback)
        self.playback_bar.rewind_clicked.connect(self._rewind)

        if self.player:
            self.player.position_changed.connect(self._on_position_changed)
            self.player.state_changed.connect(self.playback_bar.set_playing)

    # ─── Playback Callbacks ───────────────────────────────────────────────────

    def _toggle_playback(self):
        if self.player:
            self.player.toggle_playback()

    def _rewind(self):
        if self.player:
            self.player.seek(max(0, self.player.media_player.position() - 10000))

    def _on_position_changed(self, ms: int):
        self.top_bar.set_position(ms)
        self._sync_lyrics_at(ms / 1000.0)

    # ─── Lyrics sync ─────────────────────────────────────────────────────────

    def _sync_lyrics_position(self):
        """Periodically sync if player is attached."""
        if self.player and hasattr(self.player, 'media_player'):
            ms = self.player.media_player.position()
            self._sync_lyrics_at(ms / 1000.0)

    def _sync_lyrics_at(self, seconds: float):
        """Find and display correct lyrics for given time."""
        if not self.lyrics_data or not self.lyrics_data.lines:
            return

        lines = self.lyrics_data.lines
        active_idx = -1
        for i, line in enumerate(lines):
            if line.start_time <= seconds < line.end_time:
                active_idx = i
                break
        # If past last line, identify the line we're closest to
        if active_idx == -1:
            for i in range(len(lines) - 1, -1, -1):
                if seconds >= lines[i].start_time:
                    active_idx = i
                    break

        if active_idx == self._current_line_idx:
            return  # No change
        self._current_line_idx = active_idx

        if active_idx == -1:
            self.lyrics_widget.set_lines("", "", "", "", "")
            return

        def safe(idx):
            return lines[idx].text if 0 <= idx < len(lines) else ""

        self.lyrics_widget.set_lines(
            safe(active_idx - 2),
            safe(active_idx - 1),
            safe(active_idx),
            safe(active_idx + 1),
            safe(active_idx + 2),
        )

        # Update editor panel
        self.editor_panel.set_verse(safe(active_idx), safe(active_idx))

    # ─── Public update API ───────────────────────────────────────────────────

    def update_lyrics(self, lyrics_data):
        """Called when lyrics data changes externally."""
        self.lyrics_data = lyrics_data
        self._current_line_idx = -1

    def set_bpm_key(self, bpm: int, key: str):
        self.top_bar.set_bpm(bpm)
        self.top_bar.set_key(key)

    # ─── Demo content (when no lyrics loaded) ────────────────────────────────

    def _load_demo_content(self):
        self.lyrics_widget.set_lines(
            "Searching for a reason to stay",
            "In the shadows of the digital rain",
            "NEON LIGHTS SHIMMERING",
            "In the deep of the night",
            "Waiting for the pulse to ignite",
        )
        self.editor_panel.set_verse(
            "NEON LIGHTS SHIMMERING",
            "Neon lights shimmering\nIn the deep of the night"
        )
