"""
Utility for generating text overlays and images
"""

from PyQt6.QtGui import QImage, QPainter, QColor, QFont, QPen, QLinearGradient, QBrush
from PyQt6.QtCore import Qt, QRect, QPoint

class ImageGenerator:
    @staticmethod
    def generate_credits_overlay(title: str, artist: str, show_version: bool, output_path: str, width=1920, height=1080):
        """
        Generate a transparent PNG with intro credits
        """
        # Create transparent image
        image = QImage(width, height, QImage.Format.Format_ARGB32)
        image.fill(QColor(0, 0, 0, 0))
        
        painter = QPainter(image)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        # Design Properties
        # Place text in bottom left or center? Let's go with a stylish "Music Video" style: Bottom Left, slightly offset
        
        margin_left = 100
        margin_bottom = 150
        
        # 1. Background Gradient (Subtle fade for readability)
        # Gradient from bottom-left corner
        gradient = QLinearGradient(0, height, width/2, height/2)
        gradient.setColorAt(0, QColor(0, 0, 0, 200)) # Dark at corner
        gradient.setColorAt(1, QColor(0, 0, 0, 0))   # Transparent out
        
        # Draw gradient rect (partial)
        painter.fillRect(0, height - 400, 800, 400, gradient)
        
        current_y = height - margin_bottom
        
        # 2. Draw Title
        title_font = QFont("Arial", 64, QFont.Weight.Bold)
        painter.setFont(title_font)
        painter.setPen(QColor(255, 255, 255))
        
        # Draw Title with Shadow
        title_rect = painter.boundingRect(QRect(margin_left, current_y - 100, 1000, 100), Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom, title)
        
        # Shadow
        painter.setPen(QColor(0, 0, 0, 150))
        painter.drawText(title_rect.translated(4, 4), Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom, title)
        
        # Main Text
        painter.setPen(QColor(255, 255, 255))
        painter.drawText(title_rect, Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom, title)
        
        current_y = title_rect.top() - 20 # Move up
        
        # 3. Draw Artist
        artist_font = QFont("Arial", 36, QFont.Weight.Normal)
        painter.setFont(artist_font)
        
        artist_text = f"{artist}"
        artist_rect = painter.boundingRect(QRect(margin_left, current_y - 60, 1000, 60), Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom, artist_text)
        
        # Shadow
        painter.setPen(QColor(0, 0, 0, 150))
        painter.drawText(artist_rect.translated(3, 3), Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom, artist_text)
        
        # Main Text
        painter.setPen(QColor(200, 200, 200)) # Slightly grey for artist
        painter.drawText(artist_rect, Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom, artist_text)
        
        current_y = artist_rect.top() - 40
        
        # 4. Draw Version/Branding (Small, above artist)
        if show_version:
            from core.project import Project
            version_text = f"NC-KTV v{Project.PROJECT_VERSION}"
            
            ver_font = QFont("Arial", 18, QFont.Weight.Light)
            ver_font.setLetterSpacing(QFont.SpacingType.AbsoluteSpacing, 2)
            painter.setFont(ver_font)
            
            ver_rect = painter.boundingRect(QRect(margin_left, current_y - 30, 1000, 30), Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom, version_text)
            
            # Decoration Line
            line_pen = QPen(QColor(233, 30, 99)) # Pink brand color
            line_pen.setWidth(4)
            painter.setPen(line_pen)
            painter.drawLine(margin_left, ver_rect.bottom() + 10, margin_left + 50, ver_rect.bottom() + 10)
            
            # Text
            painter.setPen(QColor(255, 255, 255, 180))
            painter.drawText(ver_rect, Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignBottom, version_text)
            
        painter.end()
        image.save(str(output_path))
        return True
