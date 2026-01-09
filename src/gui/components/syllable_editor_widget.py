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
        
        self._init_ui()

    def set_position(self, position_ms):
        """Update playhead position (ms)"""
        self.current_time = position_ms / 1000.0
        self.canvas.update()
        
        # Auto-scroll logic
        pps = self.pixels_per_second
        playhead_x = int(self.current_time * pps)
        
        # 1. Horizontal Scroll: Ensure Playhead is visible (scrolling proactively)
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
        """Initialize UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        
        # Toolbar
        toolbar = self._create_toolbar()
        layout.addWidget(toolbar)
        
        # Scroll Area
        self.scroll_area = QScrollArea()
        self.scroll_area.setWidgetResizable(True)
        
        # Canvas
        self.canvas = SyllableCanvas(self)
        self.scroll_area.setWidget(self.canvas)
        
        layout.addWidget(self.scroll_area)
        
    def _create_toolbar(self):
        toolbar = QWidget()
        layout = QHBoxLayout(toolbar)
        layout.setContentsMargins(5, 5, 5, 5)
        
        layout.addWidget(QLabel("Zoom:"))
        self.zoom_slider = QSlider(Qt.Orientation.Horizontal)
        self.zoom_slider.setRange(50, 400)
        self.zoom_slider.setValue(100)
        self.zoom_slider.valueChanged.connect(self._on_zoom_changed)
        layout.addWidget(self.zoom_slider)
        
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
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        # Background
        painter.fillRect(self.rect(), QColor(30, 30, 35))
        
        if not self.editor.lyrics_data:
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, "No lyrics loaded")
            return

        # Logging to debug "nothing shown"
        # print(f"Painting: {len(self.editor.lyrics_data.lines)} lines, Duration: {self.editor.waveform_duration}")

        # Draw Playhead
        pps = self.editor.pixels_per_second
        playhead_x = int(self.editor.current_time * pps)
        painter.setPen(QPen(QColor(255, 50, 50), 2))
        painter.drawLine(playhead_x, 0, playhead_x, self.height())
        
        # Draw Playhead Triangle
        try:
            from PyQt6.QtGui import QPolygon
            triangle = QPolygon([
                QPoint(playhead_x, 0),
                QPoint(playhead_x - 6, 10),
                QPoint(playhead_x + 6, 10)
            ])
            painter.setBrush(QBrush(QColor(255, 50, 50)))
            painter.drawPolygon(triangle)
        except Exception as e:
            print(f"Error drawing playhead: {e}")

        # Draw Waveform Background (if available)
        try:
            self._draw_waveform(painter)
        except Exception as e:
            print(f"Error drawing waveform: {e}")
            
        # Draw Syllable Blocks
        try:
            self._draw_syllables(painter)
        except Exception as e:
            print(f"Error drawing syllables: {e}")

    def _draw_syllables(self, painter: QPainter):
        """Draw lyrics with one line per row container"""
        # ... (Same as before) ...
        pps = self.editor.pixels_per_second
        
        row_height = 80
        padding_y = 10
        
        painter.setFont(QFont("Arial", 10))
        
        # Calculate vertical offset to center the active area? 
        # For now, just top-down list
        
        for i, line in enumerate(self.editor.lyrics_data.lines):
            row_y = i * row_height
            
            # Draw Row Container Background
            bg_color = QColor(40, 40, 45) if i % 2 == 0 else QColor(35, 35, 40)
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
                
                rect = QRectF(start_x, block_y, w, block_h)
                
                # Check selection
                is_selected = (hasattr(token, 'text') and 
                              self.editor.selected_token == token)
                
                # Determine color (highlight if active?)
                # "highlighted as the music go on"
                # Check playhead position
                current_time = self.editor.current_time
                is_active = token.start_time <= current_time < token.end_time
                is_past = token.end_time <= current_time
                
                if is_selected:
                    color = QColor(255, 255, 150) # Bright yellow selection
                elif is_active:
                    color = QColor(255, 100, 100) # Active red
                elif is_past:
                    color = QColor(100, 100, 120) # Dimmed past
                else:
                    color = QColor(100, 150, 200) # Default future
                
                painter.fillRect(rect, color)
                painter.setPen(QPen(QColor(0, 0, 0)))
                painter.drawRect(rect)
                
                # Text
                label = token.text if hasattr(token, 'text') else token.text
                painter.setPen(QColor(255, 255, 255) if is_past else QColor(0, 0, 0))
                painter.drawText(rect, Qt.AlignmentFlag.AlignCenter, label)

    def _get_token_at_pos(self, pos):
        """Find token at position (Row Layout)"""
        pps = self.editor.pixels_per_second
        row_height = 80
        
        # Determine row index from Y
        row_idx = int(pos.y() // row_height)
        
        if not self.editor.lyrics_data or row_idx < 0 or row_idx >= len(self.editor.lyrics_data.lines):
            return None, None
            
        line = self.editor.lyrics_data.lines[row_idx]
        items = line.tokens if line.tokens else [line]
        
        block_y = row_idx * row_height + 30
        block_h = 40
        
        # Check tokens in this row
        for item in items:
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

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            item, mode = self._get_token_at_pos(event.position())
            
            if item:
                # Dragging Logic
                self.editor.selected_token = item
                self.dragging = True
                self.drag_mode = mode
                self.drag_start_pos = event.position()
                self.drag_start_time = item.start_time
                self.drag_start_duration = item.end_time - item.start_time
                self.update() # Redraw selection
            else:
                # Scrubbing Logic
                self.editor.selected_token = None
                
                # Calculate time from x
                x = event.position().x()
                time_s = x / self.editor.pixels_per_second
                time_ms = int(time_s * 1000)
                
                # Emit seek signal
                self.editor.seek_requested.emit(time_ms)
                # Ensure playhead moves visibly immediately
                self.editor.set_position(time_ms)
                
                self.update()

    def mouseMoveEvent(self, event):
        # Update cursor based on hover
        if not self.dragging:
            item, mode = self._get_token_at_pos(event.position())
            if mode in ['resize_left', 'resize_right']:
                self.setCursor(Qt.CursorShape.SizeHorCursor)
            elif mode == 'move':
                self.setCursor(Qt.CursorShape.SizeAllCursor)
            else:
                self.setCursor(Qt.CursorShape.ArrowCursor)
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
