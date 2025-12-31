"""
Keyboard Shortcuts Help Dialog
Shows all available keyboard shortcuts in the editor
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QTableWidget, QTableWidgetItem, 
    QPushButton, QLabel, QHeaderView
)
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QFont


class ShortcutsDialog(QDialog):
    """Dialog showing all keyboard shortcuts"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Keyboard Shortcuts")
        self.setMinimumSize(500, 400)
        self._init_ui()
        
    def _init_ui(self):
        layout = QVBoxLayout(self)
        
        # Title
        title = QLabel("⌨️ Keyboard Shortcuts")
        title.setFont(QFont("Arial", 16, QFont.Weight.Bold))
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title)
        
        # Shortcuts table
        self.table = QTableWidget()
        self.table.setColumnCount(3)
        self.table.setHorizontalHeaderLabels(["Category", "Shortcut", "Action"])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.table.setAlternatingRowColors(True)
        
        # Define shortcuts
        shortcuts = [
            # Playback
            ("Playback", "Space", "Play / Pause"),
            ("Playback", "←", "Seek backward 5 seconds"),
            ("Playback", "→", "Seek forward 5 seconds"),
            
            # Sync
            ("Sync", "Space (while playing)", "Set start time for current line"),
            ("Sync", "↑", "Select previous line"),
            ("Sync", "↓", "Select next line"),
            
            # Editing
            ("Editing", "Ctrl + Z", "Undo"),
            ("Editing", "Ctrl + Y", "Redo"),
            ("Editing", "Ctrl + S", "Save project"),
            
            # Nudge
            ("Nudge", "A / D", "Nudge start time -/+ 0.1s"),
            ("Nudge", "Q / E", "Nudge end time -/+ 0.1s"),
            
            # General
            ("General", "F1", "Show this help"),
            ("General", "Escape", "Close dialog"),
        ]
        
        self.table.setRowCount(len(shortcuts))
        
        for row, (category, shortcut, action) in enumerate(shortcuts):
            self.table.setItem(row, 0, QTableWidgetItem(category))
            
            shortcut_item = QTableWidgetItem(shortcut)
            shortcut_item.setFont(QFont("Consolas", 10, QFont.Weight.Bold))
            self.table.setItem(row, 1, shortcut_item)
            
            self.table.setItem(row, 2, QTableWidgetItem(action))
        
        layout.addWidget(self.table)
        
        # Close button
        btn_close = QPushButton("Close")
        btn_close.clicked.connect(self.close)
        layout.addWidget(btn_close)
        
    def keyPressEvent(self, event):
        """Handle Escape key to close"""
        if event.key() == Qt.Key.Key_Escape:
            self.close()
        else:
            super().keyPressEvent(event)
