"""
Dialog for editing word-level timestamps
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QTableWidget, 
    QTableWidgetItem, QPushButton, QHeaderView, QLabel
)
from PyQt6.QtCore import Qt
from core.lyrics import LyricsLine, LyricsToken

class WordEditorDialog(QDialog):
    def __init__(self, line: LyricsLine, parent=None):
        super().__init__(parent)
        self.line = line
        self.tokens = [t for t in line.tokens] # Shallow copy list, tokens are refs
        # If no tokens, split text and distribute time evenly (rough init)
        if not self.tokens and line.text:
            self._init_tokens_from_line()
            
        self.setWindowTitle(f"Edit Word Timings: \"{line.text}\"")
        self.resize(600, 400)
        self._init_ui()
        
    def _init_tokens_from_line(self):
        """Create initial linear tokens if none exist"""
        words = self.line.text.split()
        if not words: return
        
        duration = self.line.duration
        start = self.line.start_time
        per_word = duration / len(words)
        
        current = start
        self.tokens = []
        for w in words:
            self.tokens.append(LyricsToken(w, current, current + per_word))
            current += per_word

    def _init_ui(self):
        layout = QVBoxLayout(self)
        
        # Table
        self.table = QTableWidget()
        self.table.setColumnCount(3)
        self.table.setHorizontalHeaderLabels(["Word", "Start (s)", "End (s)"])
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.table)
        
        self._refresh_table()
        
        # Buttons
        btn_box = QHBoxLayout()
        btn_distribute = QPushButton("📏 Distribute Evenly")
        btn_distribute.clicked.connect(self._distribute_evenly)
        btn_box.addWidget(btn_distribute)
        
        btn_box.addStretch()
        
        btn_cancel = QPushButton("Cancel")
        btn_cancel.clicked.connect(self.reject)
        
        btn_save = QPushButton("Save")
        btn_save.clicked.connect(self.accept)
        btn_save.setDefault(True)
        
        btn_box.addWidget(btn_cancel)
        btn_box.addWidget(btn_save)
        
        layout.addLayout(btn_box)
        
    def _refresh_table(self):
        self.table.setRowCount(len(self.tokens))
        for i, token in enumerate(self.tokens):
            self.table.setItem(i, 0, QTableWidgetItem(token.text))
            self.table.setItem(i, 1, QTableWidgetItem(f"{token.start_time:.3f}"))
            self.table.setItem(i, 2, QTableWidgetItem(f"{token.end_time:.3f}"))
            
    def _distribute_evenly(self):
        """Reset to even distribution"""
        self._init_tokens_from_line()
        self._refresh_table()
        
    def get_tokens(self):
        """Return updated tokens"""
        new_tokens = []
        for i in range(self.table.rowCount()):
            text = self.table.item(i, 0).text()
            try:
                start = float(self.table.item(i, 1).text())
                end = float(self.table.item(i, 2).text())
            except ValueError:
                start = self.line.start_time
                end = self.line.end_time
            new_tokens.append(LyricsToken(text, start, end))
        return new_tokens
