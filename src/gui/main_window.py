"""
Main application window
Handles wizard and editor mode switching
"""

from PyQt6.QtWidgets import (
    QMainWindow, QStackedWidget, QMessageBox,
    QMenuBar, QMenu, QStatusBar
)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QAction, QIcon

from utils.config import Config
from utils.ffmpeg_utils import check_ffmpeg
from utils.gpu_detector import GPUDetector


class MainWindow(QMainWindow):
    """Main application window"""
    
    def __init__(self, config: Config):
        super().__init__()
        
        self.config = config
        self.setWindowTitle("NC-KTV - Music Video Karaoke Maker")
        self.resize(1200, 800)
        
        # Check prerequisites
        self._check_prerequisites()
        
        # Create central widget with stacked layout
        self.central_widget = QStackedWidget()
        self.setCentralWidget(self.central_widget)
        
        # Initialize UI
        self._create_menu_bar()
        self._create_status_bar()
        self._load_mode()
    
    def _check_prerequisites(self):
        """Check if required tools are installed"""
        issues = []
        
        # Check FFmpeg
        if not check_ffmpeg():
            issues.append("FFmpeg is not installed or not in system PATH")
        
        # Check GPU
        gpu_info = GPUDetector.get_gpu_info()
        if gpu_info:
            gpu_msg = f"GPU: {gpu_info['name']} ({gpu_info['total_memory_gb']:.1f} GB)"
        else:
            gpu_msg = "GPU: Not available (will use CPU - slower processing)"
        
        # Show info dialog
        if issues:
            msg = "⚠️ Setup Issues:\n\n" + "\n".join(f"• {issue}" for issue in issues)
            msg += f"\n\n{gpu_msg}"
            QMessageBox.warning(self, "Setup Check", msg)
        else:
            # Just log GPU info to status bar
            QTimer.singleShot(100, lambda: self.statusBar().showMessage(gpu_msg, 5000))
    
    def _create_menu_bar(self):
        """Create application menu bar"""
        menubar = self.menuBar()
        
        # File menu
        file_menu = menubar.addMenu("&File")
        
        new_action = QAction("&New Project", self)
        new_action.setShortcut("Ctrl+N")
        new_action.triggered.connect(self._new_project)
        file_menu.addAction(new_action)
        
        open_action = QAction("&Open Project", self)
        open_action.setShortcut("Ctrl+O")
        open_action.triggered.connect(self._open_project)
        file_menu.addAction(open_action)
        
        file_menu.addSeparator()
        
        exit_action = QAction("E&xit", self)
        exit_action.setShortcut("Ctrl+Q")
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)
        
        # View menu
        view_menu = menubar.addMenu("&View")
        
        wizard_action = QAction("&Wizard Mode", self)
        wizard_action.triggered.connect(lambda: self._switch_mode('wizard'))
        view_menu.addAction(wizard_action)
        
        editor_action = QAction("&Advanced Editor", self)
        editor_action.triggered.connect(lambda: self._switch_mode('editor'))
        view_menu.addAction(editor_action)
        
        # Settings menu
        settings_menu = menubar.addMenu("&Settings")
        
        preferences_action = QAction("&Preferences", self)
        preferences_action.triggered.connect(self._show_preferences)
        settings_menu.addAction(preferences_action)
        
        # Help menu
        help_menu = menubar.addMenu("&Help")
        
        about_action = QAction("&About", self)
        about_action.triggered.connect(self._show_about)
        help_menu.addAction(about_action)
    
    def _create_status_bar(self):
        """Create status bar"""
        self.statusBar().showMessage("Ready")
    
    def _load_mode(self):
        """Load initial mode from config"""
        start_mode = self.config.get('gui.start_mode', 'wizard')
        self._switch_mode(start_mode)
    
    def _switch_mode(self, mode: str, project=None):
        """Switch between wizard and editor mode
        
        Args:
            mode: 'wizard' or 'editor'
            project: Optional Project object to pass to editor
        """
        # Clear current central widget
        while self.central_widget.count() > 0:
            widget = self.central_widget.widget(0)
            self.central_widget.removeWidget(widget)
            widget.deleteLater()
            
        if mode == 'wizard':
            # Import and create wizard mode
            from gui.wizard_mode import WizardMode
            wizard = WizardMode(self.config)
            
            # Connect project created signal
            wizard.project_created.connect(lambda p: self._switch_mode('editor', p))
            
            self.central_widget.addWidget(wizard)
            self.central_widget.setCurrentWidget(wizard)
            self.statusBar().showMessage("Switched to Wizard Mode", 3000)
            
        elif mode == 'editor':
            # Import and create editor mode
            from gui.editor.editor_mode import EditorMode
            
            if project:
                editor = EditorMode(self.config, project)
                self.central_widget.addWidget(editor)
                self.central_widget.setCurrentWidget(editor)
                self.statusBar().showMessage(f"Opened Editor: {project.name}", 3000)
            else:
                 QMessageBox.information(self, "Editor Mode", "Please create a project via Wizard Mode first.")
                 self._switch_mode('wizard')
                 
        else:
            self.statusBar().showMessage(f"Unknown mode: {mode}", 3000)

    
    def _new_project(self):
        """Create new project"""
        # TODO: Implement
        QMessageBox.information(self, "New Project", "New project creation coming soon!")
    
    def _open_project(self):
        """Open existing project"""
        # TODO: Implement
        QMessageBox.information(self, "Open Project", "Project opening coming soon!")
    
    def _show_preferences(self):
        """Show preferences dialog"""
        # TODO: Implement
        QMessageBox.information(self, "Preferences", "Preferences dialog coming soon!")
    
    def _show_about(self):
        """Show about dialog"""
        about_text = """
        <h2>NC-KTV - Music Video Karaoke Maker</h2>
        <p>Version 0.1.0</p>
        <p>Professional karaoke video creation with automatic vocal removal and lyrics synchronization.</p>
        <p><b>Features:</b></p>
        <ul>
            <li>GPU-accelerated vocal removal (UVR 5)</li>
            <li>AI-powered lyrics synchronization (Whisper)</li>
            <li>Dual interface modes (Wizard & Advanced Editor)</li>
            <li>Professional karaoke effects</li>
        </ul>
        <p><b>Technology:</b> PyQt6, PyTorch, FFmpeg, OpenAI Whisper</p>
        """
        QMessageBox.about(self, "About NC-KTV", about_text)
    
    def closeEvent(self, event):
        """Handle window close event"""
        # TODO: Check for unsaved changes
        event.accept()
