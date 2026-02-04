"""
Syllable Editor Widget for NC-KTV
Allows fine-tuning of individual word/syllable timings using a piano-roll style interface.
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QScrollArea,
    QPushButton, QLabel, QSlider, QCheckBox
)
from PyQt6.QtCore import Qt, pyqtSignal, QRectF, QPointF, QPoint
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QFont, QPainterPath, QTransform

from core.lyrics import LyricsData, LyricsLine, LyricsToken
import logging

logger = logging.getLogger(__name__)

class SyllableEditorWidget(QWidget):
    """Widget for fine-tuning syllable timings"""
    
    # Signal when data changes (to dirty key)
    data_changed = pyqtSignal()
    # Signal for scrubbing
    seek_requested = pyqtSignal(int)
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        self.lyrics_data: LyricsData = None
        self.waveform_data = None
        self.waveform_duration = 0
        
        # View state
        self.pixels_per_second = 100.0
        self.track_height = 60
        self.current_time = 0.0 # Seconds
        
        self.selected_token: LyricsToken = None
        
        # Performance optimization: Track scrubbing state
        self._last_time_update = 0.0
        self._is_scrubbing = False
        self._scrub_threshold = 0.5  # More than 0.5s jump = scrubbing
        
        self._init_ui()

    def set_position(self, position_ms):
        """Update playhead position (ms) with throttling and scrub detection"""
        new_time = position_ms / 1000.0
        
        # Detect scrubbing (large time jumps)
        time_diff = abs(new_time - self.current_time)
        self._is_scrubbing = time_diff > self._scrub_threshold
        
        self.current_time = new_time
        self._last_time_update = new_time
        
        # Throttle updates to reduce repaint frequency
        # Only update every Nth call (configurable)
        self.canvas._update_throttle = (self.canvas._update_throttle + 1) % 3
        if self.canvas._update_throttle != 0:
            # Skip this update for performance
            return
        
        # Request minimal update (just the playhead area if possible)
        pps = self.pixels_per_second
        playhead_x = int(self.current_time * pps)
        
        # Only update a narrow strip around the playhead
        old_x = self.canvas._last_playhead_x
        if old_x >= 0:
            # Update old and new playhead positions
            update_width = 20  # pixels on each side
            self.canvas.update(old_x - update_width, 0, update_width * 2, self.canvas.height())
            self.canvas.update(playhead_x - update_width, 0, update_width * 2, self.canvas.height())
        else:
            self.canvas.update()
        
        # Auto-scroll logic
        scroll_h = self.scroll_area.horizontalScrollBar()
        visible_w = self.scroll_area.viewport().width()
        current_h = scroll_h.value()
        
        # Center the playhead
        target_h = playhead_x - (visible_w // 2)
        
        # Only scroll if we are "close" to the edge or off-screen?
        # Better: Smooth follow. If playhead goes past 70% of screen, scroll.
        if playhead_x > current_h + (visible_w * 0.8) or playhead_x < current_h:
             scroll_h.setValue(target_h)

        # 2. Vertical Scroll: Ensure Active Line is visible
        if self.lyrics_data and self.lyrics_data.lines:
             row_height = 80
             active_row = -1
             
             # Find active row based on time
             for i, line in enumerate(self.lyrics_data.lines):
                 if line.start_time <= self.current_time <= line.end_time:
                     active_row = i
                     break
                 # Also consider upcoming lines if in "dead space"
                 if line.start_time > self.current_time:
                     # This is the next line, maybe show it?
                     # Let's just track the last known active line or "current segment"
                     active_row = i
                     break
                     
             if active_row >= 0:
                 scroll_v = self.scroll_area.verticalScrollBar()
                 visible_h = self.scroll_area.viewport().height()
                 current_v = scroll_v.value()
                 
                 row_top = active_row * row_height
                 row_bottom = row_top + row_height
                 
                 # If row is outside visible area, scroll to it
                 if row_top < current_v:
                     scroll_v.setValue(row_top)
                 elif row_bottom > current_v + visible_h:
                     scroll_v.setValue(row_bottom - visible_h)

        
    def _init_ui(self):
        """Initialize UI with stunning modern styling"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        
        # Apply modern stylesheet to entire widget
        self.setStyleSheet("""
            QWidget {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 #1a1a2e, stop:1 #16213e);
                color: #ffffff;
                font-family: 'Segoe UI', Arial, sans-serif;
            }
        """)
        
        # Toolbar
        toolbar = self._create_toolbar()
        layout.addWidget(toolbar)
        
        # Scroll Area with custom styling
        self.scroll_area = QScrollArea()
        self.scroll_area.setWidgetResizable(True)
        self.scroll_area.setStyleSheet("""
            QScrollArea {
                border: none;
                background: transparent;
            }
            QScrollBar:vertical {
                background: rgba(255, 255, 255, 0.05);
                width: 12px;
                border-radius: 6px;
            }
            QScrollBar::handle:vertical {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #667eea, stop:1 #764ba2);
                border-radius: 6px;
                min-height: 30px;
            }
            QScrollBar::handle:vertical:hover {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #f093fb, stop:1 #f5576c);
            }
            QScrollBar:horizontal {
                background: rgba(255, 255, 255, 0.05);
                height: 12px;
                border-radius: 6px;
            }
            QScrollBar::handle:horizontal {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #667eea, stop:1 #764ba2);
                border-radius: 6px;
                min-width: 30px;
            }
            QScrollBar::handle:horizontal:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #f093fb, stop:1 #f5576c);
            }
        """)
        
        # Canvas
        self.canvas = SyllableCanvas(self)
        self.scroll_area.setWidget(self.canvas)
        
        layout.addWidget(self.scroll_area)
        
    def _create_toolbar(self):
        """Create stunning modern toolbar"""
        toolbar = QWidget()
        toolbar.setFixedHeight(50)
        toolbar.setStyleSheet("""
            QWidget {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 rgba(102, 126, 234, 0.1),
                    stop:1 rgba(118, 75, 162, 0.1));
                border-bottom: 2px solid rgba(102, 126, 234, 0.3);
                border-radius: 8px;
            }
            QLabel {
                color: #a8b2d1;
                font-size: 13px;
                font-weight: 600;
                background: transparent;
            }
            QSlider::groove:horizontal {
                background: rgba(255, 255, 255, 0.1);
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
                border: 2px solid #ffffff;
            }
            QSlider::handle:horizontal:hover {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 #f093fb, stop:1 #f5576c);
                transform: scale(1.1);
            }
        """)
        
        layout = QHBoxLayout(toolbar)
        layout.setContentsMargins(15, 8, 15, 8)
        
        # Zoom label with icon
        zoom_label = QLabel("🔍 Zoom:")
        layout.addWidget(zoom_label)
        
        self.zoom_slider = QSlider(Qt.Orientation.Horizontal)
        self.zoom_slider.setRange(50, 400)
        self.zoom_slider.setValue(100)
        self.zoom_slider.valueChanged.connect(self._on_zoom_changed)
        self.zoom_slider.setMinimumWidth(150)
        layout.addWidget(self.zoom_slider)
        
        # Zoom value display
        self.zoom_value_label = QLabel("100%")
        self.zoom_value_label.setStyleSheet("""
            QLabel {
                color: #667eea;
                font-weight: bold;
                font-size: 14px;
                padding: 2px 8px;
                background: rgba(102, 126, 234, 0.2);
                border-radius: 8px;
            }
        """)
        self.zoom_slider.valueChanged.connect(lambda v: self.zoom_value_label.setText(f"{v}%"))
        layout.addWidget(self.zoom_value_label)
        
        layout.addSpacing(20)
        
        # Help label
        help_label = QLabel("💡 Tip: Click to seek • Ctrl+Drag to edit timing")
        help_label.setStyleSheet("""
            QLabel {
                color: rgba(168, 178, 209, 0.7);
                font-size: 11px;
                font-style: italic;
                background: transparent;
            }
        """)
        layout.addWidget(help_label)
        
        layout.addStretch()
        return toolbar

    def set_data(self, lyrics_data: LyricsData, waveform_data, duration_ms):
        """Load data into editor"""
        self.lyrics_data = lyrics_data
        self.waveform_data = waveform_data
        self.waveform_duration = duration_ms
        
        # Auto-tokenize if needed
        if self.lyrics_data:
            from core.lyrics import LyricsToken
            
            # Check if we have valid timestamps. If totally empty (all 0),
            # we should layout them out sequentially just for visibility.
            has_valid_times = any(l.end_time > 0 for l in self.lyrics_data.lines)
            default_line_duration = 3.0 # Seconds
            current_default_time = 0.0
            
            for line in self.lyrics_data.lines:
                # If line has no timing, assign default sequential timing
                if not has_valid_times or (line.start_time == 0 and line.end_time == 0):
                    line.start_time = current_default_time
                    line.end_time = current_default_time + default_line_duration
                    current_default_time += default_line_duration
                
                if not line.tokens and line.text:
                    # Naive split
                    words = line.text.split()
                    if not words:
                        continue
                        
                    duration = line.end_time - line.start_time
                    # Safety check for 0 duration (should be covered above, but just in case)
                    if duration <= 0: duration = 2.0
                        
                    word_duration = duration / len(words)
                    
                    current_time = line.start_time
                    for word in words:
                        token = LyricsToken(
                            text=word,
                            start_time=current_time,
                            end_time=current_time + word_duration
                        )
                        line.tokens.append(token)
                        current_time += word_duration
        
        # Resize canvas to fit content
        self._update_canvas_size()
        self.canvas.update()

    def _update_canvas_size(self):
        """Update canvas dimensions based on content"""
        if not self.lyrics_data or not self.lyrics_data.lines:
            return
            
        # Height: based on line count
        num_lines = len(self.lyrics_data.lines)
        row_height = 80
        total_height = max(600, num_lines * row_height + 100) # Minimum height or fit content
        
        # Width: based on duration
        duration_s = self.waveform_duration / 1000.0 if self.waveform_duration > 0 else 60.0
        # Ensure we cover at least the default timestamps we generated
        if self.lyrics_data.lines:
            last_end = max(l.end_time for l in self.lyrics_data.lines)
            duration_s = max(duration_s, last_end + 5.0)
            
        total_width = int(duration_s * self.pixels_per_second)
        
        self.canvas.setFixedSize(total_width, total_height)
        # Invalidate waveform cache since size changed
        self.canvas.invalidate_waveform_cache()
        
    def _on_zoom_changed(self, value):
        self.pixels_per_second = float(value)
        self._update_canvas_size()
        self.canvas.update()



