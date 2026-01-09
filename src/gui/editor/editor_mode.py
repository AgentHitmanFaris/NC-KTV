"""
Lyrics Editor Mode for NC-KTV
Main interface for editing and synchronizing lyrics
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QSplitter, 
    QTextEdit, QTableWidget, QTableWidgetItem, 
    QPushButton, QLabel, QGroupBox, QHeaderView,
    QMessageBox, QProgressDialog, QFileDialog, QComboBox,
    QGraphicsView, QGraphicsScene, QAbstractItemView, QDoubleSpinBox,
    QLineEdit
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
from gui.components.timeline_widget import TimelineWidget
from utils.config import Config
from utils.subtitle_parser import export_subtitle, detect_subtitle_format, get_import_filter


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
        
        btn_import = QPushButton("📥 Import...")
        btn_import.clicked.connect(self._import_from_project)
        btn_import.setToolTip("Import components from another project")
        toolbar.addWidget(btn_import)
        
        btn_save = QPushButton("💾 Save Project")
        btn_save.clicked.connect(self._save_project)
        toolbar.addWidget(btn_save)
        
        # Timing Calibration button (Phase 6.3 - Timing Sync)
        btn_timing = QPushButton("⚙️ Timing Calibration")
        btn_timing.setToolTip("Fix timing drift & validate sample rates")
        btn_timing.clicked.connect(self._open_timing_calibration)
        toolbar.addWidget(btn_timing)

        # Export Subtitles Button
        btn_export_subs = QPushButton("📝 Export Subs")
        btn_export_subs.setToolTip("Export lyrics to SRT, ASS, VTT, etc.")
        btn_export_subs.clicked.connect(self._export_subtitles)
        toolbar.addWidget(btn_export_subs)
        
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
        self.mode_tabs.addWidget(btn_mode_input)
        
        btn_import_file = QPushButton("📄 Import Lyrics...")
        btn_import_file.clicked.connect(self._import_lyrics_file)
        btn_import_file.setToolTip("Import lyrics from .txt or .lrc file")
        self.mode_tabs.addWidget(btn_import_file)
        
        btn_mode_sync = QPushButton("🎵 Sync Mode")
        btn_mode_sync.setCheckable(True)
        btn_mode_sync.clicked.connect(lambda: self._switch_tab('sync'))
        self.mode_tabs.addWidget(btn_mode_sync)
        
        self.mode_btns = {'input': btn_mode_input, 'sync': btn_mode_sync}
        
        btn_auto = QPushButton("✨ Auto-Transcribe (AI)")
        btn_auto.clicked.connect(self._start_auto_transcription)
        btn_auto.setStyleSheet("background-color: #673AB7; color: white; font-weight: bold;")
        self.mode_tabs.addWidget(btn_auto)
        
        # Timeline View Toggle (Phase 6)
        self.btn_timeline = QPushButton("📊 Timeline View")
        self.btn_timeline.setCheckable(True)
        self.btn_timeline.setChecked(False)
        self.btn_timeline.clicked.connect(self._toggle_timeline_view)
        self.btn_timeline.setToolTip("Show/hide multi-track timeline editor")
        self.mode_tabs.addWidget(self.btn_timeline)
        
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
        
        # Jump to timestamp when clicking on row
        self.sync_table.itemClicked.connect(self._on_table_row_clicked)
        
        self.sync_table.hide()
        
        # 3. Timeline View (Phase 6)
        self.timeline_widget = TimelineWidget()
        self.timeline_widget.set_timeline_data(self.project.timeline)
        self.timeline_widget.playhead_moved.connect(self._on_timeline_seek)
        self.timeline_widget.clip_selected.connect(self._on_clip_selected)
        self.timeline_widget.clip_moved.connect(self._on_clip_moved)
        self.timeline_widget.clip_resized.connect(self._on_clip_resized)
        self.timeline_widget.clip_split.connect(self._on_clip_split)
        self.timeline_widget.clip_deleted.connect(self._on_clip_deleted)
        self.timeline_widget.effect_requested.connect(self._on_effect_requested)
        self.timeline_widget.hide()  # Hidden by default, can be toggled
        
        self.editor_stack.addWidget(self.text_editor)
        
        # Add vertical splitter for sync table and timeline
        self.sync_timeline_splitter = QSplitter(Qt.Orientation.Vertical)
        self.sync_timeline_splitter.addWidget(self.sync_table)
        self.sync_timeline_splitter.addWidget(self.timeline_widget)
        self.sync_timeline_splitter.setStretchFactor(0, 2)  # Table gets more space
        self.sync_timeline_splitter.setStretchFactor(1, 1)  # Timeline gets less
        
        self.editor_stack.addWidget(self.sync_timeline_splitter)
        
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
        
        nudge_layout.addSpacing(15)
        
        # Global Timing Offset
        nudge_layout.addWidget(QLabel("Global Offset:"))
        self.spin_timing_offset = QDoubleSpinBox()
        self.spin_timing_offset.setRange(-5.0, 5.0)
        self.spin_timing_offset.setValue(0.0)
        self.spin_timing_offset.setSingleStep(0.1)
        self.spin_timing_offset.setDecimals(1)
        self.spin_timing_offset.setSuffix(" s")
        self.spin_timing_offset.setToolTip("Shift ALL lyrics (fixes AI delay)\nTry -0.5 to -1.0")
        self.spin_timing_offset.setMaximumWidth(95)
        self.spin_timing_offset.valueChanged.connect(self._apply_timing_offset)
        nudge_layout.addWidget(self.spin_timing_offset)
        
        nudge_layout.addSpacing(15)
        
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
        
        source_layout.addSpacing(10)
        
        # Preview Mode Dropdown
        source_layout.addWidget(QLabel("Preview:"))
        self.combo_preview = QComboBox()
        self.combo_preview.addItems(["With Video", "Lyrics Only"])
        self.combo_preview.setToolTip("Preview mode: With Video shows background, Lyrics Only shows centered text")
        self.combo_preview.currentTextChanged.connect(self._change_preview_mode)
        source_layout.addWidget(self.combo_preview)
        
        source_layout.addSpacing(10)
        
        # Timing Mode (Karaoke vs Lyrics Video)
        source_layout.addWidget(QLabel("Timing:"))
        self.combo_timing_mode = QComboBox()
        self.combo_timing_mode.addItems(["Karaoke", "Lyrics Video"])
        self.combo_timing_mode.setToolTip("Karaoke: Show next line early for reading ahead\nLyrics Video: Show next line after singing")
        self.combo_timing_mode.setCurrentText("Karaoke")
        self.combo_timing_mode.currentTextChanged.connect(self._change_timing_mode)
        source_layout.addWidget(self.combo_timing_mode)
        
        # Preview Time for Karaoke mode
        source_layout.addWidget(QLabel("Preview:"))
        self.spin_preview_time = QDoubleSpinBox()
        self.spin_preview_time.setRange(0.0, 5.0)
        self.spin_preview_time.setValue(1.0)  # Default 1 second (reduced from 2)
        self.spin_preview_time.setSingleStep(0.1)
        self.spin_preview_time.setDecimals(1)
        self.spin_preview_time.setSuffix(" s")
        self.spin_preview_time.setToolTip("How early to show lyrics in Karaoke mode\n(0.5-1.5s recommended)")
        self.spin_preview_time.setMaximumWidth(85)
        source_layout.addWidget(self.spin_preview_time)
        
        source_layout.addSpacing(10)
        
        # Animation Type Select
        source_layout.addWidget(QLabel("Animation:"))
        self.combo_animation = QComboBox()
        from gui.components.karaoke_preview import AnimationType
        self.combo_animation.addItems(AnimationType.all())
        self.combo_animation.currentTextChanged.connect(self._change_animation_type)
        source_layout.addWidget(self.combo_animation)
        
        # Color Picker Button
        from PyQt6.QtWidgets import QColorDialog
        self.btn_color = QPushButton("🎨")
        self.btn_color.setToolTip("Custom Fill Color")
        self.btn_color.setFixedWidth(40)
        self.btn_color.clicked.connect(self._pick_color)
        source_layout.addWidget(self.btn_color)
        
        source_layout.addSpacing(10)
        
        # Playback Speed Control
        source_layout.addWidget(QLabel("Speed:"))
        self.combo_speed = QComboBox()
        self.combo_speed.addItems(["0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"])
        self.combo_speed.setCurrentText("1.0x")
        self.combo_speed.setToolTip("Playback speed for easier synchronization")
        self.combo_speed.currentTextChanged.connect(self._change_playback_speed)
        source_layout.addWidget(self.combo_speed)
        
        source_layout.addSpacing(10)
        
        # Romanization Controls
        source_layout.addWidget(QLabel("Display:"))
        self.combo_romanize_mode = QComboBox()
        self.combo_romanize_mode.addItems(["Original", "Romanized", "Both"])
        self.combo_romanize_mode.setToolTip("Choose text display mode:\n• Original: Show original script\n• Romanized: Show romanized text only\n• Both: Show both (dual-line)")
        self.combo_romanize_mode.setCurrentText("Original")
        self.combo_romanize_mode.currentTextChanged.connect(self._change_romanize_mode)
        source_layout.addWidget(self.combo_romanize_mode)
        
        self.combo_language = QComboBox()
        self.combo_language.addItems(["Auto", "Korean", "Japanese", "Hindi", "Tamil"])
        self.combo_language.setToolTip("Language for romanization (Auto-detect recommended)")
        self.combo_language.setCurrentText("Auto")
        source_layout.addWidget(self.combo_language)
        
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
    
    def closeEvent(self, event):
        """Handle close event - prompt to save if there are unsaved changes"""
        if self.has_unsaved_changes():
            reply = QMessageBox.question(
                self,
                "Unsaved Changes",
                "You have unsaved changes. Do you want to save before closing?",
                QMessageBox.StandardButton.Save | 
                QMessageBox.StandardButton.Discard | 
                QMessageBox.StandardButton.Cancel,
                QMessageBox.StandardButton.Save
            )
            
            if reply == QMessageBox.StandardButton.Save:
                self._save_project()
                event.accept()
            elif reply == QMessageBox.StandardButton.Discard:
                event.accept()
            else:  # Cancel
                event.ignore()
        else:
            event.accept()
        
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
        
        # Initialize timeline with default tracks (Phase 6.1 Polish)
        self._initialize_timeline()
        
        logger.info("[DEBUG] _setup_project finished")
             
    def _sync_video_state(self, is_playing):
        """Sync background video player state with main audio player"""
        
        # Update preview widget state (stops animation timer if paused)
        if hasattr(self, 'preview_widget'):
            self.preview_widget.set_playing(is_playing)
            
        if is_playing:
            self.bg_video_player.play()
        else:
            self.bg_video_player.pause()
            # Resync position on pause to ensure frames match
            self.bg_video_player.setPosition(self.player.media_player.position())


        
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
        # Mark as dirty when text is edited
        self.is_dirty = True
        # We parse only when switching to sync mode to avoid overhead
        
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
        """Handle keyboard shortcuts"""
        # Global shortcuts (work in all modes)
        if event.key() == Qt.Key.Key_Z and event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            self._perform_undo()
            event.accept()
            return
        
        # Redo: Support both Ctrl+Y and Ctrl+Shift+Z (standard on some systems)
        elif (event.key() == Qt.Key.Key_Y and event.modifiers() & Qt.KeyboardModifier.ControlModifier) or \
             (event.key() == Qt.Key.Key_Z and event.modifiers() & (Qt.KeyboardModifier.ControlModifier | Qt.KeyboardModifier.ShiftModifier)):
            self._perform_redo()
            event.accept()
            return
        
        # Timeline playback shortcuts (J/K/L - Phase 6.1 Polish)
        # CRITICAL: Only handle if NOT editing in table or text editor
        elif event.key() == Qt.Key.Key_J:  # Play backwards (rewind)
            focused_widget = self.focusWidget()
            # Skip if editing in table or text editor
            if isinstance(focused_widget, (QLineEdit, QTextEdit)) or \
               (hasattr(self, 'sync_table') and self.sync_table.state() == QAbstractItemView.State.EditingState):
                event.ignore()
                return
            
            current_ms = self.player.media_player.position()
            new_ms = max(0, current_ms - 5000)  # Rewind 5 seconds
            self.player.media_player.setPosition(new_ms)
            event.accept()
            return
        
        elif event.key() == Qt.Key.Key_K:  # Pause/Play toggle
            focused_widget = self.focusWidget()
            # Skip if editing in table or text editor
            if isinstance(focused_widget, (QLineEdit, QTextEdit)) or \
               (hasattr(self, 'sync_table') and self.sync_table.state() == QAbstractItemView.State.EditingState):
                event.ignore()
                return
            
            self.player.toggle_playback()
            event.accept()
            return
        
        elif event.key() == Qt.Key.Key_L:  # Play forwards (fast forward)
            focused_widget = self.focusWidget()
            # Skip if editing in table or text editor
            if isinstance(focused_widget, (QLineEdit, QTextEdit)) or \
               (hasattr(self, 'sync_table') and self.sync_table.state() == QAbstractItemView.State.EditingState):
                event.ignore()
                return
            
            current_ms = self.player.media_player.position()
            duration_ms = self.player.media_player.duration()
            new_ms = min(duration_ms, current_ms + 5000)  # Forward 5 seconds
            self.player.media_player.setPosition(new_ms)
            event.accept()
            return
            
        elif event.key() == Qt.Key.Key_F1:
            self._show_shortcuts_help()
            event.accept()
            return
        
        # Sync mode specific shortcuts
        if not self.sync_mode_active:
            # Don't accept - let event propagate to system
            event.ignore()
            return
        
        # CRITICAL: Only handle spacebar if text editor does NOT have focus
        # This prevents spacebar from deleting text while typing
        if event.key() == Qt.Key.Key_Space:
            focused_widget = self.focusWidget()
            # Only trigger sync if focus is NOT on text editor
            if focused_widget != self.text_editor:
                # Tap to sync logic
                self._handle_tap_sync()
                event.accept()
                return
            else:
                # Let text editor handle the spacebar normally
                super().keyPressEvent(event)
                return
            
        # Nudge controls
        # Left/Right: Nudge Start Time
        # Shift + Left/Right: Nudge End Time
        elif event.key() == Qt.Key.Key_Left:
            amount = -0.1
            is_end = bool(event.modifiers() & Qt.KeyboardModifier.ShiftModifier)
            self._nudge_timestamp(amount, is_end)
            event.accept()
            
        elif event.key() == Qt.Key.Key_Right:
            amount = 0.1
            is_end = bool(event.modifiers() & Qt.KeyboardModifier.ShiftModifier)
            self._nudge_timestamp(amount, is_end)
            event.accept()
        
        else:
            super().keyPressEvent(event)

    def _on_table_row_clicked(self, item):
        """Jump to timestamp when user clicks on a lyrics line"""
        row = item.row()
        
        if row < 0 or row >= len(self.lyrics_data.lines):
            return
        
        line = self.lyrics_data.lines[row]
        # Jump to start time of this line
        if line.start_time >= 0:
            seek_ms = int(line.start_time * 1000)
            self.player.media_player.setPosition(seek_ms)
            logger.info(f"Jumped to line {row}: {line.start_time:.2f}s")
    
    def _on_table_item_changed(self, item):
        """Handle manual edits in the table"""
        row = item.row()
        col = item.column()
        
        if row < 0 or row >= len(self.lyrics_data.lines):
            return
        
        # Save undo state before modification
        self._save_undo_state()
            
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
        from PyQt6.QtGui import QCursor
        from PyQt6.QtWidgets import QMenu
        menu = QMenu()
        edit_action = menu.addAction("🔍 Edit Word Timings")
        delete_action = menu.addAction("🗑️ Delete Line")
        
        # Calculate action
        # Use QCursor.pos() for safer global positioning
        action = menu.exec(QCursor.pos())
        
        if action == edit_action:
            self._open_word_editor()
        elif action == delete_action:
            self._delete_selected_line()

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

    def _delete_selected_line(self):
        """Delete current line"""
        rows = self.sync_table.selectionModel().selectedRows()
        if not rows: return
        
        row_idx = rows[0].row()
        self._save_undo_state()
        
        # Remove from data
        self.lyrics_data.lines.pop(row_idx)
        
        # Update UI
        self._refresh_table()
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
        if hasattr(self, 'bg_video_player'):
            vid_pos = self.bg_video_player.position()
            # Tolerance: 80ms (approx 2-3 frames at 30fps)
            # If drift is too large, snap video to audio
            if abs(vid_pos - ms) > 80 and self.player.media_player.playbackState() == QMediaPlayer.PlaybackState.PlayingState:
                self.bg_video_player.setPosition(ms)
        
        current_seconds = ms / 1000.0
        
        # Update timeline playhead (Phase 6)
        if hasattr(self, 'timeline_widget'):
            self.timeline_widget.set_current_time(current_seconds)
        
        # Debugging
        # logger.info(f"Position: {current_seconds:.2f}s, Lines: {len(self.lyrics_data.lines)}")
        
        # Find active line to display
        found_line = None
        current_index = -1
        
        # Get timing mode
        timing_mode = self.combo_timing_mode.currentText() if hasattr(self, 'combo_timing_mode') else "Karaoke"
        # Get adjustable preview time (default 1.0 second)
        preview_seconds = self.spin_preview_time.value() if hasattr(self, 'spin_preview_time') else 1.0
        early_preview = preview_seconds if timing_mode == "Karaoke" else 0.0
        
        for i, line in enumerate(self.lyrics_data.lines):
             # Loop logic
             if self.chk_loop.isChecked() and self.active_line_index == i:
                 if current_seconds >= line.end_time and line.end_time > 0:
                     # Loop back to start
                     self.player.media_player.setPosition(int(line.start_time * 1000))
                     return
             
             # FIXED: Use adjusted time for START (show early), but current_seconds for END (don't cut off)
             # This makes lyrics appear early but stay visible until they actually finish
             adjusted_start = current_seconds + early_preview
             
             if line.start_time <= adjusted_start:
                # Use ACTUAL current_seconds for end check (not adjusted)
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
            
            # RENDERING OPTIMIZATION: Check if we're very close to the next line
            # Pre-load it to avoid visual lag at transition
            rendering_lookahead = 0.15  # 150ms lookahead for smoother transitions
            if next_line and (next_line.start_time - current_seconds) <= rendering_lookahead:
                # About to transition - pre-update to reduce lag
                self.preview_widget.set_line(next_line, None)
                self.preview_widget.set_current_time(current_seconds)
            else:
                # Normal display
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
        
        # If switching to Original Source, and it's a video, ensure bg_video_player uses it too?
        # Actually bg_video_player ALWAYS plays source_file (visuals), while audio player plays stems.
        # But if we switch audio to Source, we are playing same file twice?
        # Yes, but one for audio one for video.
        # Ideally if we play source, we could just use self.player for both?
        # No, keep separate for architecture consistency (Karaoke overlay needs separate view).
        
        # Use QTimer to restore position after media loads (more reliable than signal)
        from PyQt6.QtCore import QTimer
        def restore_state():
            self.player.media_player.setPosition(current_pos)
            self.bg_video_player.setPosition(current_pos)
            if was_playing:
                self.player.media_player.play()
                self.bg_video_player.play()
        
        # Wait 200ms for media to initialize before restoring
        QTimer.singleShot(200, restore_state)

    def _change_animation_type(self, anim_type: str):
        """Change the karaoke animation style"""
        self.preview_widget.set_animation_type(anim_type)

    def _pick_color(self):
        """Open color picker for custom animation color"""
        from PyQt6.QtWidgets import QColorDialog
        color = QColorDialog.getColor(self.preview_widget.active_color, self, "Pick Fill Color")
        if color.isValid():
            self.preview_widget.set_active_color(color)
            # Update button to show selected color
            self.btn_color.setStyleSheet(f"background-color: {color.name()};")

    def _import_lyrics_file(self):
        """Import lyrics from subtitle/lyrics file"""
        from utils.subtitle_parser import get_import_filter, import_subtitle, detect_subtitle_format
        
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Import Lyrics",
            "",
            get_import_filter()
        )
        
        if not file_path:
            return
        
        try:
            file_path = Path(file_path)
            format_type = detect_subtitle_format(file_path)
            
            if format_type == 'unknown' and file_path.suffix.lower() == '.txt':
                # Plain text - one line per text line, no timestamps
                with open(file_path, 'r', encoding='utf-8') as f:
                    lines = [line.strip() for line in f if line.strip()]
                
                self.lyrics_data.lines.clear()
                from sync.sync_data import LyricLine
                for text in lines:
                    line = LyricLine(text=text, start_time=0.0, end_time=0.0)
                    self.lyrics_data.lines.append(line)
                
                logger.info(f"Imported {len(lines)} lines from text file (no timestamps)")
            else:
                # Use unified subtitle parser
                self.lyrics_data = import_subtitle(file_path)
                logger.info(f"Imported {len(self.lyrics_data.lines)} lines from {format_type.upper()} file")
            
            # CRITICAL: Sync text editor with imported lyrics to prevent timestamp reset
            lyrics_text = "\n".join(line.text for line in self.lyrics_data.lines)
            self.text_editor.blockSignals(True)  # Prevent triggering _on_text_changed
            self.text_editor.setPlainText(lyrics_text)
            self.text_editor.blockSignals(False)
            
            # Update UI - populate sync table
            self._refresh_table()
            self._initialize_timeline()
            self.is_dirty = True
            
            # Show success message
            msg = f"✅ Imported {len(self.lyrics_data.lines)} lines"
            if format_type != 'unknown':
                msg += " with timestamps"
            
            QMessageBox.information(self, "Import Successful", msg)
            
        except Exception as e:
            logger.error(f"Failed to import lyrics: {e}")
            QMessageBox.critical(
                self,
                "Import Failed",
                f"Failed to import lyrics:\n{str(e)}"
            )
    
    def _change_playback_speed(self, speed_text: str):
        """Change playback speed (0.5x - 2.0x)"""
        # Extract float from "1.0x" format
        speed = float(speed_text.replace('x', ''))
        
        # Set playback rate for both players
        if hasattr(self, 'player') and self.player.media_player:
            self.player.media_player.setPlaybackRate(speed)
            logger.info(f"Playback speed changed to {speed}x")

    def _change_preview_mode(self, mode: str):
        """Change preview mode between 'With Video' and 'Lyrics Only'"""
        show_video = (mode == "With Video")
        
        if hasattr(self, 'video_item'):
            self.video_item.setVisible(show_video)
        
        if hasattr(self, 'preview_widget'):
            if show_video:
                # Video visible: lyrics at bottom
                self.lyrics_proxy.setPos(0, 800)
            else:
                # No video: lyrics centered
                self.lyrics_proxy.setPos(0, 440)  # Centered vertically (1080/2 - 100)
    
    def _change_timing_mode(self, mode: str):
        """Change timing mode between Karaoke and Lyrics Video"""
        if hasattr(self, 'preview_widget'):
            self.preview_widget.set_timing_mode(mode)
    
    def _change_romanize_mode(self, mode: str):
        """Change romanization display mode"""
        if mode in ["Romanized", "Both"]:
            # Need romanized text - generate if not exists
            if not any(line.romanized_text for line in self.lyrics_data.lines):
                self._romanize_all_lyrics()
        
        # Update preview widget with mode
        if hasattr(self.preview_widget, 'set_romanization_mode'):
            self.preview_widget.set_romanization_mode(mode)
        
        # Refresh display
        self._refresh_table()
        
        # Update current preview
        if self.active_line_index >= 0 and self.active_line_index < len(self.lyrics_data.lines):
            line = self.lyrics_data.lines[self.active_line_index]
            next_line = self.lyrics_data.lines[self.active_line_index + 1] if self.active_line_index + 1 < len(self.lyrics_data.lines) else None
            self.preview_widget.set_line(line, next_line)

    
    def _romanize_all_lyrics(self):
        """Apply romanization to all lyrics lines"""
        from utils.romanizer import get_romanizer
        romanizer = get_romanizer()
        
        # Get selected language
        lang_text = self.combo_language.currentText().lower()
        
        for line in self.lyrics_data.lines:
            # Romanize line text
            line.romanized_text = romanizer.romanize(line.text, lang_text)
            
            # Romanize tokens if they exist
            for token in line.tokens:
                token.romanized_text = romanizer.romanize(token.text, lang_text)
        
        self.is_dirty = True
        self.status_bar.setText("Lyrics romanized successfully")
    
    def _clear_romanization(self):
        """Clear romanization from all lyrics"""
        for line in self.lyrics_data.lines:
            line.romanized_text = None
            for token in line.tokens:
                token.romanized_text = None
        
        self.is_dirty = True

    
    
    def _apply_timing_offset(self, offset_seconds: float):
        """Apply global timing offset to all lyrics to compensate for AI transcription delay"""
        if not hasattr(self, '_original_lyrics_times'):
            # Store original times on first offset change
            self._original_lyrics_times = []
            for line in self.lyrics_data.lines:
                self._original_lyrics_times.append((line.start_time, line.end_time))
        
        # Apply offset to all lines
        for i, line in enumerate(self.lyrics_data.lines):
            if i < len(self._original_lyrics_times):
                orig_start, orig_end = self._original_lyrics_times[i]
                line.start_time = max(0.0, orig_start + offset_seconds)
                line.end_time = max(0.0, orig_end + offset_seconds)
                
                # Also adjust tokens if they exist
                if line.tokens:
                    for token in line.tokens:
                        # Shift token times proportionally
                        token.start_time = max(0.0, token.start_time + offset_seconds)
                        token.end_time = max(0.0, token.end_time + offset_seconds)
        
        # Refresh table display
        self._refresh_table()
        self.is_dirty = True
        
        # Show feedback
        if offset_seconds != 0:
            direction = "earlier" if offset_seconds < 0 else "later"
            self.status_bar.setText(f"⏱️ All lyrics shifted {abs(offset_seconds):.1f}s {direction}")
        else:
            self.status_bar.setText("⏱️ Timing offset reset")

    
    def _show_shortcuts_help(self):
        """Show keyboard shortcuts help dialog"""
        from gui.dialogs.shortcuts_dialog import ShortcutsDialog
        dialog = ShortcutsDialog(self)
        dialog.exec()

    def _save_undo_state(self):
        """Save current state to undo stack before making changes"""
        self.undo_manager.push_state(self.lyrics_data)
        self.is_dirty = True  # Mark as modified
        
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
        # Audio Source Logic: Use original source for best transcription
        # Falls back to vocals if source not available
        audio_source = self.project.source_file
        source_type = "Original Source Audio"
        
        if not audio_source or not audio_source.exists():
            logger.warning("Source file not found, using vocals track.")
            audio_source = self.project.vocals_file
            source_type = "Vocals Track"
            
        if not audio_source or not audio_source.exists():
            QMessageBox.warning(self, "No Audio", "No suitable audio file found for transcription.\nNeed Source or Vocals file.")
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
        
        # Model selection - auto-detect available models
        from pathlib import Path
        whisper_dir = Path("models/whisper")
        available_models = []
        
        if whisper_dir.exists():
            for model_file in whisper_dir.glob("*.pt"):
                available_models.append(model_file.stem)
        
        # If no models found, use default from config (will download)
        if not available_models:
            model_name = self.config.get('lyrics.whisper_model', 'small')
        # If only one model, use it automatically
        elif len(available_models) == 1:
            model_name = available_models[0]
        # If multiple models, show selection
        else:
            model_descriptions = {
                "tiny": "Fastest (low quality)",
                "base": "Fast (decent quality)",
                "small": "Balanced (recommended)",
                "medium": "High quality (slower)",
                "large": "Best quality (very slow)",
                "large-v2": "Best quality v2 (very slow)",
                "large-v3": "Best quality v3 (very slow)"
            }
            
            # Merge with detected models
            detected = self.config.get_available_models('whisper')
            
            # Map detected files to simple names if possible
            detected_map = {}
            from pathlib import Path
            
            for d in detected:
                path_obj = Path(d)
                simple = path_obj.name
                
                # 1. Standard Folder (e.g. "medium")
                # This takes precedence for the simple name
                if simple in model_descriptions:
                    detected_map[simple] = d
                    continue
                
                # 2. OpenAI .pt files - Make explicit to avoid collision
                if d.endswith('.pt'):
                    base_name = path_obj.stem
                    # e.g. "large-v3" -> "large-v3 (OpenAI)"
                    key = f"{base_name} (OpenAI)"
                    detected_map[key] = d
                    
                # 3. Handle specific faster-whisper cache folders
                elif "faster-whisper" in str(d):
                     name = path_obj.name
                     # Try to simplify name: "models--Systran--faster-whisper-large-v3" -> "large-v3-faster"
                     clean_name = name.replace("models--Systran--faster-whisper-", "").replace("faster-whisper-", "")
                     key = f"{clean_name} (Cached)"
                     detected_map[key] = d
                     
                else:
                    # Fallback
                    detected_map[simple] = d
            
            # Build polished list
            choices_standard = []
            choices_openai = []
            choices_other = []
            
            processed_keys = set()
            
            for key in detected_map.keys():
                if key in model_descriptions:
                     desc = model_descriptions[key] + " [Installed]"
                     choices_standard.append(f"{key} - {desc}")
                elif "(OpenAI)" in key:
                     base = key.replace(" (OpenAI)", "")
                     desc = model_descriptions.get(base, "OpenAI Original Model")
                     choices_openai.append(f"{key} - {desc}")
                else:
                     choices_other.append(f"{key} - Local Custom Model")
            
            choices_standard.sort()
            choices_openai.sort()
            choices_other.sort()
            
            # Final list: Standard first, then OpenAI, then others
            model_choices = choices_standard + choices_openai + choices_other
            
            model_choice, ok = QInputDialog.getItem(
                self, "Select Whisper Model",
                "Choose AI transcription model:",
                model_choices, 0, False
            )
            
            if not ok:
                return
            
            # Extract model name
            model_name_display = model_choice.split(' - ')[0].strip()
            
            # Resolve to full path/ID for worker
            # If the user selected a "standard" name like "large-v3", and we have a custom path for it NOT mapped in detected_map?
            # Actually, standard names are keys in detected_map if they match .pt files.
            # But what if "large-v3" maps to the Folder Path we found?
            # It won't match "large-v3" exactly because our custom logic added " (Local Faster)".
            
            # If the user selected "large-v3 (Local Faster)", we need to map that back to the full path string.
            if model_name_display in detected_map:
                model_name = detected_map[model_name_display]
            else:
                model_name = model_name_display
            
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
        
        # Worker - pass model_name
        self.transcriber = TranscriptionWorker(audio_source, model_name=model_name, language=selected_lang_code)
        
        # Custom progress update to avoid "ms" confusion
        def update_label(msg):
             # Check if progress dialog is valid and not deleted
             if not self.progress or self.progress.isHidden():
                 return
                 
             try:
                 if self.progress.wasCanceled():
                     return

                 if "Transcribing" in msg and selected_lang_name != "Auto Detect":
                     self.progress.setLabelText(f"Transcribing ({selected_lang_name})...")
                 else:
                     self.progress.setLabelText(msg)
             except RuntimeError:
                 # Dialog might be deleted if closed violently
                 pass
                 
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
        from sync.sync_data import LyricLine, LyricWord
        for seg in segments:
            line = LyricLine(
                text=seg['text'],
                start_time=seg['start'],
                end_time=seg['end']
            )
            # Add word-level timing if available
            for word_data in seg.get('words', []):
                line.words.append(LyricWord(
                    word=word_data.get('word', word_data.get('text', '')),
                    start_time=word_data.get('start', 0.0),
                    end_time=word_data.get('end', 0.0),
                    confidence=word_data.get('confidence', 1.0)
                ))
            self.lyrics_data.lines.append(line)
        
        self.is_dirty = True
            
        self._refresh_table()
        
        # 3. Update text editor for consistency (won't trigger parse)
        text_content = "\n".join(l.text for l in self.lyrics_data.lines)
        self.text_editor.setPlainText(text_content)
        
        QMessageBox.information(self, "Success", f"Transcribed {len(segments)} lines!")

    def _import_from_project(self):
        """Import components from another .nctv project"""
        from gui.dialogs.import_dialog import ImportDialog
        
        dialog = ImportDialog(self.project, self)
        if dialog.exec():
            # Import successful - refresh UI
            self.lyrics_data = self.project.lyrics
            
            # Update text editor
            text = "\n".join(line.text for line in self.lyrics_data.lines)
            self.text_editor.blockSignals(True)
            self.text_editor.setPlainText(text)
            self.text_editor.blockSignals(False)
            
            # Refresh sync table
            self._refresh_table()
            
            # Mark as modified
            self.is_dirty = True
            
            logger.info("Project components imported successfully")
    
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
            self.is_dirty = False  # Clear dirty flag after successful auto-save
            logger.info(f"Auto-saved project to: {default_path}")
        except Exception as e:
            logger.warning(f"Auto-save failed: {e}")

    def _open_timing_calibration(self):
        """Open timing calibration dialog"""
        from gui.components.timing_calibration import TimingCalibrationDialog
        
        dialog = TimingCalibrationDialog(self.project, self)
        if dialog.exec():
            # Apply settings
            dialog.apply_settings()
            self.is_dirty = True
            logger.info("Timing calibration settings applied")
            
            # Show confirmation
            QMessageBox.information(
                self,
                "Timing Updated",
                f"Global offset set to: {dialog.get_global_offset():.3f}s\n\n"
                "Play the track to verify timing is correct."
            )
    
    def _export_subtitles(self):
        """Export lyrics to subtitle file"""
        from utils.subtitle_parser import SUPPORTED_FORMATS
        
        # Build filter string manually or add get_export_filter to parser
        # For now, we reuse the import filter as it covers supported formats
        # Or build a specific one for export
        
        filters = []
        for fmt, info in SUPPORTED_FORMATS.items():
            if info['export']:
                exts = ' '.join(f'*{ext}' for ext in info['extensions'])
                filters.append(f"{info['name']} ({exts})")
        
        filter_str = ';;'.join(filters)
        
        default_name = f"{self.project.project_name}.srt"
        
        file_path, selected_filter = QFileDialog.getSaveFileName(
            self,
            "Export Subtitles",
            default_name,
            filter_str
        )
        
        if not file_path:
            return
            
        try:
            from utils.subtitle_parser import export_subtitle
            
            output_path = Path(file_path)
            export_subtitle(self.lyrics_data, output_path)
            
            QMessageBox.information(
                self, 
                "Export Successful", 
                f"Lyrics exported to:\n{output_path.name}"
            )
            
        except Exception as e:
            logger.error(f"Subtitle export failed: {e}")
            QMessageBox.critical(
                self,
                "Export Failed",
                f"Failed to export subtitles:\n{str(e)}"
            )

    def _export_video(self):
        """Export project to video with karaoke subtitles"""
        from utils.ass_generator import ASSGenerator
        from gui.dialogs.export_dialog import ExportDialog
        from workers.export_worker import ExportWorker
        
        # 1. Open Export Dialog with current animation from preview
        default_name = f"{self.project.project_name}_karaoke.mp4"
        current_animation = self.preview_widget.animation_type
        
        dialog = ExportDialog(self, default_name, current_animation, project=self.project)
        if not dialog.exec():
            return
            
        options = dialog.get_options()
        out_path = options['path']
        style = options['style']
        animation = options['animation']
        
        # 2. Check source video
        video_input = self.project.source_file
        is_video = video_input.suffix.lower() in ['.mp4', '.avi', '.mkv', '.mov']
        
        background_options = None
        if not is_video:
            # Show video options dialog
            from gui.dialogs.video_options_dialog import VideoOptionsDialog
            bg_dialog = VideoOptionsDialog(self)
            if not bg_dialog.exec():
                return  # User cancelled
            
            background_options = bg_dialog.get_options()
            if not background_options:
                QMessageBox.warning(self, "Invalid Selection", "Please select a valid background option.")
                return

        # 1.5 Setup Intro Credits
        from gui.dialogs.export_credits_dialog import ExportCreditsDialog
        credits_dialog = ExportCreditsDialog(
            self, 
            default_title=self.project.project_name, 
            default_artist="Unknown Artist"
        )
        
        credits_enabled = False
        credits_info = {}
        
        if credits_dialog.exec():
            credits_info = credits_dialog.get_info()
            credits_enabled = credits_info['enabled']
        else:
            return # Cancelled

        # 3. Generate Subtitles
        custom_style = {}
        if style == "Match Preview":
            custom_style = {
                'active_color': self.preview_widget.active_color,
                'inactive_color': self.preview_widget.inactive_color,
                'outline_color': self.preview_widget.outline_color
            }
            
        offset = self.project.timing_metadata.get('global_offset', 0.0)
        ass_gen = ASSGenerator(self.lyrics_data, style=style, animation=animation, custom_style=custom_style, global_offset=offset)
        ass_content = ass_gen.generate()
        
        temp_ass = self.project.get_temp_dir() / "subs.ass"
        with open(temp_ass, "w", encoding="utf-8") as f:
            f.write(ass_content)
        
        # 4. Build FFmpeg Command
        inst_path = self.project.instrumental_file
        voc_path = self.project.vocals_file
        has_stems = inst_path and inst_path.exists() and voc_path and voc_path.exists()
        
        ass_path_unix = str(temp_ass).replace("\\", "/").replace(":", "\\:").replace("'", r"\'")
        
        # Generate Overlay Image if needed
        overlay_path_unix = ""
        credits_duration = 5.0
        if credits_enabled:
            from utils.image_generator import ImageGenerator
            credits_img_path = self.project.get_temp_dir() / "credits_overlay.png"
            ImageGenerator.generate_credits_overlay(
                credits_info['title'],
                credits_info['artist'],
                credits_info['show_version'],
                str(credits_img_path)
            )
            overlay_path_unix = str(credits_img_path).replace("\\", "/").replace(":", "\\:").replace("'", r"\'")
            credits_duration = credits_info['duration']
        
        # Check for Intro Mode (Global)
        intro_mode_val = credits_info.get('intro_mode', False) if credits_enabled else False
        is_preroll = credits_enabled and intro_mode_val
        
        cmd = ["ffmpeg", "-y"]
        
        # Handle different background types
        audio_source_opt = options.get('audio_source', 'mixed')
        
        if is_video:
            # Original video background
            if has_stems and audio_source_opt == 'mixed':
                cmd.extend(["-i", str(video_input)])
                cmd.extend(["-i", str(inst_path)])
                cmd.extend(["-i", str(voc_path)])
                
                # Check overlay
                next_input = 3
                if credits_enabled:
                    cmd.extend(["-i", str(credits_img_path)])
                
                # Use async=1 to fix audio timestamp drift
                # Use duration=longest to prevent early cuts
                
                fc = []
                
                fc = []
                
                if is_preroll:
                    # INTRO MODE: Concat [Intro] + [Main]
                    # 1. Main Video with Burned Subtitles (scaled to 1080p)
                    fc.append(f"[0:v]scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,ass='{ass_path_unix}'[v_main_raw]")
                    
                    # 2. Main Audio Mix
                    fc.append(f"[1:a][2:a]amix=inputs=2:duration=longest:dropout_transition=0,aresample=async=1[a_main]")
                    
                    # 3. Intro Video (Static Image Loop)
                    # Loop image, scale, set SAR to 1:1, trim to duration
                    fc.append(f"[{next_input}:v]loop=loop=-1:size=1:start=0,scale=1920:1080,setsar=1,trim=duration={credits_duration}[v_intro]")
                    
                    # 4. Intro Audio (Silence)
                    fc.append(f"anullsrc=r=48000:cl=stereo,atrim=duration={credits_duration}[a_intro]")
                    
                    # 5. Concatenate
                    # [v_intro][a_intro][v_main_raw][a_main]
                    fc.append(f"[v_intro][a_intro][v_main_raw][a_main]concat=n=2:v=1:a=1[v_out][a_out]")
                    
                    cmd.extend(["-filter_complex", ";".join(fc)])
                    cmd.extend(["-map", "[v_out]", "-map", "[a_out]"])
                    
                else:
                    # NORMAL / OVERLAY MODE
                    fc.append(f"[0:v]ass='{ass_path_unix}'[v0]")
                    
                    last_v = "[v0]"
                    if credits_enabled:
                         # Overlay on top of video
                        fc.append(f"{last_v}[{next_input}:v]overlay=0:0:enable='between(t,0,{credits_duration})'[v1]")
                        last_v = "[v1]"
                        
                    fc.append(f"[1:a][2:a]amix=inputs=2:duration=longest:dropout_transition=0,aresample=async=1[a]")
                    
                    cmd.extend(["-filter_complex", ";".join(fc)])
                    cmd.extend(["-map", last_v, "-map", "[a]"])
                    
                cmd.extend(["-c:a", "aac", "-b:a", "192k"])
            
            elif has_stems and audio_source_opt == 'instrumental':
                 cmd.extend(["-i", str(video_input)])
                 cmd.extend(["-i", str(inst_path)])
                 
                 next_input = 2
                 if credits_enabled:
                    cmd.extend(["-i", str(credits_img_path)])

                 fc = []
                 
                 if is_preroll:
                    # INTRO MODE: Concat
                    # 1. Main Video scaled
                    fc.append(f"[0:v]scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,ass='{ass_path_unix}'[v_main_raw]")
                    # 2. Audio (Instrumental only)
                    fc.append(f"[1:a]aresample=async=1[a_main]")
                    # 3. Intro Video
                    fc.append(f"[{next_input}:v]loop=loop=-1:size=1:start=0,scale=1920:1080,setsar=1,trim=duration={credits_duration}[v_intro]")
                    # 4. Intro Audio (Silence)
                    fc.append(f"anullsrc=r=48000:cl=stereo,atrim=duration={credits_duration}[a_intro]")
                    # 5. Concat
                    fc.append(f"[v_intro][a_intro][v_main_raw][a_main]concat=n=2:v=1:a=1[v_out][a_out]")
                    
                    cmd.extend(["-filter_complex", ";".join(fc)])
                    cmd.extend(["-map", "[v_out]", "-map", "[a_out]"])
                 else:
                     fc.append(f"[0:v]ass='{ass_path_unix}'[v0]")
                     
                     last_v = "[v0]"
                     if credits_enabled:
                        fc.append(f"{last_v}[{next_input}:v]overlay=0:0:enable='between(t,0,{credits_duration})'[v1]")
                        last_v = "[v1]"
                     
                     cmd.extend(["-filter_complex", ";".join(fc)])
                     cmd.extend(["-map", last_v, "-map", "1:a"])
                     
                 cmd.extend(["-c:a", "aac", "-b:a", "192k"])
                 
            else:
                # Original Audio or fallback
                cmd.extend(["-i", str(video_input)])
                
                next_input = 1
                if credits_enabled:
                    cmd.extend(["-i", str(credits_img_path)])
                    
                fc = []
                
                if is_preroll and credits_enabled:
                     # INTRO MODE
                     fc.append(f"[0:v]scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,ass='{ass_path_unix}'[v_main_raw]")
                     fc.append(f"[0:a]aresample=async=1[a_main]")
                     fc.append(f"[{next_input}:v]loop=loop=-1:size=1:start=0,scale=1920:1080,setsar=1,trim=duration={credits_duration}[v_intro]")
                     fc.append(f"anullsrc=r=48000:cl=stereo,atrim=duration={credits_duration}[a_intro]")
                     fc.append(f"[v_intro][a_intro][v_main_raw][a_main]concat=n=2:v=1:a=1[v_out][a_out]")
                     
                     cmd.extend(["-filter_complex", ";".join(fc)])
                     cmd.extend(["-map", "[v_out]", "-map", "[a_out]"])
                     cmd.extend(["-c:a", "aac", "-b:a", "192k"]) # Re-encode for concat safety
                elif credits_enabled:
                    # Overlay Mode
                    fc.append(f"[0:v]ass='{ass_path_unix}'[v0]")
                    fc.append(f"[v0][{next_input}:v]overlay=0:0:enable='between(t,0,{credits_duration})'[v1]")
                    cmd.extend(["-filter_complex", ";".join(fc)])
                    cmd.extend(["-map", "[v1]", "-map", "0:a"])
                    cmd.extend(["-c:a", "copy"])
                else:
                    # No Credits
                    cmd.extend(["-vf", f"ass='{ass_path_unix}'"])
                    cmd.extend(["-c:a", "copy"])
        
        else:
            # Audio-only source, use selected background
            bg_type = background_options['type']
            
            # Get audio duration for generating video
            import subprocess
            duration_result = subprocess.run(
                ["ffprobe", "-v", "error", "-show_entries", "format=duration", 
                 "-of", "default=noprint_wrappers=1:nokey=1", str(video_input)],
                capture_output=True, text=True
            )
            duration = float(duration_result.stdout.strip()) if duration_result.stdout.strip() else 60
            
            if bg_type == 'video':
                # Use selected video as background
                bg_video = background_options['path']
                if has_stems:
                    cmd.extend(["-stream_loop", "-1", "-i", str(bg_video)])  # Loop video
                    cmd.extend(["-i", str(inst_path)])
                    cmd.extend(["-i", str(voc_path)])
                    
                    next_input = 3
                    if credits_enabled:
                         cmd.extend(["-i", str(credits_img_path)])
                    
                    fc = []
                    fc.append(f"[0:v]ass='{ass_path_unix}'[v0]")
                    
                    last_v = "[v0]"
                    if credits_enabled:
                        fc.append(f"{last_v}[{next_input}:v]overlay=0:0:enable='between(t,0,{credits_duration})'[v1]")
                        last_v = "[v1]"
                    
                    fc.append(f"[1:a][2:a]amix=inputs=2:duration=longest:dropout_transition=0,aresample=async=1[a]")
                    
                    if is_preroll and credits_enabled:
                        # [v0] is main video from ass, [a] is mixed audio
                        # We need to tag them to be clearer
                        # Actually [v0] is what we want to be the 'main' part
                        
                        # Intro part
                        fc.append(f"[{next_input}:v]loop=loop=-1:size=1:start=0,scale=1920:1080,setsar=1,trim=duration={credits_duration}[v_intro]")
                        fc.append(f"anullsrc=r=48000:cl=stereo,atrim=duration={credits_duration}[a_intro]")
                        
                        # Main part reuse [v0] and [a]
                        # Assume [v0] is already formatted correctly? It's from bg_video + ass.
                        # We might need to scale [v0] just in case
                        fc.append(f"[v0]scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2[v_main]")
                        
                        fc.append(f"[v_intro][a_intro][v_main][a]concat=n=2:v=1:a=1[v_out][a_out]")
                        
                        cmd.extend(["-filter_complex", ";".join(fc)])
                        cmd.extend(["-map", "[v_out]", "-map", "[a_out]"])
                    else:
                        # Normal Overlay
                        last_v = "[v0]"
                        if credits_enabled:
                            fc.append(f"{last_v}[{next_input}:v]overlay=0:0:enable='between(t,0,{credits_duration})'[v1]")
                            last_v = "[v1]"
                            
                        cmd.extend(["-filter_complex", ";".join(fc)])
                        cmd.extend(["-map", last_v, "-map", "[a]"])
                        
                    cmd.extend(["-shortest"])  # Cut when audio ends
                    cmd.extend(["-c:a", "aac", "-b:a", "192k"])
                else:
                    cmd.extend(["-stream_loop", "-1", "-i", str(bg_video)])
                    cmd.extend(["-i", str(video_input)])
                    
                    next_input = 2
                    if credits_enabled:
                         cmd.extend(["-i", str(credits_img_path)])
                    
                    fc = []
                    fc.append(f"[0:v]ass='{ass_path_unix}'[v0]")
                    
                    last_v = "[v0]"
                    if credits_enabled:
                        fc.append(f"{last_v}[{next_input}:v]overlay=0:0:enable='between(t,0,{credits_duration})'[v1]")
                        last_v = "[v1]"
                        
                    cmd.extend(["-filter_complex", ";".join(fc)])
                    cmd.extend(["-map", last_v, "-map", "1:a"])
                    cmd.extend(["-shortest"])
                    cmd.extend(["-c:a", "copy"])
            
            elif bg_type == 'image':
                # Use static image as background
                bg_image = background_options['path']
                if has_stems:
                    cmd.extend(["-loop", "1", "-i", str(bg_image)])
                    cmd.extend(["-i", str(inst_path)])
                    cmd.extend(["-i", str(voc_path)])
                    
                    next_input = 3
                    if credits_enabled:
                         cmd.extend(["-i", str(credits_img_path)])
                    
                    fc = []
                    fc.append(f"[0:v]scale=1920:1080,ass='{ass_path_unix}'[v0]")
                    
                    last_v = "[v0]"
                    if credits_enabled:
                        fc.append(f"{last_v}[{next_input}:v]overlay=0:0:enable='between(t,0,{credits_duration})'[v1]")
                        last_v = "[v1]"
                        
                    fc.append(f"[1:a][2:a]amix=inputs=2:duration=longest:dropout_transition=0,aresample=async=1[a]")
                    
                    cmd.extend(["-filter_complex", ";".join(fc)])
                    cmd.extend(["-map", last_v, "-map", "[a]"])
                    cmd.extend(["-t", str(duration)])
                    cmd.extend(["-c:a", "aac", "-b:a", "192k"])
                else:
                    cmd.extend(["-loop", "1", "-i", str(bg_image)])
                    cmd.extend(["-i", str(video_input)])
                    
                    next_input = 2
                    if credits_enabled:
                         cmd.extend(["-i", str(credits_img_path)])
                    
                    fc = []
                    fc.append(f"[0:v]scale=1920:1080,ass='{ass_path_unix}'[v0]")
                    
                    last_v = "[v0]"
                    if credits_enabled:
                        fc.append(f"{last_v}[{next_input}:v]overlay=0:0:enable='between(t,0,{credits_duration})'[v1]")
                        last_v = "[v1]"
                        
                    cmd.extend(["-filter_complex", ";".join(fc)])
                    cmd.extend(["-map", last_v, "-map", "1:a"])
                    cmd.extend(["-t", str(duration)])
                    cmd.extend(["-c:a", "copy"])
            
            elif bg_type == 'color':
                # Generate solid color background
                color = background_options['color']
                color_hex = color.name().replace('#', '0x')
                
                if has_stems:
                    cmd.extend(["-f", "lavfi", "-i", f"color=c={color_hex}:s=1920x1080:r=30"])
                    cmd.extend(["-i", str(inst_path)])
                    cmd.extend(["-i", str(voc_path)])
                    
                    next_input = 3
                    if credits_enabled:
                         cmd.extend(["-i", str(credits_img_path)])
                    
                    fc = []
                    fc.append(f"[0:v]ass='{ass_path_unix}'[v0]")
                    
                    last_v = "[v0]"
                    if credits_enabled:
                        fc.append(f"{last_v}[{next_input}:v]overlay=0:0:enable='between(t,0,{credits_duration})'[v1]")
                        last_v = "[v1]"
                        
                    fc.append(f"[1:a][2:a]amix=inputs=2:duration=longest:dropout_transition=0,aresample=async=1[a]")
                    
                    cmd.extend(["-filter_complex", ";".join(fc)])
                    cmd.extend(["-map", last_v, "-map", "[a]"])
                    cmd.extend(["-t", str(duration)])
                    cmd.extend(["-c:a", "aac", "-b:a", "192k"])
                else:
                    cmd.extend(["-f", "lavfi", "-i", f"color=c={color_hex}:s=1920x1080:r=30"])
                    cmd.extend(["-i", str(video_input)])
                    
                    next_input = 2
                    if credits_enabled:
                         cmd.extend(["-i", str(credits_img_path)])
                    
                    fc = []
                    fc.append(f"[0:v]ass='{ass_path_unix}'[v0]")
                    
                    last_v = "[v0]"
                    if credits_enabled:
                        fc.append(f"{last_v}[{next_input}:v]overlay=0:0:enable='between(t,0,{credits_duration})'[v1]")
                        last_v = "[v1]"
                    
                    cmd.extend(["-filter_complex", ";".join(fc)])
                    cmd.extend(["-map", last_v, "-map", "1:a"])
                    cmd.extend(["-t", str(duration)])
                    cmd.extend(["-c:a", "copy"])
        
        # Video encoding settings for compatibility
        cmd.extend(["-c:v", "libx264", "-pix_fmt", "yuv420p", "-preset", "medium"])
        cmd.append(str(out_path))
        
        logger.info(f"Running export command: {cmd}")
        
        # 5. Show Progress Dialog
        self.export_progress = QProgressDialog("Starting Export...", "Cancel", 0, 0, self)
        self.export_progress.setWindowTitle("Exporting Video")
        self.export_progress.setMinimumDuration(0)
        self.export_progress.show()
        
        # 6. Run in Background Thread
        self.export_worker = ExportWorker(cmd, out_path)
        self.export_worker.progress.connect(lambda msg: self.export_progress.setLabelText(msg))
        self.export_worker.finished.connect(self._on_export_finished)
        self.export_worker.start()
        
    def _on_export_finished(self, success, message):
        """Handle export completion"""
        self.export_progress.close()
        
        if success:
            QMessageBox.information(self, "Success", f"Exported to:\n{message}")
            # Auto-play result
            import os
            os.startfile(message)
        else:
            QMessageBox.critical(self, "Export Failed", f"Error:\n{message}")


    # === Phase 6: Timeline Methods ===
    
    def _toggle_timeline_view(self):
        """Toggle timeline widget visibility"""
        if self.btn_timeline.isChecked():
            # Reinitialize timeline to ensure lyrics are populated
            self._initialize_timeline()
            self.timeline_widget.show()
            logger.info('Timeline view enabled')
        else:
            self.timeline_widget.hide()
            logger.info('Timeline view disabled')
    
    def _on_timeline_seek(self, seconds: float):
        """Handle seek from timeline widget"""
        seek_ms = int(seconds * 1000)
        self.player.media_player.setPosition(seek_ms)
        logger.info(f'Timeline seek to {seconds:.2f}s')
    
    def _on_clip_selected(self, clip_id: str):
        """Handle clip selection in timeline"""
        logger.info(f'Clip selected: {clip_id}')
        # TODO: Highlight corresponding lyrics line in sync table if it's a lyrics clip
        for track in self.project.timeline.tracks:
            clip = track.get_clip(clip_id)
            if clip and 'line_index' in clip.properties:
                line_idx = clip.properties['line_index']
                if 0 <= line_idx < self.sync_table.rowCount():
                    self.sync_table.selectRow(line_idx)
                break
    
    def _on_clip_moved(self, clip_id: str, new_start_time: float):
        """Handle clip being moved to new position"""
        logger.info(f'Clip {clip_id} moved to {new_start_time:.2f}s')
        
        # Update project timeline
        for track in self.project.timeline.tracks:
            clip = track.get_clip(clip_id)
            if clip:
                clip.move_to(new_start_time)
                
                # If it's a lyrics clip, update the lyrics data
                if 'line_index' in clip.properties:
                    line_idx = clip.properties['line_index']
                    if 0 <= line_idx < len(self.lyrics_data.lines):
                        duration = clip.duration
                        self.lyrics_data.lines[line_idx].start_time = new_start_time
                        self.lyrics_data.lines[line_idx].end_time = new_start_time + duration
                        self._refresh_table()
                
                self.is_dirty = True
                self.timeline_widget.timeline_canvas.update()
                break
    
    def _on_clip_resized(self, clip_id: str, new_duration: float):
        """Handle clip being resized"""
        logger.info(f'Clip {clip_id} resized to {new_duration:.2f}s duration')
        
        # Update project timeline
        for track in self.project.timeline.tracks:
            clip = track.get_clip(clip_id)
            if clip:
                # If it's a lyrics clip, update the lyrics data
                if 'line_index' in clip.properties:
                    line_idx = clip.properties['line_index']
                    if 0 <= line_idx < len(self.lyrics_data.lines):
                        self.lyrics_data.lines[line_idx].end_time = clip.start_time + new_duration
                        self._refresh_table()
                
                self.is_dirty = True
                self.timeline_widget.timeline_canvas.update()
                break
    
    def _on_clip_split(self, clip_id: str, split_time: float):
        """Handle clip being split at specified time"""
        logger.info(f'Splitting clip {clip_id} at {split_time:.2f}s')
        
        # Find and split the clip
        for track in self.project.timeline.tracks:
            clip = track.get_clip(clip_id)
            if clip:
                # Split the clip
                right_clip = clip.split_at(split_time)
                
                if right_clip:
                    track.add_clip(right_clip)
                    
                    # If it's a lyrics clip, we should split the lyrics line too
                    if 'line_index' in clip.properties:
                        line_idx = clip.properties['line_index']
                        if 0 <= line_idx < len(self.lyrics_data.lines):
                            QMessageBox.information(
                                self,
                                "Lyrics Split",
                                "Clip split successfully! Note: You may want to manually edit the lyrics text for each half."
                            )
                    
                    self.is_dirty = True
                    self.timeline_widget.timeline_canvas.update()
                break
    
    def _on_clip_deleted(self, clip_id: str):
        """Handle clip deletion"""
        logger.info(f'Deleting clip {clip_id}')
        
        # Confirm deletion
        reply = QMessageBox.question(
            self,
            "Delete Clip",
            "Are you sure you want to delete this clip?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            for track in self.project.timeline.tracks:
                if track.remove_clip(clip_id):
                    self.timeline_widget.selected_clip_id = None
                    self.timeline_widget.timeline_canvas.update()
                    self.is_dirty = True
                    logger.info(f'Clip {clip_id} deleted from track {track.track_id}')
                    break
    
    def _on_effect_requested(self, clip_id: str):
        """Handle request to add effect to clip"""
        logger.info(f'Effect requested for clip {clip_id}')
        
        # Find the clip
        selected_clip = None
        for track in self.project.timeline.tracks:
            clip = track.get_clip(clip_id)
            if clip:
                selected_clip = clip
                break
        
        if not selected_clip:
            return
        
        # Open effect panel dialog
        from gui.components.effect_panel import EffectPanelDialog
        
        dialog = EffectPanelDialog(selected_clip, self)
        if dialog.exec():
            # Effects are already modified in place
            self.is_dirty = True
            self.timeline_widget.timeline_canvas.update()
            logger.info(f'Effects updated for clip {clip_id}')
    
    def _initialize_timeline(self):
        """Initialize timeline with default tracks and populate with lyrics clips (Phase 6.1 Polish)"""
        from core.timeline_data import Track, Clip, TrackType
        import uuid
        
        timeline = self.project.timeline
        
        # Clear existing tracks (in case of re-initialization)
        timeline.tracks.clear()
        
        # Create default tracks
        # 1. Audio Track (Instrumental)
        audio_track = Track(
            track_id="audio_instrumental",
            track_type=TrackType.AUDIO,
            name="Instrumental"
        )
        timeline.add_track(audio_track)
        
        # 2. Audio Track (Vocals) if available
        if self.project.vocals_file:
            vocals_track = Track(
                track_id="audio_vocals",
                track_type=TrackType.AUDIO,
                name="Vocals"
            )
            timeline.add_track(vocals_track)
        
        # 3. Video Track if source is video
        if self.project.source_file:
            src_path = Path(self.project.source_file)
            if src_path.suffix.lower() in ['.mp4', '.avi', '.mkv', '.mov']:
                video_track = Track(
                    track_id="video_background",
                    track_type=TrackType.VIDEO,
                    name="Background Video"
                )
                timeline.add_track(video_track)
        
        # 4. Lyrics Track - Populate with lyrics lines as clips
        lyrics_track = Track(
            track_id="lyrics_main",
            track_type=TrackType.LYRICS,
            name="Lyrics"
        )
        
        # Add lyrics as clips
        for i, line in enumerate(self.lyrics_data.lines):
            if line.start_time >= 0 and line.end_time > line.start_time:
                clip = Clip(
                    clip_id=f"lyric_{i}_{uuid.uuid4().hex[:8]}",
                    track_id="lyrics_main",
                    start_time=line.start_time,
                    duration=line.duration,
                    properties={
                        'text': line.text,
                        'line_index': i
                    }
                )
                lyrics_track.add_clip(clip)
        
        timeline.add_track(lyrics_track)
        
        # Update timeline widget
        if hasattr(self, 'timeline_widget'):
            self.timeline_widget.set_timeline_data(timeline)
        
        logger.info(f"Timeline initialized with {len(timeline.tracks)} tracks, {len(lyrics_track.clips)} lyric clips")
