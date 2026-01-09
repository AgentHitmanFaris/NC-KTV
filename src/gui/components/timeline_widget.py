"""
Timeline Widget for NC-KTV
Multi-track timeline display with playhead, clips, and zoom controls
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QScrollArea,
    QPushButton, QLabel, QSlider, QCheckBox
)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QFont, QPainterPath
from typing import Optional

from core.timeline_data import TimelineData, Track, Clip, TrackType


class TimelineWidget(QWidget):
    """Main timeline interface widget with multi-track display"""
    
    # Signals
    playhead_moved = pyqtSignal(float)  # Emit when user clicks on timeline to seek (seconds)
    clip_selected = pyqtSignal(str)  # Emit clip ID when selected
    clip_moved = pyqtSignal(str, float)  # Emit clip ID and new start time
    clip_resized = pyqtSignal(str, float)  # Emit clip ID and new duration
    clip_split = pyqtSignal(str, float)  # Emit clip ID and split time
    clip_deleted = pyqtSignal(str)  # Emit clip ID
    effect_requested = pyqtSignal(str)  # Emit clip ID to add effect
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.timeline_data: Optional[TimelineData] = None
        self.current_time: float = 0.0  # Current playback position (seconds)
        self.pixels_per_second: float = 50.0  # Zoom level
        self.scroll_offset: float = 0.0  # Horizontal scroll position
        
        self.track_height = 60  # Height of each track in pixels
        self.header_width = 120  # Width of track name headers
        self.ruler_height = 30  # Height of time ruler
        
        self.ruler_height = 30  # Height of time ruler
        
        self.waveform_data = None
        self.waveform_duration = 0
        
        self.selected_clip_id: Optional[str] = None
        self.is_dragging_clip = False
        self.is_resizing_clip = False
        self.resize_from_start = False  # True if resizing from left edge
        self.drag_start_x = 0
        self.drag_start_time = 0.0
        self.dragged_clip_original_start = 0.0
        
        self._init_ui()
        
    def _init_ui(self):
        """Initialize UI layout"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        
        # Toolbar
        toolbar = self._create_toolbar()
        layout.addWidget(toolbar)
        
        # Timeline view (scroll area for tracks)
        self.scroll_area = QScrollArea()
        self.scroll_area.setWidgetResizable(True)
        self.scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
        self.scroll_area.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
        
        # Timeline canvas
        self.timeline_canvas = TimelineCanvas(self)
        self.scroll_area.setWidget(self.timeline_canvas)
        
        layout.addWidget(self.scroll_area)
        
    def _create_toolbar(self) -> QWidget:
        """Create toolbar with zoom and snap controls"""
        toolbar = QWidget()
        toolbar_layout = QHBoxLayout(toolbar)
        toolbar_layout.setContentsMargins(5, 5, 5, 5)
        
        # Zoom controls
        toolbar_layout.addWidget(QLabel("Zoom:"))
        
        zoom_out_btn = QPushButton("-")
        zoom_out_btn.setFixedWidth(30)
        zoom_out_btn.clicked.connect(self._zoom_out)
        toolbar_layout.addWidget(zoom_out_btn)
        
        self.zoom_slider = QSlider(Qt.Orientation.Horizontal)
        self.zoom_slider.setMinimum(10)  # 10 pixels/second
        self.zoom_slider.setMaximum(200)  # 200 pixels/second
        self.zoom_slider.setValue(int(self.pixels_per_second))
        self.zoom_slider.setFixedWidth(150)
        self.zoom_slider.valueChanged.connect(self._on_zoom_changed)
        toolbar_layout.addWidget(self.zoom_slider)
        
        zoom_in_btn = QPushButton("+")
        zoom_in_btn.setFixedWidth(30)
        zoom_in_btn.clicked.connect(self._zoom_in)
        toolbar_layout.addWidget(zoom_in_btn)
        
        toolbar_layout.addSpacing(20)
        
        # Snap to grid
        self.snap_checkbox = QCheckBox("Snap to Grid")
        self.snap_checkbox.setChecked(True)
        self.snap_checkbox.toggled.connect(self._on_snap_toggled)
        toolbar_layout.addWidget(self.snap_checkbox)
        
        toolbar_layout.addStretch()
        
        # Timeline info
        self.time_label = QLabel("00:00.000")
        self.time_label.setStyleSheet("font-family: monospace; font-weight: bold;")
        toolbar_layout.addWidget(self.time_label)
        
        return toolbar
    
    def set_timeline_data(self, timeline_data: TimelineData):
        """Set the timeline data to display"""
        self.timeline_data = timeline_data
        self.pixels_per_second = timeline_data.zoom_level
        self.zoom_slider.setValue(int(self.pixels_per_second))
        self.snap_checkbox.setChecked(timeline_data.snap_to_grid)
        self.timeline_canvas.update()
        
    def set_current_time(self, seconds: float):
        """Update playhead position"""
        self.current_time = seconds
        
        # Update time label
        minutes = int(seconds // 60)
        secs = seconds % 60
        self.time_label.setText(f"{minutes:02d}:{secs:06.3f}")
        
        # Auto-scroll to keep playhead visible
        playhead_x = self.current_time * self.pixels_per_second
        viewport_width = self.scroll_area.viewport().width()
        scroll_value = self.scroll_area.horizontalScrollBar().value()
        
        # If playhead is outside viewport, scroll to center it
        if playhead_x < scroll_value or playhead_x > scroll_value + viewport_width:
            new_scroll = max(0, playhead_x - viewport_width // 2)
            self.scroll_area.horizontalScrollBar().setValue(int(new_scroll))
        
        self.timeline_canvas.update()
    
        self.timeline_canvas.update()

    def set_waveform_data(self, data, duration_ms):
        """Set waveform data for audio visualization"""
        # print(f"[DEBUG] set_waveform_data called. Data len: {len(data) if data is not None else 'None'}, Duration: {duration_ms}")
        self.waveform_data = data
        self.waveform_duration = duration_ms
        self.timeline_canvas.update()

    # ... (skipping to _draw_clip) ...

    def _draw_waveform(self, painter: QPainter, clip: Clip, x: int, y: int, width: int, height: int):
        """Draw waveform inside clip rect"""
        data = self.timeline_widget.waveform_data
        total_duration_ms = self.timeline_widget.waveform_duration
        if total_duration_ms == 0 or data is None or len(data) == 0:
            return
            
        painter.setPen(QPen(QColor(255, 255, 255, 200), 1)) # Made brighter white

    
    def _zoom_in(self):
        """Increase zoom level"""
        new_value = min(self.zoom_slider.maximum(), self.zoom_slider.value() + 10)
        self.zoom_slider.setValue(new_value)
    
    def _zoom_out(self):
        """Decrease zoom level"""
        new_value = max(self.zoom_slider.minimum(), self.zoom_slider.value() - 10)
        self.zoom_slider.setValue(new_value)
    
    def _on_zoom_changed(self, value: int):
        """Handle zoom slider change"""
        self.pixels_per_second = float(value)
        if self.timeline_data:
            self.timeline_data.zoom_level = self.pixels_per_second
        self.timeline_canvas.update()
    
    def _on_snap_toggled(self, checked: bool):
        """Handle snap to grid toggle"""
        if self.timeline_data:
            self.timeline_data.snap_to_grid = checked


class TimelineCanvas(QWidget):
    """Canvas widget for drawing timeline tracks, clips, and playhead"""
    
    def __init__(self, timeline_widget: TimelineWidget):
        super().__init__()
        self.timeline_widget = timeline_widget
        self.setMouseTracking(True)
        self.setCursor(Qt.CursorShape.ArrowCursor)
        
    def sizeHint(self):
        """Calculate required size based on timeline duration and number of tracks"""
        if not self.timeline_widget.timeline_data:
            return super().sizeHint()
        
        width = int(self.timeline_widget.timeline_data.duration * self.timeline_widget.pixels_per_second) + 100
        height = (self.timeline_widget.ruler_height + 
                 len(self.timeline_widget.timeline_data.tracks) * self.timeline_widget.track_height)
        
        from PyQt6.QtCore import QSize
        return QSize(width, height)
    
    def paintEvent(self, event):
        """Draw timeline ruler, tracks, clips, and playhead"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        if not self.timeline_widget.timeline_data:
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, "No timeline loaded")
            return
        
        # Draw background
        painter.fillRect(self.rect(), QColor(30, 30, 35))
        
        # Draw time ruler
        self._draw_ruler(painter)
        
        # Draw tracks
        y_offset = self.timeline_widget.ruler_height
        for track in self.timeline_widget.timeline_data.tracks:
            self._draw_track(painter, track, y_offset)
            y_offset += self.timeline_widget.track_height
        
        # Draw playhead (on top of everything)
        self._draw_playhead(painter)
    
    def _draw_ruler(self, painter: QPainter):
        """Draw time ruler with second markers"""
        ruler_height = self.timeline_widget.ruler_height
        painter.fillRect(0, 0, self.width(), ruler_height, QColor(40, 40, 45))
        
        painter.setPen(QPen(QColor(180, 180, 180)))
        painter.setFont(QFont("Arial", 8))
        
        # Draw second markers
        duration = self.timeline_widget.timeline_data.duration
        pps = self.timeline_widget.pixels_per_second
        
        # Calculate marker interval (1s, 5s, 10s, etc. based on zoom)
        if pps > 100:
            interval = 1.0
        elif pps > 50:
            interval = 2.0
        elif pps > 20:
            interval = 5.0
        else:
            interval = 10.0
        
        time = 0.0
        while time <= duration + 1:
            x = int(time * pps)
            
            # Draw tick mark
            painter.drawLine(x, ruler_height - 10, x, ruler_height)
            
            # Draw time label
            minutes = int(time // 60)
            seconds = int(time % 60)
            painter.drawText(x + 2, 12, f"{minutes}:{seconds:02d}")
            
            time += interval
    
    def _draw_track(self, painter: QPainter, track: Track, y_offset: int):
        """Draw a single track with its clips"""
        track_height = self.timeline_widget.track_height
        
        # Track background (alternating colors)
        bg_color = QColor(45, 45, 50) if y_offset % (track_height * 2) == 0 else QColor(50, 50, 55)
        painter.fillRect(0, y_offset, self.width(), track_height, bg_color)
        
        # Track header
        painter.setPen(QPen(QColor(200, 200, 200)))
        painter.setFont(QFont("Arial", 9, QFont.Weight.Bold))
        painter.drawText(5, y_offset + 20, track.name)
        
        # Draw track type indicator
        type_colors = {
            TrackType.AUDIO: QColor(100, 150, 255),
            TrackType.VIDEO: QColor(255, 100, 150),
            TrackType.EFFECTS: QColor(150, 255, 100),
            TrackType.LYRICS: QColor(255, 200, 100)
        }
        type_color = type_colors.get(track.track_type, QColor(150, 150, 150))
        painter.fillRect(5, y_offset + 5, 8, track_height - 10, type_color)
        
        # Draw clips
        for clip in track.clips:
            self._draw_clip(painter, clip, y_offset, track.track_type)
    
    def _draw_clip(self, painter: QPainter, clip: Clip, track_y: int, track_type: str = None):
        """Draw a single clip on the track"""
        pps = self.timeline_widget.pixels_per_second
        x = int(clip.start_time * pps)
        width = int(clip.duration * pps)
        
        # Clip background
        is_selected = clip.clip_id == self.timeline_widget.selected_clip_id
        clip_color = QColor(80, 120, 180) if not is_selected else QColor(120, 160, 220)
        
        painter.fillRect(x, track_y + 5, width, self.timeline_widget.track_height - 10, clip_color)
        
        # Phase 6.2: Draw Waveform for Audio Tracks
        if track_type == TrackType.AUDIO and self.timeline_widget.waveform_data is not None:
             self._draw_waveform(painter, clip, x, track_y + 5, width, self.timeline_widget.track_height - 10)
             # Draw lyrics overlay on top of waveform
             self._draw_lyrics_overlay(painter, clip, x, track_y + 5, width, self.timeline_widget.track_height - 10)
        
        # Clip border
        border_color = QColor(255, 255, 255) if is_selected else QColor(100, 140, 200)
        painter.setPen(QPen(border_color, 2))
        painter.drawRect(x, track_y + 5, width, self.timeline_widget.track_height - 10)
        
        # Clip label (Phase 6.1 Polish - show lyrics text)
        painter.setPen(QPen(QColor(255, 255, 255)))
        painter.setFont(QFont("Arial", 8))
        
        # If this is a lyrics clip, show the text
        if 'text' in clip.properties:
            clip_label = clip.properties['text'][:30]  # Truncate long text
        else:
            clip_label = clip.clip_id[:20]  # Fallback to ID
        
        painter.drawText(x + 5, track_y + 25, clip_label)

    def _draw_waveform(self, painter: QPainter, clip: Clip, x: int, y: int, width: int, height: int):
        """Draw waveform inside clip rect"""
        data = self.timeline_widget.waveform_data
        total_duration_ms = self.timeline_widget.waveform_duration
        if total_duration_ms == 0 or len(data) == 0:
            return
            
        painter.setPen(QPen(QColor(30, 30, 50, 120), 1))
        # Draw center line
        center_y = y + height / 2
        # painter.drawLine(x, int(center_y), x + width, int(center_y))
        
        painter.setPen(QPen(QColor(200, 220, 255, 200), 1))
        
        # Optimization: Don't draw every sample, draw per pixel or step
        step = max(1, width // 200) # Detail level
        
        path = QPainterPath()
        started = False
        
        clip_start_ms = clip.start_time * 1000
        clip_end_ms = clip.end_time * 1000
        
        # Map pixel x to waveform index
        samples_count = len(data)
        
        for pixel_i in range(0, width, 1):
            # Time at this pixel relative to clip start
            # pixel_i corresponds to (pixel_i / width) * clip_duration?
            # No, pixel_i is offset from clip start x.
            
            # Global time of this pixel
            # current_x_global = x + pixel_i
            # Only draw if inside clip (well, we are iterating width)
            
            # Convert pixel offset to time offset in seconds
            time_offset_s = pixel_i / self.timeline_widget.pixels_per_second
            current_time_ms = clip_start_ms + (time_offset_s * 1000)
            
            if current_time_ms > total_duration_ms:
                break
                
            # Index in waveform data
            idx = int((current_time_ms / total_duration_ms) * samples_count)
            
            if 0 <= idx < samples_count:
                amp = data[idx]
                h_amp = amp * (height / 2 - 2)
                
                if not started:
                    path.moveTo(x + pixel_i, center_y - h_amp)
                    started = True
                else:
                    path.lineTo(x + pixel_i, center_y - h_amp)
                    
        # Mirror path for bottom half? Or just draw simple line
        painter.drawPath(path)
        
        # Bottom half mirror
        path_b = QPainterPath()
        started_b = False
        for pixel_i in range(0, width, 1):
            time_offset_s = pixel_i / self.timeline_widget.pixels_per_second
            current_time_ms = clip_start_ms + (time_offset_s * 1000)
            idx = int((current_time_ms / total_duration_ms) * samples_count)
            if 0 <= idx < samples_count:
                amp = data[idx]
                h_amp = amp * (height / 2 - 2)
                if not started_b:
                    path_b.moveTo(x + pixel_i, center_y + h_amp)
                    started_b = True
                else:
                    path_b.lineTo(x + pixel_i, center_y + h_amp)
        painter.drawPath(path_b)
    
    def _draw_lyrics_overlay(self, painter: QPainter, audio_clip: Clip, x: int, y: int, width: int, height: int):
        """Draw lyrics text overlaying the audio clip"""
        if not self.timeline_widget.timeline_data:
            return

        # Find lyrics track
        lyrics_track = None
        for track in self.timeline_widget.timeline_data.tracks:
            if track.track_type == TrackType.LYRICS:
                lyrics_track = track
                break
        
        if not lyrics_track:
            return

        painter.setPen(QPen(QColor(255, 255, 100))) # Yellow text
        painter.setFont(QFont("Arial", 10, QFont.Weight.Bold))
        
        pps = self.timeline_widget.pixels_per_second
        audio_start = audio_clip.start_time
        audio_end = audio_clip.end_time
        
        for clip in lyrics_track.clips:
            # Check overlap
            if clip.end_time > audio_start and clip.start_time < audio_end:
                # Calculate relative position
                rel_start = max(0, clip.start_time - audio_start)
                rel_x = int(rel_start * pps)
                
                # Check if text is visible within the audio clip rect
                if rel_x < width:
                     text = clip.properties.get('text', '')
                     # Draw text with a slight shadow for readability
                     painter.setPen(QPen(QColor(0, 0, 0)))
                     painter.drawText(x + rel_x + 6, y + 21, text) # Shadow
                     
                     painter.setPen(QPen(QColor(255, 255, 100)))
                     painter.drawText(x + rel_x + 5, y + 20, text)

    def _draw_playhead(self, painter: QPainter):
        """Draw the playhead indicator"""
        pps = self.timeline_widget.pixels_per_second
        x = int(self.timeline_widget.current_time * pps)
        
        # Playhead line
        painter.setPen(QPen(QColor(255, 50, 50), 2))
        painter.drawLine(x, 0, x, self.height())
        
        # Playhead triangle at top
        from PyQt6.QtGui import QPolygon
        from PyQt6.QtCore import QPoint
        triangle = QPolygon([
            QPoint(x, 0),
            QPoint(x - 6, 10),
            QPoint(x + 6, 10)
        ])
        painter.setBrush(QBrush(QColor(255, 50, 50)))
        painter.drawPolygon(triangle)
    
    def mousePressEvent(self, event):
        """Handle mouse press for seeking, clip selection, and drag/resize initiation"""
        if event.button() == Qt.MouseButton.LeftButton:
            x = event.pos().x()
            y = event.pos().y()
            time = x / self.timeline_widget.pixels_per_second
            
            # Check if clicking on a clip
            clicked_clip, clip_zone = self._get_clip_at_position_with_zone(event.pos())
            
            if clicked_clip:
                self.timeline_widget.selected_clip_id = clicked_clip.clip_id
                self.timeline_widget.clip_selected.emit(clicked_clip.clip_id)
                
                # Check if clicking on resize zones
                if clip_zone == 'left_edge':
                    self.timeline_widget.is_resizing_clip = True
                    self.timeline_widget.resize_from_start = True
                    self.timeline_widget.drag_start_x = x
                    self.timeline_widget.drag_start_time = clicked_clip.start_time
                    self.timeline_widget.dragged_clip_original_start = clicked_clip.start_time
                elif clip_zone == 'right_edge':
                    self.timeline_widget.is_resizing_clip = True
                    self.timeline_widget.resize_from_start = False
                    self.timeline_widget.drag_start_x = x
                    self.timeline_widget.drag_start_time = clicked_clip.start_time
                else:
                    # Start dragging clip
                    self.timeline_widget.is_dragging_clip = True
                    self.timeline_widget.drag_start_x = x
                    self.timeline_widget.drag_start_time = time
                    self.timeline_widget.dragged_clip_original_start = clicked_clip.start_time
                
                self.update()
            else:
                # Click on empty space - seek playhead
                if y > self.timeline_widget.ruler_height:
                    self.timeline_widget.playhead_moved.emit(max(0.0, time))
    
    def mouseMoveEvent(self, event):
        """Handle mouse move for dragging clips, resizing, and cursor updates"""
        x = event.pos().x()
        
        # Update cursor based on hover position
        if not self.timeline_widget.is_dragging_clip and not self.timeline_widget.is_resizing_clip:
            clicked_clip, clip_zone = self._get_clip_at_position_with_zone(event.pos())
            if clicked_clip:
                if clip_zone == 'left_edge' or clip_zone == 'right_edge':
                    self.setCursor(Qt.CursorShape.SizeHorCursor)
                else:
                    self.setCursor(Qt.CursorShape.OpenHandCursor)
            else:
                self.setCursor(Qt.CursorShape.ArrowCursor)
        
        # Handle clip dragging
        if self.timeline_widget.is_dragging_clip and self.timeline_widget.selected_clip_id:
            if not self.timeline_widget.timeline_data:
                return
            
            # Calculate new position
            time_delta = (x - self.timeline_widget.drag_start_x) / self.timeline_widget.pixels_per_second
            new_start_time = self.timeline_widget.dragged_clip_original_start + time_delta
            
            # Snap to grid if enabled
            if self.timeline_widget.timeline_data.snap_to_grid:
                new_start_time = self.timeline_widget.timeline_data.snap_time(new_start_time)
            
            # Find the clip and update its position (visual only, committed on release)
            for track in self.timeline_widget.timeline_data.tracks:
                clip = track.get_clip(self.timeline_widget.selected_clip_id)
                if clip:
                    # Temporary visual update
                    clip.start_time = max(0.0, new_start_time)
                    self.update()
                    break
        
        # Handle clip resizing
        elif self.timeline_widget.is_resizing_clip and self.timeline_widget.selected_clip_id:
            if not self.timeline_widget.timeline_data:
                return
            
            self.setCursor(Qt.CursorShape.SizeHorCursor)
            
            # Find the clip
            for track in self.timeline_widget.timeline_data.tracks:
                clip = track.get_clip(self.timeline_widget.selected_clip_id)
                if clip:
                    time_delta = (x - self.timeline_widget.drag_start_x) / self.timeline_widget.pixels_per_second
                    
                    if self.timeline_widget.resize_from_start:
                        # Resizing from left edge
                        new_start = self.timeline_widget.dragged_clip_original_start + time_delta
                        new_duration = (self.timeline_widget.dragged_clip_original_start + clip.duration) - new_start
                        new_duration = max(0.1, new_duration)  # Minimum 100ms
                        
                        clip.start_time = new_start
                        clip.duration = new_duration
                    else:
                        # Resizing from right edge
                        original_duration = clip.duration
                        new_duration = original_duration + time_delta
                        clip.duration = max(0.1, new_duration)  # Minimum 100ms
                    
                    self.update()
                    break
    
    def mouseReleaseEvent(self, event):
        """Handle mouse release to commit drag/resize operations"""
        if event.button() == Qt.MouseButton.LeftButton:
            # Commit drag operation
            if self.timeline_widget.is_dragging_clip and self.timeline_widget.selected_clip_id:
                if self.timeline_widget.timeline_data:
                    # Find the clip and emit signal
                    for track in self.timeline_widget.timeline_data.tracks:
                        clip = track.get_clip(self.timeline_widget.selected_clip_id)
                        if clip:
                            # Check for overlap
                            if not track.check_overlap(clip, exclude_clip_id=clip.clip_id):
                                self.timeline_widget.clip_moved.emit(clip.clip_id, clip.start_time)
                            else:
                                # Revert to original position
                                clip.start_time = self.timeline_widget.dragged_clip_original_start
                            break
            
            # Commit resize operation
            elif self.timeline_widget.is_resizing_clip and self.timeline_widget.selected_clip_id:
                if self.timeline_widget.timeline_data:
                    for track in self.timeline_widget.timeline_data.tracks:
                        clip = track.get_clip(self.timeline_widget.selected_clip_id)
                        if clip:
                            self.timeline_widget.clip_resized.emit(clip.clip_id, clip.duration)
                            break
            
            # Reset drag states
            self.timeline_widget.is_dragging_clip = False
            self.timeline_widget.is_resizing_clip = False
            self.timeline_widget.resize_from_start = False
            self.setCursor(Qt.CursorShape.ArrowCursor)
            self.update()
    
    def keyPressEvent(self, event):
        """Handle keyboard shortcuts for clip operations"""
        if self.timeline_widget.selected_clip_id:
            if event.key() == Qt.Key.Key_Delete:
                # Delete selected clip
                self.timeline_widget.clip_deleted.emit(self.timeline_widget.selected_clip_id)
            elif event.key() == Qt.Key.Key_B and event.modifiers() == Qt.KeyboardModifier.ControlModifier:
                # Split clip at playhead
                self.timeline_widget.clip_split.emit(
                    self.timeline_widget.selected_clip_id,
                    self.timeline_widget.current_time
                )
            elif event.key() == Qt.Key.Key_E and event.modifiers() == Qt.KeyboardModifier.ControlModifier:
                # Request effect panel
                self.timeline_widget.effect_requested.emit(self.timeline_widget.selected_clip_id)
    
    def _get_clip_at_position_with_zone(self, pos) -> tuple[Optional[Clip], Optional[str]]:
        """
        Get clip at mouse position and detect which zone (center, left_edge, right_edge)
        
        Returns:
            (clip, zone) where zone is 'left_edge', 'right_edge', or 'center'
        """
        clip = self._get_clip_at_position(pos)
        if not clip:
            return None, None
        
        x = pos.x()
        clip_start_x = int(clip.start_time * self.timeline_widget.pixels_per_second)
        clip_end_x = int(clip.end_time * self.timeline_widget.pixels_per_second)
        
        EDGE_THRESHOLD = 10  # pixels
        
        # Check edges
        if abs(x - clip_start_x) <= EDGE_THRESHOLD:
            return clip, 'left_edge'
        elif abs(x - clip_end_x) <= EDGE_THRESHOLD:
            return clip, 'right_edge'
        else:
            return clip, 'center'
    
    def _get_clip_at_position(self, pos) -> Optional[Clip]:
        """Get clip at mouse position"""
        if not self.timeline_widget.timeline_data:
            return None
        
        x = pos.x()
        y = pos.y() - self.timeline_widget.ruler_height
        
        if y < 0:
            return None
        
        track_index = y // self.timeline_widget.track_height
        if track_index >= len(self.timeline_widget.timeline_data.tracks):
            return None
        
        track = self.timeline_widget.timeline_data.tracks[track_index]
        time = x / self.timeline_widget.pixels_per_second
        
        for clip in track.clips:
            if clip.start_time <= time <= clip.end_time:
                return clip
        
        return None