class SyllableCanvas(QWidget):
    """Drawing canvas for syllables"""
    
    def __init__(self, editor: SyllableEditorWidget):
        super().__init__(editor)
        self.editor = editor
        self.setMouseTracking(True)
        
        # Interaction state
        self.dragging = False
        self.drag_mode = None # 'move', 'resize_left', 'resize_right'
        self.drag_start_pos = QPointF()
        self.drag_start_time = 0.0
        self.drag_start_duration = 0.0
        
        # Performance optimizations
        self._waveform_path = None  # Cached waveform path
        self._last_playhead_x = -1  # Track last playhead position for partial updates
        self._update_throttle = 0   # Frame counter for throttling updates
        
    def sizeHint(self):
        if not self.editor.waveform_duration:
            return super().sizeHint()
            
        width = int((self.editor.waveform_duration / 1000.0) * self.editor.pixels_per_second) + 200
        # Determine height based on lines? Or just one long strip? 
        # Making it one long strip for now, maybe wrapping later.
        # Ideally, we stack lines vertically.
        line_count = len(self.editor.lyrics_data.lines) if self.editor.lyrics_data else 0
        height = max(400, line_count * 80 + 100)
        
        from PyQt6.QtCore import QSize
        return QSize(width, height)
        
    def paintEvent(self, event):
        """Paint event with stunning modern visuals"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
        
        # Get the update region (only paint what's needed)
        update_rect = event.rect()
        
        # Stunning gradient background
        from PyQt6.QtGui import QLinearGradient
        gradient = QLinearGradient(0, 0, self.width(), self.height())
        gradient.setColorAt(0, QColor(26, 26, 46))    # #1a1a2e
        gradient.setColorAt(0.5, QColor(22, 33, 62))  # #16213e
        gradient.setColorAt(1, QColor(15, 20, 40))    # Darker
        painter.fillRect(update_rect, QBrush(gradient))
        
        if not self.editor.lyrics_data:
            # Styled empty state
            painter.setPen(QColor(168, 178, 209, 80))
            font = QFont("Segoe UI", 14)
            painter.setFont(font)
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, "🎵 No lyrics loaded")
            return

        # Draw Waveform Background with glow
        if update_rect == self.rect() or self._waveform_path is None:
            try:
                self._draw_waveform(painter)
            except Exception as e:
                print(f"Error drawing waveform: {e}")
        elif self._waveform_path:
            # Draw waveform with gradient and glow
            painter.setPen(Qt.PenStyle.NoPen)
            waveform_gradient = QLinearGradient(0, 0, 0, self.height())
            waveform_gradient.setColorAt(0, QColor(102, 126, 234, 60))   # Top: purple
            waveform_gradient.setColorAt(0.5, QColor(118, 75, 162, 40))   # Middle: darker purple
            waveform_gradient.setColorAt(1, QColor(102, 126, 234, 60))   # Bottom: purple
            painter.setBrush(waveform_gradient)
            painter.drawPath(self._waveform_path)
            
        # Draw Syllable Blocks with effects
        try:
            self._draw_syllables(painter)
        except Exception as e:
            print(f"Error drawing syllables: {e}")
            
        # Draw Playhead with glow effect
        pps = self.editor.pixels_per_second
        playhead_x = int(self.editor.current_time * pps)
        
        if playhead_x != self._last_playhead_x:
            # Glow effect (multiple lines with decreasing opacity)
            for i in range(5, 0, -1):
                glow_color = QColor(255, 100, 150, 20 * i)
                painter.setPen(QPen(glow_color, i * 2))
                painter.drawLine(playhead_x, 0, playhead_x, self.height())
            
            # Main playhead line with gradient
            playhead_gradient = QLinearGradient(0, 0, 0, self.height())
            playhead_gradient.setColorAt(0, QColor(255, 107, 129))    # #ff6b81
            playhead_gradient.setColorAt(0.5, QColor(255, 50, 90))    # Brighter middle
            playhead_gradient.setColorAt(1, QColor(255, 107, 129))    # #ff6b81
            painter.setPen(QPen(QBrush(playhead_gradient), 3))
            painter.drawLine(playhead_x, 0, playhead_x, self.height())
            
            # Draw Playhead Triangle with glow
            try:
                from PyQt6.QtGui import QPolygon
                triangle = QPolygon([
                    QPoint(playhead_x, 0),
                    QPoint(playhead_x - 8, 16),
                    QPoint(playhead_x + 8, 16)
                ])
                
                # Triangle glow
                painter.setPen(QPen(QColor(255, 107, 129, 100), 2))
                gradient_triangle = QLinearGradient(playhead_x, 0, playhead_x, 16)
                gradient_triangle.setColorAt(0, QColor(255, 107, 129))
                gradient_triangle.setColorAt(1, QColor(255, 50, 90))
                painter.setBrush(QBrush(gradient_triangle))
                painter.drawPolygon(triangle)
            except Exception as e:
                print(f"Error drawing playhead: {e}")
            
            self._last_playhead_x = playhead_x


    def _draw_waveform(self, painter: QPainter):
        """Draw waveform background with caching"""
        if self.editor.waveform_data is None or self.editor.waveform_duration <= 0:
            return
            
        data = self.editor.waveform_data
        if len(data) < 2:
            return
        
        # Only regenerate path if cache is invalid
        if self._waveform_path is None:
            pps = self.editor.pixels_per_second
            duration_s = self.editor.waveform_duration / 1000.0
            
            # Canvas dimensions
            height = self.height()
            center_y = height / 2.0
            scale_y = height / 2.0
            
            # Build waveform path once
            path = QPainterPath()
            count = len(data)
            
            # Start at left, center
            path.moveTo(0, center_y)
            
            # Top edge
            for i in range(count):
                t = (i / count) * duration_s
                x = t * pps
                amp = data[i]
                y = center_y - (amp * scale_y)
                path.lineTo(x, y)
                
            # Bottom edge (reverse)
            for i in range(count - 1, -1, -1):
                t = (i / count) * duration_s
                x = t * pps
                amp = data[i]
                y = center_y + (amp * scale_y)
                path.lineTo(x, y)
                
            path.closeSubpath()
            self._waveform_path = path
        
        # Draw cached path
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QColor(100, 200, 255, 40))
        painter.drawPath(self._waveform_path)
    
    def invalidate_waveform_cache(self):
        """Force waveform path to be regenerated on next paint"""
        self._waveform_path = None


    def _draw_syllables(self, painter: QPainter):
        """Draw lyrics with one line per row container (OPTIMIZED with viewport culling)"""
        pps = self.editor.pixels_per_second
        
        row_height = 80
        padding_y = 10
        
        painter.setFont(QFont("Arial", 10))
        
        # VIEWPORT CULLING: Only render visible rows
        # Get visible area from scroll position
        scroll_area = self.editor.scroll_area
        viewport_rect = scroll_area.viewport().rect()
        scroll_y = scroll_area.verticalScrollBar().value()
        scroll_x = scroll_area.horizontalScrollBar().value()
        
        # Calculate visible bounds
        visible_top = scroll_y
        visible_bottom = scroll_y + viewport_rect.height()
        visible_left = scroll_x
        visible_right = scroll_x + viewport_rect.width()
        
        # Calculate which rows are visible (with small margin for smooth scrolling)
        margin = row_height  # One row margin on each side
        first_visible_row = max(0, int((visible_top - margin) / row_height))
        last_visible_row = min(len(self.editor.lyrics_data.lines) - 1, 
                               int((visible_bottom + margin) / row_height))
        
        # Only iterate over visible rows!
        for i in range(first_visible_row, last_visible_row + 1):
            line = self.editor.lyrics_data.lines[i]
            row_y = i * row_height
            
            # Draw Row Container Background (Transparent)
            bg_color = QColor(40, 40, 45, 150) if i % 2 == 0 else QColor(35, 35, 40, 150)
            painter.fillRect(0, row_y, self.width(), row_height, bg_color)
            
            # Draw Line Separator
            painter.setPen(QColor(60, 60, 65))
            painter.drawLine(0, row_y + row_height, self.width(), row_y + row_height)
            
            # Draw Line Label
            painter.setPen(QColor(150, 150, 150))
            painter.drawText(5, row_y + 20, f"Line {i+1}")
            
            # Draw Syllable Blocks
            items_to_draw = line.tokens if line.tokens else [line]
            
            # Center vertically within row
            block_y = row_y + 30
            block_h = 40
            
            for j, token in enumerate(items_to_draw):
                start_x = int(token.start_time * pps)
                duration = token.end_time - token.start_time
                w = max(5, int(duration * pps))
                
                # HORIZONTAL CULLING: Skip syllables outside visible area
                # Add margin for smooth scrolling
                h_margin = 100  # pixels
                if start_x + w < visible_left - h_margin:
                    continue  # Block is completely to the left, skip
                if start_x > visible_right + h_margin:
                    continue  # Block is completely to the right, skip
                
                rect = QRectF(start_x, block_y, w, block_h)
                
                # Check selection
                is_selected = (hasattr(token, 'text') and 
                              self.editor.selected_token == token)
                
                # Determine state and colors
                current_time = self.editor.current_time
                is_active = token.start_time <= current_time < token.end_time
                is_past = token.end_time <= current_time
                
                # Detect if we're scrubbing (fast seeking)
                is_scrubbing = self.editor._is_scrubbing
                
                from PyQt6.QtGui import QLinearGradient, QRadialGradient
                
                # Create stunning gradients based on state
                if is_selected:
                    # Golden gradient for selection
                    gradient = QLinearGradient(rect.left(), rect.top(), rect.right(), rect.bottom())
                    gradient.setColorAt(0, QColor(255, 215, 0))      # Gold
                    gradient.setColorAt(0.5, QColor(255, 200, 50))   # Lighter gold
                    gradient.setColorAt(1, QColor(255, 180, 0))      # Darker gold
                    
                    # Only draw glow if NOT scrubbing (performance)
                    if not is_scrubbing:
                        glow_rect = rect.adjusted(-4, -4, 4, 4)
                        painter.setPen(Qt.PenStyle.NoPen)
                        for i in range(4, 0, -1):
                            glow_expand = glow_rect.adjusted(-i, -i, i, i)
                            glow_color = QColor(255, 215, 0, 30 * (5 - i))
                            painter.setBrush(glow_color)
                            painter.drawRoundedRect(glow_expand, 8, 8)
                    
                elif is_active:
                    # Vibrant pink/red gradient for active
                    gradient = QLinearGradient(rect.left(), rect.top(), rect.right(), rect.bottom())
                    gradient.setColorAt(0, QColor(255, 107, 129))    # #ff6b81
                    gradient.setColorAt(0.5, QColor(240, 147, 251))   # #f093fb (pink)
                    gradient.setColorAt(1, QColor(245, 87, 108))     # #f5576c
                    
                    # Only draw glow if NOT scrubbing (performance)
                    if not is_scrubbing:
                        for i in range(3, 0, -1):
                            glow_expand = rect.adjusted(-i*2, -i*2, i*2, i*2)
                            glow_color = QColor(255, 107, 129, 40 * (4 - i))
                            painter.setPen(Qt.PenStyle.NoPen)
                            painter.setBrush(glow_color)
                            painter.drawRoundedRect(glow_expand, 8, 8)
                    
                elif is_past:
                    # Muted blue-grey gradient for past
                    gradient = QLinearGradient(rect.left(), rect.top(), rect.right(), rect.bottom())
                    gradient.setColorAt(0, QColor(108, 117, 125, 180))   # Muted grey
                    gradient.setColorAt(0.5, QColor(90, 98, 105, 160))   # Darker
                    gradient.setColorAt(1, QColor(72, 80, 87, 180))      # Even darker
                    
                else:
                    # Cool purple/blue gradient for future
                    gradient = QLinearGradient(rect.left(), rect.top(), rect.right(), rect.bottom())
                    gradient.setColorAt(0, QColor(102, 126, 234))     # #667eea
                    gradient.setColorAt(0.5, QColor(118, 75, 162))     # #764ba2  
                    gradient.setColorAt(1, QColor(88, 144, 255))      # Lighter blue
                
                # Draw syllable block with rounded corners
                painter.setPen(Qt.PenStyle.NoPen)
                painter.setBrush(QBrush(gradient))
                painter.drawRoundedRect(rect, 8, 8)
                
                # Glassmorphism effect - top highlight
                highlight_rect = QRectF(rect.left() + 2, rect.top() + 2, rect.width() - 4, rect.height() / 3)
                highlight_gradient = QLinearGradient(0, highlight_rect.top(), 0, highlight_rect.bottom())
                highlight_gradient.setColorAt(0, QColor(255, 255, 255, 60))
                highlight_gradient.setColorAt(1, QColor(255, 255, 255, 0))
                painter.setBrush(highlight_gradient)
                painter.drawRoundedRect(highlight_rect, 6, 6)
                
                # Border with subtle glow
                if is_selected:
                    border_color = QColor(255, 215, 0, 200)
                    border_width = 2.5
                elif is_active:
                    border_color = QColor(255, 255, 255, 150)
                    border_width = 2
                else:
                    border_color = QColor(255, 255, 255, 80)
                    border_width = 1.5
                    
                painter.setPen(QPen(border_color, border_width))
                painter.setBrush(Qt.BrushStyle.NoBrush)
                painter.drawRoundedRect(rect, 8, 8)
                
                # Text with shadow
                label = token.text if hasattr(token, 'text') else token.text
                
                # Text shadow for depth
                shadow_offset = QPointF(1, 1)
                shadow_rect = rect.translated(shadow_offset)
                painter.setPen(QColor(0, 0, 0, 100))
                painter.setFont(QFont("Segoe UI", 11, QFont.Weight.Bold))
                painter.drawText(shadow_rect, Qt.AlignmentFlag.AlignCenter, label)
                
                # Main text
                if is_past:
                    text_color = QColor(200, 200, 200)  # Light grey for past
                else:
                    text_color = QColor(255, 255, 255)  # White for active/future
                    
                painter.setPen(text_color)
                painter.drawText(rect, Qt.AlignmentFlag.AlignCenter, label)

    def _get_token_at_pos(self, pos):
        """Find token at position (Row Layout)"""
        try:
            pps = self.editor.pixels_per_second
            if pps <= 0: pps = 100.0
            
            row_height = 80
            
            # Determine row index from Y
            row_idx = int(pos.y() // row_height)
            
            if not self.editor.lyrics_data or not self.editor.lyrics_data.lines:
                return None, None
                
            if row_idx < 0 or row_idx >= len(self.editor.lyrics_data.lines):
                return None, None
                
            line = self.editor.lyrics_data.lines[row_idx]
            if not line:
                return None, None
                
            items = line.tokens if line.tokens else [line]
            
            block_y = row_idx * row_height + 30
            block_h = 40
            
            # Check tokens in this row
            for item in items:
                # Ensure item has valid times
                if not hasattr(item, 'start_time') or not hasattr(item, 'end_time'):
                    continue
                    
                start_x = int(item.start_time * pps)
                duration = item.end_time - item.start_time
                w = max(5, int(duration * pps))
                
                rect = QRectF(start_x, block_y, w, block_h)
                
                if rect.contains(pos):
                     # Check for edge interactions (resize)
                    if pos.x() < rect.left() + 10:
                        return item, 'resize_left'
                    elif pos.x() > rect.right() - 10:
                        return item, 'resize_right'
                    else:
                        return item, 'move'
                        
            return None, None
        except Exception as e:
            logger.error(f"Error in _get_token_at_pos: {e}", exc_info=True)
            return None, None

    def mousePressEvent(self, event):
        try:
            if event.button() == Qt.MouseButton.LeftButton:
                # Calculate time from click position
                pps = self.editor.pixels_per_second
                if pps <= 0: pps = 100.0 # Safety fallback
                
                x = event.position().x()
                time_s = x / pps
                time_ms = int(time_s * 1000)
                
                # ALWAYS seek to clicked position first (for playback)
                self.editor.seek_requested.emit(time_ms)
                self.editor.current_time = time_s
                
                # Check if clicking on a token
                item, mode = self._get_token_at_pos(event.position())
                
                # Enable drag mode only if:
                # 1. Clicking on a token AND
                # 2. Holding Ctrl key (for intentional editing)
                if item and (event.modifiers() & Qt.KeyboardModifier.ControlModifier):
                    # Dragging/Editing Mode
                    self.editor.selected_token = item
                    self.dragging = True
                    self.drag_mode = mode
                    self.drag_start_pos = event.position()
                    self.drag_start_time = item.start_time
                    self.drag_start_duration = item.end_time - item.start_time
                else:
                    # Scrubbing Mode (default)
                    self.editor.selected_token = None
                    self.dragging = False
                
                # Update display
                self.update()
        except Exception as e:
            logger.error(f"Error in mousePressEvent: {e}", exc_info=True)

    def mouseMoveEvent(self, event):
        # Update cursor based on hover
        if not self.dragging:
            item, mode = self._get_token_at_pos(event.position())
            
            # Only change cursor if mode actually changed
            current_cursor = self.cursor().shape()
            new_cursor = Qt.CursorShape.ArrowCursor
            
            # Check if Ctrl is held (drag mode)
            ctrl_held = event.modifiers() & Qt.KeyboardModifier.ControlModifier
            
            if item:
                if ctrl_held:
                    # Ctrl held: show drag/resize cursors
                    if mode in ['resize_left', 'resize_right']:
                        new_cursor = Qt.CursorShape.SizeHorCursor
                    elif mode == 'move':
                        new_cursor = Qt.CursorShape.SizeAllCursor
                else:
                    # No Ctrl: show "clickable" cursor (seeking mode)
                    new_cursor = Qt.CursorShape.PointingHandCursor
            
            # Only set cursor if it changed
            if current_cursor != new_cursor:
                self.setCursor(new_cursor)
            return

        # Handle Dragging
        if self.dragging and self.editor.selected_token:
            delta_x = event.position().x() - self.drag_start_pos.x()
            delta_time = delta_x / self.editor.pixels_per_second
            
            item = self.editor.selected_token
            
            if self.drag_mode == 'move':
                new_start = max(0, self.drag_start_time + delta_time)
                # maintain duration
                item.start_time = new_start
                item.end_time = new_start + self.drag_start_duration
                
            elif self.drag_mode == 'resize_left':
                # Move start time, keep end time fixed (change duration)
                old_end = self.drag_start_time + self.drag_start_duration
                new_start = min(old_end - 0.05, max(0, self.drag_start_time + delta_time))
                item.start_time = new_start
                # End time stays same? No, end_time is fixed property? 
                # LyricsToken has start/end.
                # If we resize left edge, start changes, end stays same.
                
            elif self.drag_mode == 'resize_right':
                # Keep start time, move end time
                new_duration = max(0.05, self.drag_start_duration + delta_time)
                item.end_time = item.start_time + new_duration
                
            self.update()



    def mouseReleaseEvent(self, event):
        if self.dragging:
            self.dragging = False
            self.drag_mode = None
            # Emit signal that data changed
            self.editor.data_changed.emit()
            self.update()
