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
from utils.theme_manager import ThemeManager
from core.plugin_manager import PluginManager


class MainWindow(QMainWindow):
    """Main application window"""
    
    def __init__(self, config: Config):
        super().__init__()
        
        self.config = config
        self.current_project = None  # Track active project
        self.setWindowTitle("NC-KTV v0.11.0 - Music Video Karaoke Maker")
        self.setWindowIcon(QIcon("assets/logo.png"))
        self.resize(1200, 800)
        
        # Initialize plugin and theme managers
        self.theme_manager = ThemeManager(config)
        self.plugin_manager = PluginManager(config)
        
        # Load theme and plugins
        self.theme_manager.initialize_default_theme()
        self.plugin_manager.load_all_plugins()
        
        # Check prerequisites
        self._check_prerequisites()
        
        # Create central widget with stacked layout
        self.central_widget = QStackedWidget()
        self.setCentralWidget(self.central_widget)
        
        # Initialize UI
        self._create_menu_bar()
        self._create_status_bar()
        self._load_mode()
        
        # Open in maximized mode by default for full workspace
        self.showMaximized()
    
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
        
        settings_menu.addSeparator()
        
        themes_action = QAction("🎨 &Themes", self)
        themes_action.triggered.connect(self._show_theme_manager)
        settings_menu.addAction(themes_action)
        
        plugins_action = QAction("🔌 &Plugins", self)
        plugins_action.triggered.connect(self._show_plugin_manager)
        settings_menu.addAction(plugins_action)
        
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
        # Ensure we don't lose work when switching modes or projects
        # Exception: switching to editor with SAME project is just a refresh/re-entry, logic below handles it
        # But if we are in editor and switching to wizard, or different project, check first.
        
        # Determine if we are effectively leaving the current editing session
        current_widget = self.central_widget.currentWidget()
        from gui.editor.editor_mode import EditorMode
        is_editing = isinstance(current_widget, EditorMode)
        
        # If we are already editing and switching to a DIFFERENT project or mode, check.
        # Logic below handles "same project" checks, but we need to check dirty before clearing.
        
        if is_editing:
             # If target is same project, don't nag.
             if mode == 'editor' and project and current_widget.project == project:
                 pass # Will fallback to "Already editing" logic below
             elif mode == 'editor' and not project and self.current_project == current_widget.project:
                 pass
             else:
                 # Switching to new project, wizard, or unrelated state
                 if not self._check_unsaved_changes():
                     return
        
        if mode == 'wizard':
            # Clear current central widget
            self._clear_central_widget()
            
            # Import and create wizard mode
            from gui.wizard_mode import WizardMode
            wizard = WizardMode(self.config)
            
            # Connect project created signal
            wizard.project_created.connect(lambda p: self._switch_mode('editor', p))
            
            self.central_widget.addWidget(wizard)
            self.central_widget.setCurrentWidget(wizard)
            self.statusBar().showMessage("Switched to Wizard Mode", 3000)
            
        elif mode == 'editor':
            # Resolve project priority: Argument > Active Project
            if project:
                self.current_project = project
            elif self.current_project:
                project = self.current_project
            
            if project:
                # Check if we are already in editor mode with this project
                current_widget = self.central_widget.currentWidget()
                from gui.editor.editor_mode import EditorMode
                
                if isinstance(current_widget, EditorMode) and current_widget.project == project:
                    self.statusBar().showMessage(f"Already editing: {project.name}", 3000)
                    return

                # Clear current central widget
                self._clear_central_widget()
                
                # Import and create editor mode
                editor = EditorMode(self.config, project)
                self.central_widget.addWidget(editor)
                self.central_widget.setCurrentWidget(editor)
                self.statusBar().showMessage(f"Opened Editor: {project.name}", 3000)
            else:
                # No project argument and no active project
                # Auto-initialize a blank project instead of showing a dialog
                from core.project import Project
                project = Project()
                self.current_project = project
                
                # Clear current central widget
                self._clear_central_widget()
                
                # Import and create editor mode
                editor = EditorMode(self.config, project)
                self.central_widget.addWidget(editor)
                self.central_widget.setCurrentWidget(editor)
                self.statusBar().showMessage(f"Opened Editor: New Project", 3000)
                 
        else:
            self.statusBar().showMessage(f"Unknown mode: {mode}", 3000)

    def _clear_central_widget(self):
        while self.central_widget.count() > 0:
            widget = self.central_widget.widget(0)
            
            # Safe cleanup for resource-heavy widgets
            if hasattr(widget, 'cleanup'):
                try:
                    widget.cleanup()
                except Exception as e:
                    print(f"Error cleaning up widget: {e}")
                    
            self.central_widget.removeWidget(widget)
            widget.deleteLater()

    
    def _new_project(self):
        """Create new project (Switch to Wizard)"""
        if self._check_unsaved_changes():
            self._switch_mode('wizard')
    
    def _open_project(self):
        """Open existing project"""
        from pathlib import Path
        from core.project import Project
        from PyQt6.QtWidgets import QFileDialog
        
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open Project",
            "",
            "NC-KTV Project (*.nctv)"
        )
        
        if file_path:
            try:
                project = Project.load(Path(file_path))
                self.statusBar().showMessage(f"Loaded project: {project.name}", 3000)
                self._switch_mode('editor', project)
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to load project:\n{e}")
    
    def _show_preferences(self):
        """Show preferences dialog"""
        from gui.dialogs.preferences_dialog import PreferencesDialog
        dialog = PreferencesDialog(self.config, self)
        if dialog.exec():
            # Apply immediate changes if needed (e.g. theme)
            pass
    
    def _show_theme_manager(self):
        """Show theme manager dialog"""
        from gui.dialogs.theme_manager_dialog import ThemeManagerDialog
        dialog = ThemeManagerDialog(self.theme_manager, self)
        dialog.exec()
    
    def _show_plugin_manager(self):
        """Show plugin manager dialog"""
        from gui.dialogs.plugin_manager_dialog import PluginManagerDialog
        dialog = PluginManagerDialog(self.plugin_manager, self)
        dialog.exec()
    
    def _show_about(self):
        """Show about dialog"""
        about_text = """
        <h2>NC-KTV - Music Video Karaoke Maker</h2>
        <p>Version 0.11.0</p>
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
    
    def _check_unsaved_changes(self):
        """
        Check for unsaved changes in current editor.
        Returns: True if safe to proceed (Saved, Discarded, or Nothing to save), False if Cancelled.
        """
        # Check if current widget is editor
        widget = self.central_widget.currentWidget()
        from gui.editor.editor_mode import EditorMode
        
        if isinstance(widget, EditorMode) and hasattr(widget, 'has_unsaved_changes'):
            if widget.has_unsaved_changes():
                project_name = widget.project.name if widget.project else "Project"
                
                reply = QMessageBox.question(
                    self,
                    "Unsaved Changes",
                    f"Project '{project_name}' has unsaved changes.\nDo you want to save them before closing?",
                    QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel
                )
                
                if reply == QMessageBox.StandardButton.Save:
                    # Trigger save
                    # Note: _save_project is technically internal but accessible
                    # Ideally expose a public method, but this works in Python
                    widget._save_project()
                    # Check if save was successful? _save_project shows its own success msg
                    # We assume if they clicked save and went through dialog, it's handled.
                    # But if they cancelled the file dialog in save, is_dirty remains True.
                    if widget.has_unsaved_changes():
                         # Save cancelled or failed
                         return False
                    return True
                    
                elif reply == QMessageBox.StandardButton.Discard:
                    return True
                    
                elif reply == QMessageBox.StandardButton.Cancel:
                    return False
                    
        return True

    def closeEvent(self, event):
        """Handle window close event"""
        if self._check_unsaved_changes():
            event.accept()
        else:
            event.ignore()
