"""
Theme Manager Dialog
GUI for browsing, previewing, and managing themes
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QPushButton, QLabel,
    QListWidget, QListWidgetItem, QTextEdit, QFileDialog,
    QMessageBox, QGroupBox, QGridLayout, QFrame
)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QPixmap, QColor
from pathlib import Path

from utils.theme_manager import ThemeManager, ThemeMetadata


class ThemeCard(QFrame):
    """Visual card for a theme"""
    
    def __init__(self, metadata: ThemeMetadata, is_active: bool = False):
        super().__init__()
        self.metadata = metadata
        self.is_active = is_active
        
        self.setFrameStyle(QFrame.Shape.Box | QFrame.Shadow.Raised)
        self.setLineWidth(2 if is_active else 1)
        
        layout = QVBoxLayout()
        
        # Theme name
        name_label = QLabel(f"<b>{metadata.name}</b>")
        name_label.setStyleSheet("font-size: 12pt;")
        layout.addWidget(name_label)
        
        # Author
        author_label = QLabel(f"by {metadata.author}")
        author_label.setStyleSheet("color: #666; font-size: 9pt;")
        layout.addWidget(author_label)
        
        # Description
        if metadata.description:
            desc_label = QLabel(metadata.description)
            desc_label.setWordWrap(True)
            desc_label.setStyleSheet("color: #888; font-size: 9pt; font-style: italic;")
            layout.addWidget(desc_label)
        
        # Version
        version_label = QLabel(f"v{metadata.version}")
        version_label.setStyleSheet("color: #999; font-size: 8pt;")
        layout.addWidget(version_label)
        
        # Active indicator
        if is_active:
            active_label = QLabel("✓ ACTIVE")
            active_label.setStyleSheet("color: #4CAF50; font-weight: bold;")
            layout.addWidget(active_label)
        
        self.setLayout(layout)
        self.setMinimumHeight(120)


class ThemeManagerDialog(QDialog):
    """Dialog for managing themes"""
    
    theme_changed = pyqtSignal(str)  # Emits theme ID when changed
    
    def __init__(self, theme_manager: ThemeManager, parent=None):
        super().__init__(parent)
        self.theme_manager = theme_manager
        self.setWindowTitle("Theme Manager")
        self.resize(800, 600)
        
        self._init_ui()
        self._load_themes()
    
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout()
        
        # Title
        title = QLabel("Manage Themes")
        title.setStyleSheet("font-size: 16pt; font-weight: bold; padding: 10px;")
        layout.addWidget(title)
        
        # Main content layout
        content_layout = QHBoxLayout()
        
        # Left: Theme list
        left_panel = QVBoxLayout()
        
        list_label = QLabel("Installed Themes:")
        list_label.setStyleSheet("font-weight: bold;")
        left_panel.addWidget(list_label)
        
        self.theme_list = QListWidget()
        self.theme_list.currentItemChanged.connect(self._on_theme_selected)
        left_panel.addWidget(self.theme_list)
        
        # Action buttons
        btn_layout = QHBoxLayout()
        
        btn_apply = QPushButton("Apply")
        btn_apply.clicked.connect(self._apply_theme)
        btn_apply.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
        btn_layout.addWidget(btn_apply)
        
        btn_import = QPushButton("Import...")
        btn_import.clicked.connect(self._import_theme)
        btn_layout.addWidget(btn_import)
        
        btn_uninstall = QPushButton("Uninstall")
        btn_uninstall.clicked.connect(self._uninstall_theme)
        btn_layout.addWidget(btn_uninstall)
        
        left_panel.addLayout(btn_layout)
        
        # Right: Theme preview
        right_panel = QVBoxLayout()
        
        preview_label = QLabel("Theme Preview:")
        preview_label.setStyleSheet("font-weight: bold;")
        right_panel.addWidget(preview_label)
        
        self.preview_widget = QGroupBox("Preview")
        preview_layout = QVBoxLayout()
        
        self.theme_info = QTextEdit()
        self.theme_info.setReadOnly(True)
        self.theme_info.setMaximumHeight(200)
        preview_layout.addWidget(self.theme_info)
        
        # Color palette preview
        self.color_preview = QGroupBox("Color Palette")
        color_layout = QGridLayout()
        self.color_labels = {}
        color_names = ['background', 'foreground', 'primary', 'accent', 'success', 'warning', 'error']
        for i, color_name in enumerate(color_names):
            label = QLabel(color_name.capitalize())
            color_layout.addWidget(label, i, 0)
            
            color_box = QLabel()
            color_box.setFixedSize(100, 30)
            color_box.setFrameStyle(QFrame.Shape.Box)
            self.color_labels[color_name] = color_box
            color_layout.addWidget(color_box, i, 1)
        
        self.color_preview.setLayout(color_layout)
        preview_layout.addWidget(self.color_preview)
        
        self.preview_widget.setLayout(preview_layout)
        right_panel.addWidget(self.preview_widget)
        
        # Add panels to content layout
        content_layout.addLayout(left_panel, 1)
        content_layout.addLayout(right_panel, 1)
        
        layout.addLayout(content_layout)
        
        # Bottom buttons
        bottom_layout = QHBoxLayout()
        bottom_layout.addStretch()
        
        btn_close = QPushButton("Close")
        btn_close.clicked.connect(self.accept)
        bottom_layout.addWidget(btn_close)
        
        layout.addLayout(bottom_layout)
        
        self.setLayout(layout)
    
    def _load_themes(self):
        """Load and display available themes"""
        self.theme_list.clear()
        
        # Get active theme ID
        active_theme = self.theme_manager.get_active_theme()
        active_id = active_theme.metadata.id if active_theme else None
        
        # Discover and load all themes
        discovered = self.theme_manager.discover_themes()
        
        for metadata in discovered:
            item = QListWidgetItem(self.theme_list)
            
            # Create theme card
            is_active = (metadata.id == active_id)
            card = ThemeCard(metadata, is_active)
            
            item.setSizeHint(card.sizeHint())
            item.setData(Qt.ItemDataRole.UserRole, metadata.id)
            
            self.theme_list.addItem(item)
            self.theme_list.setItemWidget(item, card)
    
    def _on_theme_selected(self, current, previous):
        """Handle theme selection"""
        if not current:
            return
        
        theme_id = current.data(Qt.ItemDataRole.UserRole)
        
        # Load theme if not loaded
        if theme_id not in self.theme_manager.themes:
            self.theme_manager.load_theme(theme_id)
        
        theme = self.theme_manager.themes.get(theme_id)
        if not theme:
            return
        
        # Update preview
        info_text = f"""
        <h3>{theme.metadata.name}</h3>
        <p><b>Author:</b> {theme.metadata.author}</p>
        <p><b>Version:</b> {theme.metadata.version}</p>
        <p><b>ID:</b> {theme.metadata.id}</p>
        <p><b>Description:</b> {theme.metadata.description or 'No description'}</p>
        <p><b>Karaoke Styles:</b> {len(theme.karaoke_styles)}</p>
        """
        self.theme_info.setHtml(info_text)
        
        # Update color preview
        for color_name, color_box in self.color_labels.items():
            color_value = theme.get_color(color_name)
            color_box.setStyleSheet(f"background-color: {color_value}; border: 1px solid #888;")
    
    def _apply_theme(self):
        """Apply selected theme"""
        current = self.theme_list.currentItem()
        if not current:
            QMessageBox.warning(self, "No Theme Selected", "Please select a theme to apply.")
            return
        
        theme_id = current.data(Qt.ItemDataRole.UserRole)
        
        if self.theme_manager.set_active_theme(theme_id):
            QMessageBox.information(
                self,
                "Theme Applied",
                f"Theme '{theme_id}' has been set as active.\n\n"
                "Note: Some changes may require restarting the application."
            )
            self.theme_changed.emit(theme_id)
            self._load_themes()  # Refresh to show new active theme
        else:
            QMessageBox.critical(self, "Error", f"Failed to apply theme: {theme_id}")
    
    def _import_theme(self):
        """Import theme package"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Import Theme Package",
            "",
            "NC-KTV Theme (*.ncktheme);;All Files (*)"
        )
        
        if file_path:
            if self.theme_manager.install_theme_package(Path(file_path)):
                QMessageBox.information(self, "Success", "Theme imported successfully!")
                self._load_themes()
            else:
                QMessageBox.critical(self, "Error", "Failed to import theme package.")
    
    def _uninstall_theme(self):
        """Uninstall selected theme"""
        current = self.theme_list.currentItem()
        if not current:
            QMessageBox.warning(self, "No Theme Selected", "Please select a theme to uninstall.")
            return
        
        theme_id = current.data(Qt.ItemDataRole.UserRole)
        
        # Prevent uninstalling built-in themes
        if theme_id.startswith('builtin-'):
            QMessageBox.warning(self, "Cannot Uninstall", "Built-in themes cannot be uninstalled.")
            return
        
        # Prevent uninstalling active theme
        active_theme = self.theme_manager.get_active_theme()
        if active_theme and active_theme.metadata.id == theme_id:
            QMessageBox.warning(
                self,
                "Cannot Uninstall",
                "Cannot uninstall the currently active theme.\nPlease switch to a different theme first."
            )
            return
        
        reply = QMessageBox.question(
            self,
            "Confirm Uninstall",
            f"Are you sure you want to uninstall theme '{theme_id}'?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            if self.theme_manager.uninstall_theme(theme_id):
                QMessageBox.information(self, "Success", "Theme uninstalled successfully!")
                self._load_themes()
            else:
                QMessageBox.critical(self, "Error", "Failed to uninstall theme.")
