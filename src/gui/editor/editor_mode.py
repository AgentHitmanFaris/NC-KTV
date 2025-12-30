"""
Lyrics Editor Mode for NC-KTV
Main interface for editing and synchronizing lyrics
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QSplitter, 
    QTextEdit, QTableWidget, QTableWidgetItem, 
    QPushButton, QLabel, QGroupBox, QHeaderView
)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QKeyEvent, QColor, QFont

from core.project import Project
from core.lyrics import LyricsData, LyricsLine
from gui.components.audio_player import AudioPlayer
from utils.config import Config


class EditorMode(QWidget):
    """Advanced editor for lyrics synchronization"""
    
    def __init__(self, config: Config, project: Project):
        super().__init__()
        self.config = config
        self.project = project
        self.lyrics_data = LyricsData()
        
        self._init_ui()
        self._setup_project()
        
    def _init_ui(self):
        """Initialize UI layout"""
        layout = QVBoxLayout(self)
        
        # Top toolbar
        toolbar = QHBoxLayout()
        self.lbl_project = QLabel(f"Project: {self.project.name}")
        toolbar.addWidget(self.lbl_project)
        toolbar.addStretch()
        
        btn_save = QPushButton("💾 Save Project")
        btn_save.clicked.connect(self._save_project)
        toolbar.addWidget(btn_save)
        
        layout.addLayout(toolbar)
        
        # Main Splitter (Left: Editor, Right: Preview)
        splitter = QSplitter(Qt.Orientation.Horizontal)
        
        # === Left Panel: Editor operations ===
        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)
        
        # Mode tabs (Input / Sync)
        self.mode_tabs = QHBoxLayout()
        
        btn_mode_input = QPushButton("📝 Text Input")
        btn_mode_input.setCheckable(True)
        btn_mode_input.setChecked(True)
        btn_mode_input.clicked.connect(lambda: self._switch_tab('input'))
        
        btn_mode_sync = QPushButton("⏱️ Sync Timing")
        btn_mode_sync.setCheckable(True)
        btn_mode_sync.clicked.connect(lambda: self._switch_tab('sync'))
        
        self.mode_btns = {'input': btn_mode_input, 'sync': btn_mode_sync}
        self.mode_tabs.addWidget(btn_mode_input)
        self.mode_tabs.addWidget(btn_mode_sync)
        
        # Auto-Transcribe Button
        btn_auto = QPushButton("✨ Auto-Transcribe (AI)")
        btn_auto.clicked.connect(self._start_auto_transcription)
        btn_auto.setStyleSheet("background-color: #673AB7; color: white; font-weight: bold;")
        self.mode_tabs.addWidget(btn_auto)
        
        self.mode_tabs.addStretch()
        
        left_layout.addLayout(self.mode_tabs)
        
        # Editors Stack
        self.editor_stack = QSplitter(Qt.Orientation.Vertical)
        
        # 1. Text Input Area
        self.text_editor = QTextEdit()
        self.text_editor.setPlaceholderText("Paste lyrics here...\nOne line per line.")
        self.text_editor.textChanged.connect(self._on_text_changed)
        
        # 2. Sync Table Area
        self.sync_table = QTableWidget()
        self.sync_table.setColumnCount(3)
        self.sync_table.setHorizontalHeaderLabels(["Start", "End", "Lyrics"])
        self.sync_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        self.sync_table.hide()
        
        self.editor_stack.addWidget(self.text_editor)
        self.editor_stack.addWidget(self.sync_table)
        
        left_layout.addWidget(self.editor_stack)
        
        # Audio Player Control
        self.player_group = QGroupBox("Audio Player")
        player_layout = QVBoxLayout()
        self.player = AudioPlayer()
        self.player.position_changed.connect(self._on_player_position)
        player_layout.addWidget(self.player)
        self.player_group.setLayout(player_layout)
        
        left_layout.addWidget(self.player_group)
        
        # === Right Panel: Preview ===
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_panel.setStyleSheet("background-color: #222; color: white;")
        
        self.lbl_preview = QLabel("Lyrics Preview")
        self.lbl_preview.setAlignment(Qt.AlignmentFlag.AlignCenter)
        font = QFont()
        font.setPointSize(24)
        font.setBold(True)
        self.lbl_preview.setFont(font)
        self.lbl_preview.setWordWrap(True)
        
        right_layout.addStretch()
        right_layout.addWidget(self.lbl_preview)
        right_layout.addStretch()
        
        # Add panels to splitter
        splitter.addWidget(left_panel)
        splitter.addWidget(right_panel)
        splitter.setStretchFactor(0, 4)
        splitter.setStretchFactor(1, 3)
        
        layout.addWidget(splitter)
        
        # Initial state
        self.active_line_index = -1
        self.sync_mode_active = False
        
    def _setup_project(self):
        """Load project data"""
        if self.project.instrumental_file and self.project.instrumental_file.exists():
            self.player.load_audio(self.project.instrumental_file)
        
    def _switch_tab(self, mode: str):
        """Switch between Input and Sync modes"""
        # Update buttons
        for m, btn in self.mode_btns.items():
            btn.setChecked(m == mode)
            
        if mode == 'input':
            self.text_editor.show()
            self.sync_table.hide()
            self.sync_mode_active = False
        else:
            self.text_editor.hide()
            self.sync_table.show()
            self._parse_lyrics_from_text()
            self.sync_mode_active = True
            
    def _on_text_changed(self):
        """Handle text input changes"""
        # We parse only when switching to sync mode to avoid overhead
        pass
        
    def _parse_lyrics_from_text(self):
        """Parse text editor content into LyricsData"""
        text = self.text_editor.toPlainText()
        self.lyrics_data.import_from_text(text)
        self._refresh_table()
        
    def _refresh_table(self):
        """Refresh sync table from data"""
        self.sync_table.setRowCount(len(self.lyrics_data.lines))
        
        for i, line in enumerate(self.lyrics_data.lines):
            # Start
            item_start = QTableWidgetItem(f"{line.start_time:.2f}")
            self.sync_table.setItem(i, 0, item_start)
            
            # End
            item_end = QTableWidgetItem(f"{line.end_time:.2f}")
            self.sync_table.setItem(i, 1, item_end)
            
            # Text
            item_text = QTableWidgetItem(line.text)
            self.sync_table.setItem(i, 2, item_text)
            
    def keyPressEvent(self, event: QKeyEvent):
        """Handle keyboard shortcuts for syncing"""
        if not self.sync_mode_active:
            super().keyPressEvent(event)
            return
            
        if event.key() == Qt.Key.Key_Space:
            # Tap to sync logic
            self._handle_tap_sync()
            
    def _handle_tap_sync(self):
        """Record timestamp for current line"""
        current_time = self.player.media_player.position() / 1000.0
        
        # Simple logic: Start of next line is end of previous
        if self.active_line_index < len(self.lyrics_data.lines) - 1:
            self.active_line_index += 1
            line = self.lyrics_data.lines[self.active_line_index]
            line.start_time = current_time
            
            # Set end time of previous line
            if self.active_line_index > 0:
                prev_line = self.lyrics_data.lines[self.active_line_index - 1]
                prev_line.end_time = current_time
            
            # Update UI
            self.sync_table.selectRow(self.active_line_index)
            self._refresh_table_row(self.active_line_index)
            if self.active_line_index > 0:
                self._refresh_table_row(self.active_line_index - 1)
                
            # Update Preview
            self.lbl_preview.setText(line.text)
            
    def _refresh_table_row(self, row):
        """Update single row in table"""
        line = self.lyrics_data.lines[row]
        self.sync_table.item(row, 0).setText(f"{line.start_time:.2f}")
        self.sync_table.item(row, 1).setText(f"{line.end_time:.2f}")

    def _on_player_position(self, ms):
        """Handle playback position updates for preview"""
        if not self.sync_mode_active:
            return
            
        current_seconds = ms / 1000.0
        
        # Find active line to display
        # This is linear search, could be optimized but fine for < 100 lines
        found_line = False
        for i, line in enumerate(self.lyrics_data.lines):
            if line.start_time <= current_seconds and (line.end_time == 0 or current_seconds < line.end_time):
                if self.lbl_preview.text() != line.text:
                    self.lbl_preview.setText(line.text)
                found_line = True
                break
        
        if not found_line:
            self.lbl_preview.setText("...")

    def _start_auto_transcription(self):
        """Start AI transcription"""
        from workers.transcription_worker import TranscriptionWorker
        from PyQt6.QtWidgets import QProgressDialog
        
        # Check if we have audio to transcribe
        # Use vocals if available, else source
        audio_source = self.project.vocals_file or self.project.source_file
        
        if not audio_source or not audio_source.exists():
            from PyQt6.QtWidgets import QMessageBox
            QMessageBox.warning(self, "No Audio", "No suitable audio file found for transcription.")
            return

        # Confirm if overwrite
        if self.lyrics_data.lines:
            reply = QMessageBox.question(
                self, "Confirm Overwrite", 
                "This will replace existing lyrics. Continue?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            if reply == QMessageBox.StandardButton.No:
                return

        # Progress Dialog
        self.progress = QProgressDialog("Initializing AI...", "Cancel", 0, 0, self)
        self.progress.setWindowTitle("AI Transcription")
        self.progress.setWindowModality(Qt.WindowModality.WindowModal)
        self.progress.show()
        
        # Worker
        self.transcriber = TranscriptionWorker(audio_source)
        self.transcriber.progress_updated.connect(self.progress.setLabelText)
        self.transcriber.transcription_complete.connect(self._on_transcription_complete)
        self.transcriber.error_occurred.connect(lambda e: (self.progress.close(), QMessageBox.critical(self, "Error", f"AI Error: {e}")))
        
        self.transcriber.start()
        
    def _on_transcription_complete(self, result):
        """Handle AI results"""
        self.progress.close()
        
        segments = result.get('segments', [])
        if not segments:
            QMessageBox.warning(self, "Result", "AI found no lyrics.")
            return
            
        self.lyrics_data.clear()
        for seg in segments:
            self.lyrics_data.add_line(seg['text'], seg['start'], seg['end'])
            
        self._refresh_table()
        
        # Update text editor view too
        text_content = "\n".join(l.text for l in self.lyrics_data.lines)
        self.text_editor.setPlainText(text_content)
        
        # Switch to sync view to show results
        self.mode_btns['sync'].click()
        QMessageBox.information(self, "Success", f"Transcribed {len(segments)} lines!")

    def _save_project(self):
        """Save project"""
        # TODO: Implement saving
        pass
