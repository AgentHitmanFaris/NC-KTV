"""
Curve Editor Widget for NC-KTV
Visual editor for Bezier animation curves
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QPushButton,
    QWidget, QLabel
)
from PyQt6.QtCore import Qt, QPointF, QRectF, pyqtSignal
from PyQt6.QtGui import QPainter, QPen, QBrush, QColor, QPainterPath
from typing import List, Optional, Tuple


class CurveCanvas(QWidget):
    """Canvas for drawing and editing Bezier curves"""
    
    curve_changed = pyqtSignal(list)  # Emits list of control points
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(400, 400)
        
        # Control points for cubic Bezier (4 points)
        # Format: [(x, y), (x, y), (x, y), (x, y)]
        # First and last are anchored at (0,0) and (1,1)
        self.control_points = [
            (0.0, 0.0),      # Start (fixed)
            (0.25, 0.25),    # Control point 1 (movable)
            (0.75, 0.75),    # Control point 2 (movable)
            (1.0, 1.0)       # End (fixed)
        ]
        
        self.dragging_point = None
        self.point_radius = 8
        
    def set_preset(self, preset_name: str):
        """Set curve to a preset"""
        presets = {
            'linear': [(0.0, 0.0), (0.33, 0.33), (0.66, 0.66), (1.0, 1.0)],
            'ease_in': [(0.0, 0.0), (0.42, 0.0), (1.0, 1.0), (1.0, 1.0)],
            'ease_out': [(0.0, 0.0), (0.0, 0.0), (0.58, 1.0), (1.0, 1.0)],
            'ease_in_out': [(0.0, 0.0), (0.42, 0.0), (0.58, 1.0), (1.0, 1.0)],
        }
        
        if preset_name in presets:
            self.control_points = presets[preset_name]
            self.curve_changed.emit(self.control_points)
            self.update()
    
    def set_custom_points(self, points: Optional[List[Tuple[float, float]]]):
        """Set custom control points"""
        if points and len(points) == 4:
            self.control_points = points
            self.update()
    
    def get_control_points(self) -> List[Tuple[float, float]]:
        """Get current control points"""
        return self.control_points.copy()
    
    def _widget_to_curve(self, x: int, y: int) -> Tuple[float, float]:
        """Convert widget coordinates to curve coordinates (0-1)"""
        margin = 40
        width = self.width() - 2 * margin
        height = self.height() - 2 * margin
        
        curve_x = (x - margin) / width
        curve_y = 1.0 - (y - margin) / height  # Flip Y axis
        
        # Clamp to 0-1
        curve_x = max(0.0, min(1.0, curve_x))
        curve_y = max(0.0, min(1.0, curve_y))
        
        return (curve_x, curve_y)
    
    def _curve_to_widget(self, curve_x: float, curve_y: float) -> Tuple[int, int]:
        """Convert curve coordinates to widget coordinates"""
        margin = 40
        width = self.width() - 2 * margin
        height = self.height() - 2 * margin
        
        x = int(margin + curve_x * width)
        y = int(margin + (1.0 - curve_y) * height)  # Flip Y axis
        
        return (x, y)
    
    def paintEvent(self, event):
        """Draw the curve and control points"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        margin = 40
        width = self.width() - 2 * margin
        height = self.height() - 2 * margin
        
        # Draw background
        painter.fillRect(self.rect(), QColor(240, 240, 245))
        
        # Draw grid
        painter.setPen(QPen(QColor(200, 200, 200), 1))
        for i in range(1, 4):
            # Vertical lines
            x = margin + (i * width // 4)
            painter.drawLine(x, margin, x, margin + height)
            # Horizontal lines
            y = margin + (i * height // 4)
            painter.drawLine(margin, y, margin + width, y)
        
        # Draw border
        painter.setPen(QPen(QColor(100, 100, 100), 2))
        painter.drawRect(margin, margin, width, height)
        
        # Draw diagonal reference line (linear)
        painter.setPen(QPen(QColor(180, 180, 180), 1, Qt.PenStyle.DashLine))
        painter.drawLine(margin, margin + height, margin + width, margin)
        
        # Draw Bezier curve
        path = QPainterPath()
        
        # Convert control points to widget coordinates
        p0 = QPointF(*self._curve_to_widget(*self.control_points[0]))
        p1 = QPointF(*self._curve_to_widget(*self.control_points[1]))
        p2 = QPointF(*self._curve_to_widget(*self.control_points[2]))
        p3 = QPointF(*self._curve_to_widget(*self.control_points[3]))
        
        path.moveTo(p0)
        path.cubicTo(p1, p2, p3)
        
        painter.setPen(QPen(QColor(50, 150, 255), 3))
        painter.drawPath(path)
        
        # Draw control lines
        painter.setPen(QPen(QColor(150, 150, 150), 1, Qt.PenStyle.DashLine))
        painter.drawLine(p0, p1)
        painter.drawLine(p2, p3)
        
        # Draw control points
        for i, (cx, cy) in enumerate(self.control_points):
            wx, wy = self._curve_to_widget(cx, cy)
            
            # Different colors for fixed vs movable points
            if i == 0 or i == 3:
                color = QColor(100, 100, 100)  # Fixed points (darker)
            else:
                color = QColor(50, 150, 255)  # Movable points (blue)
            
            painter.setPen(QPen(QColor(255, 255, 255), 2))
            painter.setBrush(QBrush(color))
            painter.drawEllipse(wx - self.point_radius, wy - self.point_radius,
                              self.point_radius * 2, self.point_radius * 2)
        
        # Draw labels
        painter.setPen(QPen(QColor(0, 0, 0)))
        painter.drawText(margin - 30, margin + height + 5, "0.0")
        painter.drawText(margin - 30, margin + 5, "1.0")
        painter.drawText(margin - 5, margin + height + 30, "0.0")
        painter.drawText(margin + width - 10, margin + height + 30, "1.0")
        
        painter.drawText(10, margin + height // 2, "Output")
        painter.drawText(margin + width // 2 - 20, self.height() - 10, "Input")
    
    def mousePressEvent(self, event):
        """Handle mouse press to start dragging"""
        if event.button() == Qt.MouseButton.LeftButton:
            # Check if clicking on a control point (excluding first and last)
            for i in range(1, 3):  # Only middle two points are draggable
                cx, cy = self.control_points[i]
                wx, wy = self._curve_to_widget(cx, cy)
                
                distance = ((event.pos().x() - wx)**2 + (event.pos().y() - wy)**2)**0.5
                if distance <= self.point_radius:
                    self.dragging_point = i
                    break
    
    def mouseMoveEvent(self, event):
        """Handle mouse move to drag control point"""
        if self.dragging_point is not None:
            curve_x, curve_y = self._widget_to_curve(event.pos().x(), event.pos().y())
            self.control_points[self.dragging_point] = (curve_x, curve_y)
            self.curve_changed.emit(self.control_points)
            self.update()
    
    def mouseReleaseEvent(self, event):
        """Handle mouse release to stop dragging"""
        if event.button() == Qt.MouseButton.LeftButton:
            self.dragging_point = None


class CurveEditorDialog(QDialog):
    """Dialog for editing animation curves"""
    
    def __init__(self, initial_points: Optional[List[Tuple[float, float]]] = None, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Animation Curve Editor")
        self.setMinimumSize(500, 550)
        
        self._init_ui()
        
        if initial_points:
            self.canvas.set_custom_points(initial_points)
    
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        
        # Info label
        info = QLabel("Drag the blue control points to shape the animation curve")
        info.setStyleSheet("color: #666; padding: 10px;")
        layout.addWidget(info)
        
        # Canvas
        self.canvas = CurveCanvas()
        layout.addWidget(self.canvas)
        
        # Preset buttons
        preset_layout = QHBoxLayout()
        preset_layout.addWidget(QLabel("Presets:"))
        
        btn_linear = QPushButton("Linear")
        btn_linear.clicked.connect(lambda: self.canvas.set_preset('linear'))
        preset_layout.addWidget(btn_linear)
        
        btn_ease_in = QPushButton("Ease In")
        btn_ease_in.clicked.connect(lambda: self.canvas.set_preset('ease_in'))
        preset_layout.addWidget(btn_ease_in)
        
        btn_ease_out = QPushButton("Ease Out")
        btn_ease_out.clicked.connect(lambda: self.canvas.set_preset('ease_out'))
        preset_layout.addWidget(btn_ease_out)
        
        btn_ease_in_out = QPushButton("Ease In-Out")
        btn_ease_in_out.clicked.connect(lambda: self.canvas.set_preset('ease_in_out'))
        preset_layout.addWidget(btn_ease_in_out)
        
        preset_layout.addStretch()
        layout.addLayout(preset_layout)
        
        # Bottom buttons
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        
        btn_reset = QPushButton("Reset")
        btn_reset.clicked.connect(lambda: self.canvas.set_preset('linear'))
        btn_layout.addWidget(btn_reset)
        
        btn_apply = QPushButton("Apply")
        btn_apply.clicked.connect(self.accept)
        btn_layout.addWidget(btn_apply)
        
        btn_cancel = QPushButton("Cancel")
        btn_cancel.clicked.connect(self.reject)
        btn_layout.addWidget(btn_cancel)
        
        layout.addLayout(btn_layout)
    
    def get_curve_points(self) -> List[Tuple[float, float]]:
        """Get the current curve control points"""
        return self.canvas.get_control_points()
