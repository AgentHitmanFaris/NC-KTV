"""
Lyrics Editor Mode for NC-KTV
Main interface for editing and synchronizing lyrics
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QSplitter, 
    QTextEdit, QTableWidget, QTableWidgetItem, 
    QPushButton, QLabel, QGroupBox, QHeaderView,
    QMessageBox, QProgressDialog, QFileDialog, QComboBox,
    QGraphicsView, QGraphicsScene, QAbstractItemView
)
from PyQt6.QtMultimediaWidgets import QVideoWidget, QGraphicsVideoItem
from PyQt6.QtCore import Qt, QTimer, QSizeF, QUrl
from PyQt6.QtGui import QKeyEvent, QColor, QFont
from PyQt6.QtMultimedia import QMediaPlayer, QAudioOutput
from pathlib import Path

from core.project import Project
from core.lyrics import LyricsData, LyricsLine
from gui.components.audio_player import AudioPlayer
from gui.components.karaoke_preview import KaraokePreviewWidget
from utils.config import Config


import logging
logger = logging.getLogger(__name__)

class AutoFitGraphicsView(QGraphicsView):
    def resizeEvent(self, event):
        super().resizeEvent(event)
        if self.scene():
            self.fitInView(self.scene().sceneRect(), Qt.AspectRatioMode.KeepAspectRatio)

class EditorMode(QWidget):
    """Advanced editor for lyrics synchronization"""
    
    def __init__(self, config: Config, project: Project):
        super().__init__()
        logger.info("[DEBUG] EditorMode.__init__ started")
        self.config = config
        self.project = project
        # Initialize lyrics data (clone from project to avoid reference issues, or use directly)
        # Using deep copy logic by re-creating to ensure safety
        if project.lyrics and project.lyrics.lines:
            logger.info("Loading existing lyrics from project")
            # We can use to_dict/from_dict to clone
            self.lyrics_data = LyricsData.from_dict(project.lyrics.to_dict())
        else:
            self.lyrics_data = LyricsData()
        
        self.is_dirty = False # Track unsaved changes
        
        # Initialize Undo/Redo manager
        from gui.editor.undo_manager import UndoManager
        self.undo_manager = UndoManager(max_history=50)
        self.undo_manager.set_initial_state(self.lyrics_data)
        
        # Initialize Auto-Save timer (every 5 minutes)
        from PyQt6.QtCore import QTimer
        self.auto_save_timer = QTimer(self)
        self.auto_save_timer.timeout.connect(self._auto_save)
        self.auto_save_timer.start(5 * 60 * 1000)  # 5 minutes
        
        logger.info(f"Initializing EditorMode for project: {project.name}")
        
        logger.info("[DEBUG] Calling _init_ui")
        self._init_ui()
        logger.info("[DEBUG] Calling _setup_project")
        self._setup_project()
        logger.info("[DEBUG] EditorMode.__init__ finished")

    def has_unsaved_changes(self):
        """Check if project has unsaved changes"""
        return self.is_dirty
        
    def _init_ui(self):
        """Initialize UI layout"""
        logger.info("[DEBUG] _init_ui started")
        layout = QVBoxLayout(self)
        
        # Top toolbar
        toolbar = QHBoxLayout()
        self.lbl_project = QLabel(f"Project: {self.project.name}")
        toolbar.addWidget(self.lbl_project)
        toolbar.addStretch()
        
        btn_save = QPushButton("💾 Save Project")
        btn_save.clicked.connect(self._save_project)
        toolbar.addWidget(btn_save)
        
        btn_export = QPushButton("🎬 Export Video")
        btn_export.clicked.connect(self._export_video)
        btn_export.setStyleSheet("background-color: #E91E63; color: white; font-weight: bold;")
        toolbar.addWidget(btn_export)
        
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
        self.sync_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows) # Highlight full row
        self.sync_table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.sync_table.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.sync_table.customContextMenuRequested.connect(self._show_context_menu)
        self.sync_table.itemChanged.connect(self._on_table_item_changed)
        self.sync_table.hide()
        
        self.editor_stack.addWidget(self.text_editor)
        self.editor_stack.addWidget(self.sync_table)
        
        left_layout.addWidget(self.editor_stack)
        
        # Fine-Tune Timing Controls
        self.nudge_group = QGroupBox("Fine-Tune Timing")
        nudge_layout = QHBoxLayout()
        
        # Start Time Controls
        nudge_layout.addWidget(QLabel("Start:"))
        btn_start_dec = QPushButton("⏪ -0.1s")
        btn_start_dec.clicked.connect(lambda: self._nudge_timestamp(-0.1, False))
        nudge_layout.addWidget(btn_start_dec)
        
        btn_start_inc = QPushButton("+0.1s ⏩")
        btn_start_inc.clicked.connect(lambda: self._nudge_timestamp(0.1, False))
        nudge_layout.addWidget(btn_start_inc)
        
        # Separator
        nudge_layout.addSpacing(20)
        
        # End Time Controls
        nudge_layout.addWidget(QLabel("End:"))
        btn_end_dec = QPushButton("⏪ -0.1s")
        btn_end_dec.clicked.connect(lambda: self._nudge_timestamp(-0.1, True))
        nudge_layout.addWidget(btn_end_dec)
        
        btn_end_inc = QPushButton("+0.1s ⏩")
        btn_end_inc.clicked.connect(lambda: self._nudge_timestamp(0.1, True))
        nudge_layout.addWidget(btn_end_inc)
        
        nudge_layout.addSpacing(20)
        
        # Loop Control
        self.chk_loop = QPushButton("🔁 Loop Line")
        self.chk_loop.setCheckable(True)
        self.chk_loop.setStyleSheet("QPushButton:checked { background-color: #4CAF50; color: white; }")
        nudge_layout.addWidget(self.chk_loop)
        
        # Word Editor Button
        btn_edit_words = QPushButton("📝 Edit Words")
        btn_edit_words.clicked.connect(self._open_word_editor)
        nudge_layout.addWidget(btn_edit_words)
        
        # Help Button
        btn_help = QPushButton("❓ Help (F1)")
        btn_help.clicked.connect(self._show_shortcuts_help)
        nudge_layout.addWidget(btn_help)
        
        nudge_layout.addStretch()
        self.nudge_group.setLayout(nudge_layout)
        left_layout.addWidget(self.nudge_group)
        
        # Audio Player Control
        self.player_group = QGroupBox("Audio Player")
        player_layout = QVBoxLayout()
        
        # Audio Source Select
        source_layout = QHBoxLayout()
        source_layout.addWidget(QLabel("Track:"))
        self.combo_source = QComboBox()
        self.combo_source.addItems(["Instrumental", "Vocals", "Original Source"])
        self.combo_source.currentIndexChanged.connect(self._change_audio_source)
        source_layout.addWidget(self.combo_source)
        
        # Animation Type Select
        source_layout.addWidget(QLabel("Animation:"))
        self.combo_animation = QComboBox()
        from gui.components.karaoke_preview import AnimationType
        self.combo_animation.addItems(AnimationType.all())
        self.combo_animation.currentTextChanged.connect(self._change_animation_type)
        source_layout.addWidget(self.combo_animation)
        
        source_layout.addStretch()
        
        player_layout.addLayout(source_layout)
        
        self.player = AudioPlayer()
        self.player.position_changed.connect(self._on_player_position)
        player_layout.addWidget(self.player)
        self.player_group.setLayout(player_layout)
        
        left_layout.addWidget(self.player_group)
        
        # === Right Panel: Preview ===
        right_panel = QWidget()
        right_panel.setMinimumWidth(300) # Ensure visibility
        # Use StackedLayout to overlay Lyrics on Video
        self.right_layout = QVBoxLayout(right_panel)
        self.right_layout.setContentsMargins(0,0,0,0)
        
        # Container for stack
        from PyQt6.QtWidgets import QGraphicsView, QGraphicsScene
        from PyQt6.QtMultimediaWidgets import QGraphicsVideoItem
        
        logger.info("[DEBUG] Initializing Graphics Scene")
        self.preview_scene = QGraphicsScene()
        # Important: Set fixed scene rect for FullHD coordinate system
        self.preview_scene.setSceneRect(0, 0, 1920, 1080)
        
        self.preview_view = AutoFitGraphicsView(self.preview_scene)
        self.preview_view.setStyleSheet("background: black; border: none;")
        self.preview_view.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.preview_view.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)

        # Layer 1: Video (Bottom)
        logger.info("[DEBUG] Initializing Video Item")
        self.video_item = QGraphicsVideoItem()
        self.video_item.setSize(QSizeF(1920, 1080)) # Default size, will scale
        self.preview_scene.addItem(self.video_item)
        
        # Layer 2: Lyrics (Top)
        logger.info("[DEBUG] Initializing Lyric Widget")
        self.preview_widget = KaraokePreviewWidget()
        self.preview_widget.resize(1920, 200) # Give it width
        self.lyrics_proxy = self.preview_scene.addWidget(self.preview_widget)
        # Position lyrics at bottom
        self.lyrics_proxy.setPos(0, 800)
        self.lyrics_proxy.setZValue(10) # Ensure on top
        
        # === Video Backend Setup ===
        # We need a separate player for the visual video track if the main player is playing audio stems
        from PyQt6.QtMultimedia import QMediaPlayer, QAudioOutput
        
        logger.info("[DEBUG] Initializing Background Video Player")
        self.bg_video_player = QMediaPlayer()
        self.video_audio_output = QAudioOutput()
        self.bg_video_player.setAudioOutput(self.video_audio_output)
        self.bg_video_player.setVideoOutput(self.video_item)
        
        # Mute background video (we only want visuals)
        self.video_audio_output.setVolume(0.0) 
        
        # Connect main player to background player for sync
        # Note: self.player.media_player is the source of truth
        self.player.state_changed.connect(self._sync_video_state)
        # self.player.position_changed is connected to _on_player_position, we can hook there too or separately
        
        self.right_layout.addWidget(self.preview_view)
        
        # Add panels to splitter
        splitter.addWidget(left_panel)
        splitter.addWidget(right_panel)
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)
        splitter.setCollapsible(1, False) # Prevent preview from being hidden
        
        layout.addWidget(splitter)
        
        # Status Bar
        self.status_bar = QLabel("Ready")
        self.status_bar.setStyleSheet("""
            QLabel {
                padding: 5px 10px;
                background-color: #2d2d2d;
                color: #aaa;
                border-top: 1px solid #444;
            }
        """)
        layout.addWidget(self.status_bar)
        
        # Initial state
        self.active_line_index = -1
        self.sync_mode_active = False
        self._update_status_bar()
        logger.info("[DEBUG] _init_ui finished")

    def cleanup(self):
        """Release resources"""
        logger.info("[DEBUG] EditorMode.cleanup called")
        try:
            if hasattr(self, 'player'):
                self.player.stop()
            
            if hasattr(self, 'bg_video_player'):
                self.bg_video_player.stop()
                self.bg_video_player.setSource(QUrl())
        except Exception as e:
            logger.error(f"Error during cleanup: {e}")
        
    def _setup_project(self):
        """Load project data"""
        logger.info(f"[DEBUG] _setup_project started")
        logger.info(f"Instrumental file path: {self.project.instrumental_file}")
        
        if self.project.instrumental_file:
            path = Path(self.project.instrumental_file)
            logger.info(f"Path exists check for {path}: {path.exists()}")
            
            if path.exists():
                logger.info("Loading instrumental into audio player")
                self.player.load_audio(path)
            else:
                logger.error(f"Instrumental file does not exist at: {path}")
        else:
             logger.warning("No instrumental file found in project data")
             
        # Load background video for preview (always load source if it's a video)
        if self.project.source_file:
             src_path = Path(self.project.source_file)
             if src_path.suffix.lower() in ['.mp4', '.avi', '.mkv', '.mov']:
                 logger.info(f"Loading background video source: {src_path}")
                 from PyQt6.QtCore import QUrl
                 self.bg_video_player.setSource(QUrl.fromLocalFile(str(src_path.absolute())))
                 # Pause immediately to be ready
                 logger.info("[DEBUG] Pausing bg_video_player")
                 self.bg_video_player.pause()

        # Populate UI with lyrics if they exist
        if self.lyrics_data.lines:
            logger.info("Populating UI with lyrics")
            text_content = "\n".join(l.text for l in self.lyrics_data.lines)
            self.text_editor.setPlainText(text_content)
            self._refresh_table()
            
            # If we have timestamps, switch to sync tab, otherwise input
            has_timestamps = any(l.end_time > 0 for l in self.lyrics_data.lines)
            if has_timestamps:
                self._switch_tab('sync')
        logger.info("[DEBUG] _setup_project finished")
             
    def _sync_video_state(self, is_playing):
        """Sync background video player state with main audio player"""
        if is_playing:
            self.bg_video_player.play()
        else:
            self.bg_video_player.pause()
            # Resync position on pause to ensure frames match
            self.bg_video_player.setPosition(self.player.media_player.position())

    def _on_player_position(self, ms):
        """Handle playback position updates for preview"""
        # Sync background video if it drifted too much (>100ms)
        # But don't spam setPosition as it causes stutter.
        # Ideally QMediaPlayer syncs reasonably well if started together.
        # We only force sync if drift is large.
        
        vid_pos = self.bg_video_player.position()
        diff = abs(vid_pos - ms)
        
        if diff > 200: # 200ms tolerance
             self.bg_video_player.setPosition(ms)
        
        if not self.sync_mode_active:
            return

        
    def _switch_tab(self, mode: str):
        """Switch between Input and Sync modes"""
        # Update buttons
        for m, btn in self.mode_btns.items():
            btn.setChecked(m == mode)
            
        if mode == 'input':
            self.text_editor.show()
            self.sync_table.hide()
            self.nudge_group.hide()
            self.sync_mode_active = False
        else:
            self.text_editor.hide()
            self.sync_table.show()
            self.nudge_group.show()
            self._parse_lyrics_from_text()
            self.sync_mode_active = True
            
            # Force update of preview based on current position
            if hasattr(self, 'player'):
                self._on_player_position(self.player.media_player.position())
            
    def _on_text_changed(self):
        """Handle text input changes"""
        # We parse only when switching to sync mode to avoid overhead
        pass
        
    def _parse_lyrics_from_text(self):
        """Parse text editor content into LyricsData"""
        text = self.text_editor.toPlainText().strip()
        
        # Check if text actually changed to avoid wiping timestamps
        current_text = "\n".join(l.text for l in self.lyrics_data.lines).strip()
        
        if text == current_text:
            return
            
        # Smart Update: If line count is same, just update text and KEEP timestamps
        new_lines_text = [l.strip() for l in text.splitlines() if l.strip()]
        
        if len(new_lines_text) == len(self.lyrics_data.lines):
            logger.info("Line count matches. Updating text while preserving timestamps.")
            for i, new_text in enumerate(new_lines_text):
                # If text changed, check if we can preserve specific word timings
                if self.lyrics_data.lines[i].text != new_text:
                    old_tokens = self.lyrics_data.lines[i].tokens
                    new_words = new_text.split()
                    
                    # 1. Update the main line text
                    self.lyrics_data.lines[i].text = new_text
                    
                    # 2. Try to map tokens if word count matches
                    if old_tokens and len(new_words) == len(old_tokens):
                        logger.info(f"Word count matches ({len(new_words)}), preserving token timings.")
                        for idx, token in enumerate(old_tokens):
                            token.text = new_words[idx]
                        # self.lyrics_data.lines[i].tokens is already modified in place
                    else:
                        logger.info("Word count mismatch or no tokens. resetting word timings.")
                        self.lyrics_data.lines[i].tokens = [] # Fallback to Linear Wipe
        else:
            logger.warning("Line count changed. Resetting timestamps.")
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
            
        # Nudge controls
        # Left/Right: Nudge Start Time
        # Shift + Left/Right: Nudge End Time
        elif event.key() == Qt.Key.Key_Left:
            amount = -0.1
            is_end = bool(event.modifiers() & Qt.KeyboardModifier.ShiftModifier)
            self._nudge_timestamp(amount, is_end)
            
        elif event.key() == Qt.Key.Key_Right:
            amount = 0.1
            is_end = bool(event.modifiers() & Qt.KeyboardModifier.ShiftModifier)
            self._nudge_timestamp(amount, is_end)
            
        elif event.key() == Qt.Key.Key_F1:
            self._show_shortcuts_help()
            
        # Undo/Redo (works in both modes)
        elif event.key() == Qt.Key.Key_Z and event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            self._perform_undo()
            
        elif event.key() == Qt.Key.Key_Y and event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            self._perform_redo()

    def _on_table_item_changed(self, item):
        """Handle manual edits in the table"""
        row = item.row()
        col = item.column()
        
        if row < 0 or row >= len(self.lyrics_data.lines):
            return
            
        line = self.lyrics_data.lines[row]
        text = item.text()
        
        try:
            if col == 0: # Start Time
                line.start_time = float(text)
            elif col == 1: # End Time
                line.end_time = float(text)
            elif col == 2: # Text
                # Update text
                line.text = text
                # Force update preview if this is the active line
                # Note: Token timings might be invalid now if word count changed
                # We could clear tokens here or try to re-align
                line.tokens = [] # Clear tokens to force linear wipe fallback
                if self.active_line_index == row:
                    self.preview_widget.set_text(text)
            
            self.is_dirty = True
                    
        except ValueError:
            pass # Ignore invalid number formats

    def _show_context_menu(self, position):
        """Show context menu for table"""
        from PyQt6.QtWidgets import QMenu
        menu = QMenu()
        edit_action = menu.addAction("🔍 Edit Word Timings")
        action = menu.exec(self.sync_table.mapToGlobal(position))
        
        if action == edit_action:
            self._open_word_editor()

    def _open_word_editor(self):
        """Open dialog to edit word-level timestamps"""
        rows = self.sync_table.selectionModel().selectedRows()
        if not rows: return
        
        row_idx = rows[0].row()
        line = self.lyrics_data.lines[row_idx]
        
        from gui.dialogs.word_editor import WordEditorDialog
        dialog = WordEditorDialog(line, self)
        
        if dialog.exec():
            # Apply changes
            new_tokens = dialog.get_tokens()
            self.lyrics_data.lines[row_idx].tokens = new_tokens
            
            # Recalculate line text to ensure consistency with tokens
            # This fixes issues where tokens don't match text, breaking the preview
            self.lyrics_data.lines[row_idx].text = " ".join(t.text for t in new_tokens)
            
            self._refresh_table_row(row_idx)
            self.preview_widget.set_line(self.lyrics_data.lines[row_idx])
            self.is_dirty = True

    def _nudge_timestamp(self, amount: float, is_end: bool = False):
        """Nudge timestamp of currently selected or active line"""
        self._save_undo_state()  # Save state before modification
        # Prefer selected row in table
        rows = self.sync_table.selectionModel().selectedRows()
        if rows:
            row_idx = rows[0].row()
        else:
            # Fallback to active playback line
            row_idx = self.active_line_index
            
        if 0 <= row_idx < len(self.lyrics_data.lines):
            line = self.lyrics_data.lines[row_idx]
            
            if is_end:
                line.end_time = max(line.start_time, line.end_time + amount)
            else:
                line.start_time = max(0.0, line.start_time + amount)
                # Ensure end time is not before start
                if line.end_time < line.start_time and line.end_time > 0:
                    line.end_time = line.start_time
            
            # Update UI
            self._refresh_table_row(row_idx)
            self.is_dirty = True
            
            # Select the row to show feedback
            self.sync_table.selectRow(row_idx)
            
    def _handle_tap_sync(self):
        """Record timestamp for current line"""
        self._save_undo_state()  # Save state before modification
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
            self.preview_widget.set_text(line.text)
            self.is_dirty = True
            
    def _refresh_table_row(self, row):
        """Update single row in table"""
        line = self.lyrics_data.lines[row]
        self.sync_table.item(row, 0).setText(f"{line.start_time:.2f}")
        self.sync_table.item(row, 1).setText(f"{line.end_time:.2f}")

    def _on_player_position(self, ms):
        """Update UI based on playback position"""
        # Sync background video if needed
        # ... logic ...
        
        current_seconds = ms / 1000.0
        
        # Debugging
        # logger.info(f"Position: {current_seconds:.2f}s, Lines: {len(self.lyrics_data.lines)}")
        
        # Find active line to display
        found_line = None
        current_index = -1
        
        for i, line in enumerate(self.lyrics_data.lines):
             # Log first line for debug
             # if i == 0: logger.info(f"Line 0: {line.start_time} - {line.end_time}")
             
             if line.start_time <= current_seconds:
                if line.end_time == 0 or current_seconds < line.end_time + 0.5:
                    found_line = line
                    current_index = i
                    break

        
        if found_line:
            self.active_line_index = current_index
            
            # Find next line
            next_line = None
            if current_index + 1 < len(self.lyrics_data.lines):
                next_line = self.lyrics_data.lines[current_index + 1]
                
            self.preview_widget.set_line(found_line, next_line)
            self.preview_widget.set_current_time(current_seconds)
            
            # --- Auto-Scroll Table ---
            # Avoid spamming selection updates if already selected
            current_selected = -1
            rows = self.sync_table.selectionModel().selectedRows()
            if rows:
                current_selected = rows[0].row()
                
            if current_selected != current_index:
                self.sync_table.selectRow(current_index)
                # Scroll to keep it visible (center)
                self.sync_table.scrollToItem(self.sync_table.item(current_index, 0), QTableWidget.ScrollHint.PositionAtCenter)
        else:
            # If no line active, check if we are simply BEFORE the first line or between lines
            # Strategy: Find the NEXT line to show as "inactive" main line, and line after that as bottom
            next_line = None
            next_next_line = None
            
            for i, line in enumerate(self.lyrics_data.lines):
                if line.start_time > current_seconds:
                    next_line = line
                    if i + 1 < len(self.lyrics_data.lines):
                        next_next_line = self.lyrics_data.lines[i+1]
                    break
            
            if next_line:
                 # Show next line as main, and the one after as secondary
                 self.preview_widget.set_line(next_line, next_next_line)
                 self.preview_widget.set_current_time(current_seconds)
            else:
                 # End of song or no lyrics
                 pass

    def _change_audio_source(self):
        """Switch between audio tracks while preserving position"""
        mode = self.combo_source.currentText()
        path = None
        
        if mode == "Instrumental":
            path = self.project.instrumental_file
        elif mode == "Vocals":
            path = self.project.vocals_file
        elif mode == "Original Source":
            path = self.project.source_file
            
        if not path or not path.exists():
            QMessageBox.warning(self, "Audio Error", f"File for '{mode}' not found.")
            # Revert to valid option if possible, or handle gracefully
            return

        # Preserve state
        current_pos = self.player.media_player.position()
        was_playing = self.player.media_player.playbackState() == self.player.media_player.PlaybackState.PlayingState
        
        logger.info(f"Switching audio to: {path}, was_playing={was_playing}, pos={current_pos}")
        
        # Stop current playback first
        self.player.media_player.stop()
        
        # Load new audio
        self.player.load_audio(path)
        
        # Use QTimer to restore position after media loads (more reliable than signal)
        from PyQt6.QtCore import QTimer
        
        def restore_state():
            self.player.media_player.setPosition(current_pos)
            if was_playing:
                self.player.media_player.play()
                # Also sync video
                self.bg_video_player.setPosition(current_pos)
                self.bg_video_player.play()
        
        # Wait 200ms for media to initialize before restoring
        QTimer.singleShot(200, restore_state)

    def _change_animation_type(self, anim_type: str):
        """Change the karaoke animation style"""
        self.preview_widget.set_animation_type(anim_type)

    def _show_shortcuts_help(self):
        """Show keyboard shortcuts help dialog"""
        from gui.dialogs.shortcuts_dialog import ShortcutsDialog
        dialog = ShortcutsDialog(self)
        dialog.exec()

    def _save_undo_state(self):
        """Save current state to undo stack before making changes"""
        self.undo_manager.push_state(self.lyrics_data)
        
    def _perform_undo(self):
        """Undo last lyrics change"""
        if not self.undo_manager.can_undo():
            return
            
        self.lyrics_data = self.undo_manager.undo(self.lyrics_data)
        self._refresh_table()
        self.is_dirty = True
        self._update_status_bar()
        logger.info(f"Undo performed. Stack: {self.undo_manager.get_undo_count()}")
        
    def _perform_redo(self):
        """Redo last undone change"""
        if not self.undo_manager.can_redo():
            return
            
        self.lyrics_data = self.undo_manager.redo(self.lyrics_data)
        self._refresh_table()
        self.is_dirty = True
        self._update_status_bar()
        logger.info(f"Redo performed. Stack: {self.undo_manager.get_redo_count()}")

    def _update_status_bar(self):
        """Update status bar with current state info"""
        lines_count = len(self.lyrics_data.lines)
        mode = "Sync Mode" if self.sync_mode_active else "Input Mode"
        undo_count = self.undo_manager.get_undo_count()
        redo_count = self.undo_manager.get_redo_count()
        dirty = "●" if self.is_dirty else ""
        
        status = f"{dirty} {lines_count} lines | {mode} | Undo: {undo_count} | Redo: {redo_count}"
        self.status_bar.setText(status.strip())

    def _start_auto_transcription(self):
        """Start AI transcription"""
        from workers.transcription_worker import TranscriptionWorker
        
        # Check if we have audio to transcribe
        # Audio Source Logic
        # Strictly prefer vocals file. Fallback to source only if vocals missing.
        # NEVER use instrumental.
        audio_source = self.project.vocals_file
        source_type = "Vocals Track"
        
        if not audio_source or not audio_source.exists():
            logger.warning("Vocals file not found, falling back to source file.")
            audio_source = self.project.source_file
            source_type = "Original Source Audio"
            
        if not audio_source or not audio_source.exists():
            QMessageBox.warning(self, "No Audio", "No suitable audio file found for transcription.\nNeed Vocals or Source file.")
            return

        # Explicitly warn if using source, as it might have music
        if source_type == "Original Source Audio":
             msg = f"Vocals track not found.\nUsing {source_type} instead.\nAccuracy might be lower if music is present."
             QMessageBox.information(self, "Audio Source", msg)
        else:
             logger.info(f"Transcribing using: {source_type}")

        # Confirm if overwrite
        if self.lyrics_data.lines:
            reply = QMessageBox.question(
                self, "Confirm Overwrite", 
                "This will replace existing lyrics. Continue?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            if reply == QMessageBox.StandardButton.No:
                return

        # Language selection
        langs = [
            "Auto Detect",
            "Malay (ms)",
            "Indonesian (id)",
            "English (en)",
            "Japanese (ja)",
            "Korean (ko)",
            "Chinese (zh)",
            "Vietnamese (vi)",
            "Thai (th)",
            "Tagalog (tl)"
        ]
        
        from PyQt6.QtWidgets import QInputDialog
        lang, ok = QInputDialog.getItem(
            self, "Select Language", 
            "Language for transcription:", 
            langs, 0, False
        )
        
        if not ok:
            return
            
        selected_lang_code = None
        selected_lang_name = lang # Keep full name for display
        
        if lang != "Auto Detect":
            # Extract code from "Malay (ms)" -> "ms"
            selected_lang_code = lang.split('(')[-1].strip(')')
            selected_lang_name = lang.split('(')[0].strip()

        # Progress Dialog
        self.progress = QProgressDialog(f"Initializing AI ({selected_lang_name})...", "Cancel", 0, 0, self)
        self.progress.setWindowTitle("AI Transcription")
        self.progress.setWindowModality(Qt.WindowModality.WindowModal)
        self.progress.show()
        
        # Worker
        self.transcriber = TranscriptionWorker(audio_source, language=selected_lang_code)
        
        # Custom progress update to avoid "ms" confusion
        def update_label(msg):
             if "Transcribing" in msg and selected_lang_name != "Auto Detect":
                 self.progress.setLabelText(f"Transcribing ({selected_lang_name})...")
             else:
                 self.progress.setLabelText(msg)
                 
        self.transcriber.progress_updated.connect(update_label)
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
            
        # 1. Switch tab FIRST to ensure any text parsing happens before we overwrite with AI data
        # However, switching triggers parse_from_text which wipes data.
        # So we manualy switch UI elements instead of calling _switch_tab logic
        self.mode_btns['sync'].setChecked(True)
        self.mode_btns['input'].setChecked(False)
        self.text_editor.hide()
        self.sync_table.show()
        self.sync_mode_active = True
            
        # 2. Populate data with timestamps
        self.lyrics_data.clear()
        for seg in segments:
            self.lyrics_data.add_line(seg['text'], seg['start'], seg['end'], seg.get('words', []))
        
        self.is_dirty = True
            
        self._refresh_table()
        
        # 3. Update text editor for consistency (won't trigger parse)
        text_content = "\n".join(l.text for l in self.lyrics_data.lines)
        self.text_editor.setPlainText(text_content)
        
        QMessageBox.information(self, "Success", f"Transcribed {len(segments)} lines!")

    def _save_project(self):
        """Save project"""
        # Default filename
        default_name = f"{self.project.project_name}.nctv"
        
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Project",
            default_name,
            "NC-KTV Project (*.nctv)"
        )
        
        if file_path:
            try:
                # Sync current editor lyrics to project object
                self.project.lyrics = self.lyrics_data
                
                self.project.save(Path(file_path))
                self.is_dirty = False
                QMessageBox.information(self, "Success", "Project saved successfully!")
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to save project:\n{e}")

    def _auto_save(self):
        """Auto-save project silently if there are unsaved changes"""
        if not self.is_dirty:
            return
            
        try:
            # Save to default location
            default_path = Path(f"output/{self.project.project_name}_autosave.nctv")
            self.project.lyrics = self.lyrics_data
            self.project.save(default_path)
            logger.info(f"Auto-saved project to: {default_path}")
        except Exception as e:
            logger.warning(f"Auto-save failed: {e}")

    def _export_video(self):
        """Export project to video with karaoke subtitles"""
        from utils.ass_generator import ASSGenerator
        from gui.dialogs.export_dialog import ExportDialog
        import subprocess
        
        # 1. Open Export Dialog
        default_name = f"{self.project.project_name}_karaoke.mp4"
        
        dialog = ExportDialog(self, default_name)
        if not dialog.exec():
            return
            
        options = dialog.get_options()
        out_path = options['path']
        style = options['style']
        animation = options['animation']
        
        # 2. Check source video
        # We need a visual source. Either the original file is video, or we need a background image/color.
        # User requested "if it has video mp4".
        
        # Determine input video
        video_input = self.project.source_file
        # Simple check: extensions
        is_video = video_input.suffix.lower() in ['.mp4', '.avi', '.mkv', '.mov']
        
        if not is_video:
            QMessageBox.warning(self, "Export Limit", "Currently only supports exporting if the source is a video file.\nStatic image background support coming soon.")
            return

        # 3. Generate Subtitles
        ass_gen = ASSGenerator(self.lyrics_data, style=style, animation=animation)
        ass_content = ass_gen.generate()
        
        temp_ass = self.project.get_temp_dir() / "subs.ass"
        with open(temp_ass, "w", encoding="utf-8") as f:
            f.write(ass_content)
            
        progress = QProgressDialog("Rendering Video... (Check Console)", "Cancel", 0, 0, self)
        progress.show()
        
        # 4. FFmpeg Command
        # We want to use the high quality separated audio if available
        inst_path = self.project.instrumental_file
        voc_path = self.project.vocals_file
        
        has_stems = inst_path and inst_path.exists() and voc_path and voc_path.exists()
        
        # Escape paths for filter_complex is tricky (windows backslashes).
        # Safest is to use forward slashes for filter graph
        ass_path_unix = str(temp_ass).replace("\\", "/").replace(":", "\\:")
        
        cmd = ["ffmpeg", "-y"]
        
        if has_stems:
            # Inputs: 0:Video, 1:Inst, 2:Vocals
            cmd.extend(["-i", str(video_input)])
            cmd.extend(["-i", str(inst_path)])
            cmd.extend(["-i", str(voc_path)])
            
            # Filter: Burn subs on video, Mix audio
            # Note: fonts might require fontconfig or valid path. Standard Arial usually ok.
            filter_complex = f"[0:v]ass='{ass_path_unix}'[v];[1:a][2:a]amix=inputs=2:duration=first[a]"
            cmd.extend(["-filter_complex", filter_complex])
            cmd.extend(["-map", "[v]", "-map", "[a]"])
        else:
            # Just burn subs, keep original audio
            cmd.extend(["-i", str(video_input)])
            cmd.extend(["-vf", f"ass='{ass_path_unix}'"])
            cmd.extend(["-c:a", "copy"])
        
        cmd.append(str(out_path))
        
        logger.info(f"Running export command: {cmd}")
        
        try:
            # Run blocking for now (should be worker, but quick implementation)
            subprocess.run(cmd, check=True)
            progress.close()
            QMessageBox.information(self, "Success", f"Exported to:\n{out_path}")
            
            # Auto-play result?
            import os
            os.startfile(out_path)
            
        except Exception as e:
            progress.close()
            QMessageBox.critical(self, "Export Failed", f"FFmpeg Error:\n{e}")
