"""
Karaoke Preview Widget
Renders text with a karaoke-style progress fill effect
Supports multiple animation types
"""

from PyQt6.QtWidgets import QWidget
from PyQt6.QtCore import Qt, QRectF, QTime
from PyQt6.QtGui import QPainter, QFont, QColor, QPainterPath, QPen, QRadialGradient
from core.effect_compositor import EffectCompositor


class AnimationType:
    """Animation type constants"""
    LINEAR_WIPE = "Linear Wipe"
    SYLLABLE_STEP = "Syllable Step"
    GLOW_PULSE = "Glow Pulse"
    FADE_IN = "Fade In"
    BOUNCING_BALL = "Bouncing Ball"
    RAINBOW = "Rainbow"
    TYPEWRITER = "Typewriter"
    SCALE_PULSE = "Scale Pulse"
    
    @classmethod
    def all(cls):
        return [cls.LINEAR_WIPE, cls.SYLLABLE_STEP, cls.GLOW_PULSE, cls.FADE_IN, 
                cls.BOUNCING_BALL, cls.RAINBOW, cls.TYPEWRITER, cls.SCALE_PULSE]


class KaraokePreviewWidget(QWidget):
    """
    Widget that displays lyrics with a karaoke-style fill effect.
    Supports multiple animation types.
    """
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        self.current_line = None
        self.next_line = None
        self.prev_line = None
        
        # Timing & Interpolation
        self.current_time = 0.0
        self.last_audio_time = 0.0
        self.is_playing = False
        
        # Romanization support
        self.romanization_mode = "Original"  # Options: "Original", "Romanized", "Both"
        
        # Timing mode for upcoming lyrics
        self.timing_mode = "Karaoke"  # Options: "Karaoke" (show early), "Lyrics Video" (show after)
        self.early_preview_seconds = 2.0  # Show next line 2 seconds early in Karaoke mode
        
        # Effect compositor for timeline effects (Phase 6.3)
        self.effect_compositor = EffectCompositor()
        self.current_clip = None  # Current lyrics clip with effects
        self.clip_start_time = 0.0  # Clip start time for effect calculations
        
        # Monotonic timer for interpolation
        from PyQt6.QtCore import QElapsedTimer
        self.delta_timer = QElapsedTimer()
        self.delta_timer.start()
        self.last_update_time = self.delta_timer.elapsed()
        
        self.anim_start_time = None
        self.animation_type = AnimationType.LINEAR_WIPE
        
        # Animation loop
        from PyQt6.QtCore import QTimer
        self.anim_timer = QTimer(self)
        self.anim_timer.setInterval(16) # ~60fps
        self.anim_timer.timeout.connect(self._update_animation)
        
        # Style
        self.font = QFont("Arial", 40, QFont.Weight.Bold)
        self.secondary_font = QFont("Arial", 28, QFont.Weight.Bold)
        self.romanized_font = QFont("Arial", 24, QFont.Weight.Normal)  # Smaller for romanized text
        self.inactive_color = QColor(255, 255, 255)
        self.active_color = QColor(255, 215, 0)
        self.secondary_color = QColor(200, 200, 200, 180)
        self.outline_color = QColor(0, 0, 0)
        self.glow_color = QColor(255, 200, 50, 150)
        
        self.setMinimumHeight(200)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setStyleSheet("background-color: transparent;")

        
    def _update_animation(self):
        """Called every 16ms to interpolate time and trigger repaint"""
        if not self.is_playing:
            return
            
        now = self.delta_timer.elapsed()
        elapsed_sec = (now - self.last_update_time) / 1000.0
        
        # Clamp interpolation to avoid runaway if UI freezes
        if elapsed_sec > 0.2: 
            elapsed_sec = 0.2
            
        self.current_time = self.last_audio_time + elapsed_sec
        self.update()

    def set_animation_type(self, anim_type: str):
        """Set the animation type"""
        if anim_type in AnimationType.all():
            self.animation_type = anim_type
            self.update()
            
    def set_active_color(self, color):
        """Set custom active/fill color"""
        self.active_color = color
        self.update()
        
    def set_line(self, line, next_line=None):
        """Set the current lyric line object and optional next line"""
        if self.current_line != line:
            self.prev_line = self.current_line
            self.current_line = line
            self.next_line = next_line
            self.anim_start_time = QTime.currentTime()
            self.update()
        elif self.next_line != next_line:
            self.next_line = next_line
            self.update()

    def set_current_time(self, seconds: float):
        """Set current playback time in seconds (from audio player)"""
        # Update raw audio time reference
        self.last_audio_time = seconds
        self.last_update_time = self.delta_timer.elapsed()
        
        # Start smooth animation loop if not running
        if not self.anim_timer.isActive():
            self.anim_timer.start()
        self.is_playing = True
            
        # Force immediate sync if drift is huge (seek happened)
        if abs(self.current_time - seconds) > 0.5:
             self.current_time = seconds
             
    def set_playing(self, playing: bool):
        """Update playing state"""
        self.is_playing = playing
        # Snap time slightly on pause to ensure we stop exactly where audio is
        if not playing:
            self.current_time = self.last_audio_time
            self.anim_timer.stop()
            self.update() # Constant update to show stopped state
            
    def set_text(self, text: str):
        """Compatibility method"""
        pass
    
    def set_romanization_enabled(self, enabled: bool):
        """Enable or disable romanization display (legacy method)"""
        self.romanization_mode = "Both" if enabled else "Original"
        self.update()  # Trigger repaint
    
    def set_romanization_mode(self, mode: str):
        """Set romanization display mode: 'Original', 'Romanized', or 'Both'"""
        self.romanization_mode = mode
        self.update()  # Trigger repaint
    
    def set_timing_mode(self, mode: str):
        """Set timing mode: 'Karaoke' or 'Lyrics Video'"""
        self.timing_mode = mode
        # No need to update() - will apply on next line change


    def paintEvent(self, event):
        """Draw the widget with scroll animation"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        rect = self.rect()
        
        if not self.current_line or not self.current_line.text:
            placeholder_font = QFont(self.font)
            placeholder_font.setPointSize(60)
            painter.setFont(placeholder_font)
            painter.setPen(QColor(200, 200, 200, 100))
            painter.drawText(rect, Qt.AlignmentFlag.AlignCenter, "Waiting for Lyrics...")
            return
        
        # Layout positions
        y_center = rect.height() * 0.4
        y_bottom = rect.height() * 0.85
        y_top_exit = rect.height() * 0.1
        y_below = rect.height() * 1.2
        
        # Scroll animation (Entrance/Exit)
        anim_progress = 1.0
        if self.anim_start_time is not None:
            elapsed = self.anim_start_time.msecsTo(QTime.currentTime())
            # For entering lines, we probably don't need high-precision QElapsedTimer 
            # as much as the karaoke fill, so QTime is roughly fine here, 
            # provided we don't wrap midnight during the 400ms transition.
            duration = 400
            if elapsed < duration:
                anim_progress = elapsed / duration
                anim_progress = 1.0 - (1.0 - anim_progress)**4
            else:
                self.anim_start_time = None
                
        def lerp(a, b, t):
            return a + (b - a) * t
            
        # Draw exiting line
        if anim_progress < 1.0 and self.prev_line and self.prev_line.text:
            y_pos = lerp(y_center, y_top_exit, anim_progress)
            painter.setOpacity(1.0 - anim_progress)
            h = rect.height() * 0.3
            r = QRectF(rect.left(), y_pos - h/2, rect.width(), h)
            self._draw_karaoke_line(painter, self.prev_line, r, is_main=True, no_wipe=True)
            painter.setOpacity(1.0)
            
        # Draw active line
        if self.current_line:
            y_pos = lerp(y_bottom, y_center, anim_progress)
            h = rect.height() * 0.65
            r = QRectF(rect.left(), y_pos - h/2, rect.width(), h)
            self._draw_karaoke_line(painter, self.current_line, r, is_main=True, no_wipe=False)
            
        # Draw next line
        if self.next_line and self.next_line.text:
            y_pos = lerp(y_below, y_bottom, anim_progress)
            h = rect.height() * 0.35
            r = QRectF(rect.left(), y_pos - h/2, rect.width(), h)
            self._draw_karaoke_line(painter, self.next_line, r, is_main=False, no_wipe=True)
            
    def _draw_karaoke_line(self, painter, line, rect, is_main=True, no_wipe=False):
        """Helper to draw a single line with selected animation effect"""
        if not line or not line.text:
            return
        
        base_font = self.font if is_main else self.secondary_font
        
        # Determine display mode and text
        display_text = line.text
        # Allow romanization for both main and secondary (next) lines
        has_romanized = hasattr(line, 'romanized_text') and line.romanized_text
        
        # Handle different display modes
        if self.romanization_mode == "Romanized" and has_romanized:
            # Show only romanized text (single line)
            self._draw_single_text_line(painter, line, rect, line.romanized_text, base_font, is_main, no_wipe)
            
        elif self.romanization_mode == "Both" and has_romanized:
            # Show both (dual-line)
            original_rect = QRectF(rect.left(), rect.top(), rect.width(), rect.height() * 0.55)
            romanized_rect = QRectF(rect.left(), rect.top() + rect.height() * 0.55, rect.width(), rect.height() * 0.45)
            
            # Draw original text (top line)
            self._draw_single_text_line(painter, line, original_rect, display_text, base_font, is_main, no_wipe)
            
            # Draw romanized text (bottom line, smaller, no wipe)
            # Use even smaller font for next line's romanized text
            rom_font = self.romanized_font if is_main else QFont("Arial", 18, QFont.Weight.Normal)
            self._draw_single_text_line(painter, line, romanized_rect, line.romanized_text, rom_font, is_main=False, no_wipe=True)
            
        else:
            # Original mode or no romanized text available
            self._draw_single_text_line(painter, line, rect, display_text, base_font, is_main, no_wipe)

    
    def _draw_single_text_line(self, painter, line, rect, text, base_font, is_main=True, no_wipe=False):
        """Draw a single text line with karaoke effect"""
        # Scale font to fit
        scaled_font = QFont(base_font)
        painter.setFont(scaled_font)
        fm = painter.fontMetrics()
        text_width = fm.horizontalAdvance(text)
        
        max_width = rect.width() - 40
        if text_width > max_width and max_width > 0:
            scale_factor = max_width / text_width
            new_size = max(12, int(scaled_font.pointSize() * scale_factor))
            scaled_font.setPointSize(new_size)
            painter.setFont(scaled_font)
            fm = painter.fontMetrics()
            text_width = fm.horizontalAdvance(text)
            
        x = rect.center().x() - (text_width / 2)
        y = rect.center().y() + (fm.ascent() / 2)
        
        # Build text path
        path = QPainterPath()
        path.addText(x, y, scaled_font, text)
        
        # Draw outline
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.setPen(QPen(self.outline_color, 4 if is_main else 2, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin))
        painter.drawPath(path)
        
        # Draw base fill
        painter.setPen(Qt.PenStyle.NoPen)
        color = self.inactive_color if is_main else self.secondary_color
        painter.setBrush(color)
        painter.drawPath(path)
        
        if is_main and not no_wipe:
            self._apply_animation(painter, line, path, x, text_width, fm, rect)


    def _apply_animation(self, painter, line, path, x, text_width, fm, rect):
        """Apply the selected animation type"""
        progress = self._get_line_progress(line)
        
        if self.animation_type == AnimationType.LINEAR_WIPE:
            self._draw_linear_wipe(painter, line, path, x, text_width, fm)
            
        elif self.animation_type == AnimationType.SYLLABLE_STEP:
            self._draw_syllable_step(painter, line, path, x, text_width, fm)
            
        elif self.animation_type == AnimationType.GLOW_PULSE:
            self._draw_glow_pulse(painter, line, path, x, text_width, fm, progress)
            
        elif self.animation_type == AnimationType.FADE_IN:
            self._draw_fade_in(painter, line, path, x, text_width, fm, progress)
            
        elif self.animation_type == AnimationType.BOUNCING_BALL:
            self._draw_bouncing_ball(painter, line, path, x, text_width, fm, rect)
            
        elif self.animation_type == AnimationType.RAINBOW:
            self._draw_rainbow(painter, line, path, x, text_width, fm)
            
        elif self.animation_type == AnimationType.TYPEWRITER:
            self._draw_typewriter(painter, line, path, x, text_width, fm)
            
        elif self.animation_type == AnimationType.SCALE_PULSE:
            self._draw_scale_pulse(painter, line, path, x, text_width, fm, rect, progress)

    def _get_line_progress(self, line):
        """Get overall progress through the line (0.0 to 1.0)"""
        duration = line.end_time - line.start_time
        if duration <= 0:
            return 1.0 if self.current_time > line.start_time else 0.0
        progress = (self.current_time - line.start_time) / duration
        return max(0.0, min(1.0, progress))

    def _draw_linear_wipe(self, painter, line, path, x, text_width, fm):
        """Linear left-to-right fill"""
        wipe_width = self._calculate_wipe_width(line, fm, text_width)
        
        if wipe_width > 0:
            painter.save()
            clip_rect = QRectF(x, 0, wipe_width, self.height())
            painter.setClipRect(clip_rect, Qt.ClipOperation.ReplaceClip)
            painter.setBrush(self.active_color)
            painter.drawPath(path)
            painter.restore()

    def _draw_syllable_step(self, painter, line, path, x, text_width, fm):
        """Words instantly fill when their time comes"""
        if not line.tokens:
            # Fallback to linear
            self._draw_linear_wipe(painter, line, path, x, text_width, fm)
            return
            
        # Calculate which words are complete
        full_text = line.text
        accumulated_width = 0.0
        search_start_idx = 0
        
        for token in line.tokens:
            idx = full_text.find(token.text.strip(), search_start_idx)
            if idx == -1:
                continue
                
            # Pre-token space
            pre_text = full_text[search_start_idx:idx]
            pre_width = fm.horizontalAdvance(pre_text)
            
            # Include space if word has started
            if self.current_time >= token.start_time:
                accumulated_width += pre_width
            
            token_width = fm.horizontalAdvance(token.text.strip())
            
            # Full word fill if we've reached its start time
            if self.current_time >= token.start_time:
                accumulated_width += token_width
                
            search_start_idx = idx + len(token.text.strip())
        
        if accumulated_width > 0:
            painter.save()
            clip_rect = QRectF(x, 0, accumulated_width, self.height())
            painter.setClipRect(clip_rect, Qt.ClipOperation.ReplaceClip)
            painter.setBrush(self.active_color)
            painter.drawPath(path)
            painter.restore()

    def _draw_glow_pulse(self, painter, line, path, x, text_width, fm, progress):
        """Active portion glows with a pulsing effect"""
        wipe_width = self._calculate_wipe_width(line, fm, text_width)
        
        if wipe_width > 0:
            # Calculate pulse (using time for animation)
            import math
            pulse = 0.5 + 0.5 * math.sin(self.current_time * 8)  # 8 Hz pulse
            
            painter.save()
            clip_rect = QRectF(x, 0, wipe_width, self.height())
            painter.setClipRect(clip_rect, Qt.ClipOperation.ReplaceClip)
            
            # Draw glow layer
            glow_color = QColor(self.glow_color)
            glow_color.setAlpha(int(100 + 100 * pulse))
            painter.setBrush(glow_color)
            painter.drawPath(path)
            
            # Draw main color
            painter.setBrush(self.active_color)
            painter.drawPath(path)
            painter.restore()

    def _draw_fade_in(self, painter, line, path, x, text_width, fm, progress):
        """Words fade from transparent to opaque"""
        if not line.tokens:
            # Linear fade based on progress
            painter.save()
            painter.setOpacity(progress)
            painter.setBrush(self.active_color)
            painter.drawPath(path)
            painter.restore()
            return
            
        # Per-word fade
        full_text = line.text
        search_start_idx = 0
        
        for token in line.tokens:
            idx = full_text.find(token.text.strip(), search_start_idx)
            if idx == -1:
                continue
                
            # Calculate word opacity
            word_progress = 0.0
            if self.current_time >= token.end_time:
                word_progress = 1.0
            elif self.current_time > token.start_time:
                duration = token.end_time - token.start_time
                if duration > 0:
                    word_progress = (self.current_time - token.start_time) / duration
            
            if word_progress > 0:
                # Draw this word with fade
                word_x = x + fm.horizontalAdvance(full_text[:idx])
                word_width = fm.horizontalAdvance(token.text.strip())
                
                painter.save()
                painter.setOpacity(word_progress)
                clip_rect = QRectF(word_x, 0, word_width, self.height())
                painter.setClipRect(clip_rect, Qt.ClipOperation.ReplaceClip)
                painter.setBrush(self.active_color)
                painter.drawPath(path)
                painter.restore()
                
            search_start_idx = idx + len(token.text.strip())

    def _calculate_wipe_width(self, line, fm, text_width):
        """Calculate how much of the text should be filled"""
        wipe_width = 0.0
        
        if line.tokens:
            full_text = line.text
            accumulated_width = 0.0
            search_start_idx = 0
            
            for token in line.tokens:
                idx = full_text.find(token.text.strip(), search_start_idx)
                if idx == -1:
                    continue
                
                pre_text = full_text[search_start_idx:idx]
                pre_width = fm.horizontalAdvance(pre_text)
                accumulated_width += pre_width
                
                token_width = fm.horizontalAdvance(token.text.strip())
                
                fill = 0
                if self.current_time >= token.end_time:
                    fill = token_width
                elif self.current_time <= token.start_time:
                    fill = 0
                else:
                    duration = token.end_time - token.start_time
                    if duration > 0:
                        ratio = (self.current_time - token.start_time) / duration
                        fill = token_width * ratio
                    else:
                        fill = token_width
                
                accumulated_width += fill
                
                if self.current_time < token.end_time:
                    wipe_width = accumulated_width
                    break
                    
                search_start_idx = idx + len(token.text.strip())
                wipe_width = accumulated_width
            
            # Fallback if token matching failed
            duration = line.end_time - line.start_time
            if duration > 0:
                should_be_progress = (self.current_time - line.start_time) / duration
                if wipe_width == 0 and should_be_progress > 0.05 and self.current_time > line.start_time:
                    wipe_width = text_width * min(1.0, should_be_progress)
        else:
            duration = line.end_time - line.start_time
            if duration > 0:
                progress = (self.current_time - line.start_time) / duration
                progress = max(0.0, min(1.0, progress))
                wipe_width = text_width * progress
            else:
                wipe_width = text_width if self.current_time > line.start_time else 0.0
        
        return wipe_width

    def _draw_bouncing_ball(self, painter, line, path, x, text_width, fm, rect):
        """Draw a bouncing ball above the current word"""
        import math
        
        # First draw the wipe effect (same as linear wipe)
        wipe_width = self._calculate_wipe_width(line, fm, text_width)
        
        if wipe_width > 0:
            painter.save()
            clip_rect = QRectF(x, 0, wipe_width, self.height())
            painter.setClipRect(clip_rect, Qt.ClipOperation.ReplaceClip)
            painter.setBrush(self.active_color)
            painter.drawPath(path)
            painter.restore()
        
        # Now draw the bouncing ball
        if not line.tokens:
            # Linear fallback - ball moves across text
            progress = self._get_line_progress(line)
            ball_x = x + (text_width * progress)
            ball_y = rect.center().y() - fm.height() / 2 - 20  # Above text
            
            # Bounce calculation
            bounce = abs(math.sin(self.current_time * 6)) * 15
            ball_y -= bounce
        else:
            # Word-level positioning
            full_text = line.text
            search_start_idx = 0
            ball_x = x
            ball_y = rect.center().y() - fm.height() / 2 - 20
            current_token = None
            token_progress = 0.0
            
            for i, token in enumerate(line.tokens):
                idx = full_text.find(token.text.strip(), search_start_idx)
                if idx == -1:
                    continue
                
                # Check if this is the current token
                if self.current_time >= token.start_time and self.current_time < token.end_time:
                    current_token = token
                    # Position ball at center of this word
                    word_x = x + fm.horizontalAdvance(full_text[:idx])
                    word_width = fm.horizontalAdvance(token.text.strip())
                    ball_x = word_x + word_width / 2
                    
                    # Calculate bounce based on position within word
                    duration = token.end_time - token.start_time
                    if duration > 0:
                        token_progress = (self.current_time - token.start_time) / duration
                    break
                elif self.current_time >= token.end_time:
                    # Past this token, position at next word
                    word_x = x + fm.horizontalAdvance(full_text[:idx])
                    word_width = fm.horizontalAdvance(token.text.strip())
                    ball_x = word_x + word_width / 2
                    
                search_start_idx = idx + len(token.text.strip())
            
            # Bounce animation - ball goes up and down within each word
            # Peaks at start of word, lands at end
            bounce_height = 25
            bounce = bounce_height * abs(math.sin((1.0 - token_progress) * math.pi))
            ball_y -= bounce
        
        # Draw the ball
        ball_radius = 12
        ball_color = QColor(255, 100, 100)  # Red ball
        
        painter.save()
        painter.setPen(Qt.PenStyle.NoPen)
        
        # Glow effect
        glow = QRadialGradient(ball_x, ball_y, ball_radius * 2)
        glow.setColorAt(0, QColor(255, 150, 150, 200))
        glow.setColorAt(0.5, QColor(255, 100, 100, 100))
        glow.setColorAt(1, QColor(255, 50, 50, 0))
        painter.setBrush(glow)
        painter.drawEllipse(QRectF(ball_x - ball_radius * 2, ball_y - ball_radius * 2, 
                                   ball_radius * 4, ball_radius * 4))
        
        # Main ball
        painter.setBrush(ball_color)
        painter.drawEllipse(QRectF(ball_x - ball_radius, ball_y - ball_radius, 
                                   ball_radius * 2, ball_radius * 2))
        
        # Highlight
        highlight = QColor(255, 255, 255, 180)
        painter.setBrush(highlight)
        painter.drawEllipse(QRectF(ball_x - ball_radius * 0.5, ball_y - ball_radius * 0.7, 
                                   ball_radius * 0.6, ball_radius * 0.4))
        painter.restore()

    def _draw_rainbow(self, painter, line, path, x, text_width, fm):
        """Draw with rainbow gradient that cycles through colors"""
        import math
        from PyQt6.QtGui import QLinearGradient
        
        wipe_width = self._calculate_wipe_width(line, fm, text_width)
        
        if wipe_width > 0:
            painter.save()
            clip_rect = QRectF(x, 0, wipe_width, self.height())
            painter.setClipRect(clip_rect, Qt.ClipOperation.ReplaceClip)
            
            # Create rainbow gradient based on time
            gradient = QLinearGradient(x, 0, x + text_width, 0)
            
            # Animate the starting hue
            hue_offset = (self.current_time * 50) % 360
            
            for i in range(7):  # 7 rainbow colors
                pos = i / 6.0
                hue = (hue_offset + i * 51) % 360  # 360/7 ≈ 51
                color = QColor.fromHsv(int(hue), 255, 255)
                gradient.setColorAt(pos, color)
            
            painter.setBrush(gradient)
            painter.drawPath(path)
            painter.restore()

    def _draw_typewriter(self, painter, line, path, x, text_width, fm):
        """Reveal text character by character"""
        progress = self._get_line_progress(line)
        
        # Calculate how many characters to show
        total_chars = len(line.text)
        chars_to_show = int(total_chars * progress)
        
        if chars_to_show > 0:
            # Calculate width of revealed portion
            revealed_text = line.text[:chars_to_show]
            revealed_width = fm.horizontalAdvance(revealed_text)
            
            painter.save()
            clip_rect = QRectF(x, 0, revealed_width, self.height())
            painter.setClipRect(clip_rect, Qt.ClipOperation.ReplaceClip)
            painter.setBrush(self.active_color)
            painter.drawPath(path)
            painter.restore()

    def _draw_scale_pulse(self, painter, line, path, x, text_width, fm, rect, progress):
        """Words enlarge when active"""
        import math
        
        # First draw the wipe
        wipe_width = self._calculate_wipe_width(line, fm, text_width)
        
        if wipe_width > 0:
            painter.save()
            clip_rect = QRectF(x, 0, wipe_width, self.height())
            painter.setClipRect(clip_rect, Qt.ClipOperation.ReplaceClip)
            painter.setBrush(self.active_color)
            painter.drawPath(path)
            painter.restore()
        
        # Apply scale animation based on progress
        # Scale up at the start, scale down at the end
        if progress > 0 and progress < 1:
            scale_factor = 1.0 + 0.1 * math.sin(progress * math.pi)  # Max 1.1x at middle
            
            painter.save()
            
            # Transform from center
            center_x = rect.center().x()
            center_y = rect.center().y()
            
            painter.translate(center_x, center_y)
            painter.scale(scale_factor, scale_factor)
            painter.translate(-center_x, -center_y)
            
            # Redraw with transform
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(self.active_color.lighter(110))
            painter.setOpacity(0.3)  # Subtle overlay
            painter.drawPath(path)
            
            painter.restore()
