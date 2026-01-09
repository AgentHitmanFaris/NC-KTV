"""
Plugin Manager Dialog
GUI for managing plugins (install, enable/disable, configure)
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QPushButton, QLabel,
    QListWidget, QListWidgetItem, QTextEdit, QFileDialog,
    QMessageBox, QCheckBox, QGroupBox, QFrame
)
from PyQt6.QtCore import Qt, pyqtSignal
from pathlib import Path

from core.plugin_manager import PluginManager
from core.plugin_base import PluginMetadata, PluginPermission


class PluginCard(QFrame):
    """Visual card for a plugin"""
    
    enabled_changed = pyqtSignal(str, bool)  # plugin_id, enabled
    
    def __init__(self, metadata: PluginMetadata, is_enabled: bool = False):
        super().__init__()
        self.metadata = metadata
        
        self.setFrameStyle(QFrame.Shape.Box | QFrame.Shadow.Raised)
        
        layout = QVBoxLayout()
        
        # Header with name and checkbox
        header_layout = QHBoxLayout()
        
        name_label = QLabel(f"<b>{metadata.name}</b>")
        name_label.setStyleSheet("font-size: 12pt;")
        header_layout.addWidget(name_label)
        
        header_layout.addStretch()
        
        self.enabled_checkbox = QCheckBox("Enabled")
        self.enabled_checkbox.setChecked(is_enabled)
        self.enabled_checkbox.stateChanged.connect(
            lambda state: self.enabled_changed.emit(metadata.id, state == Qt.CheckState.Checked.value)
        )
        header_layout.addWidget(self.enabled_checkbox)
        
        layout.addLayout(header_layout)
        
        # Author and version
        info_layout = QHBoxLayout()
        author_label = QLabel(f"by {metadata.author}")
        author_label.setStyleSheet("color: #666; font-size: 9pt;")
        info_layout.addWidget(author_label)
        
        info_layout.addStretch()
        
        version_label = QLabel(f"v{metadata.version}")
        version_label.setStyleSheet("color: #999; font-size: 8pt;")
        info_layout.addWidget(version_label)
        
        layout.addLayout(info_layout)
        
        # Description
        if metadata.description:
            desc_label = QLabel(metadata.description)
            desc_label.setWordWrap(True)
            desc_label.setStyleSheet("color: #888; font-size: 9pt; font-style: italic;")
            layout.addWidget(desc_label)
        
        # Type badge
        type_label = QLabel(f"Type: {metadata.plugin_type.value}")
        type_label.setStyleSheet("color: #0066cc; font-size: 9pt; font-weight: bold;")
        layout.addWidget(type_label)
        
        # Permissions
        if metadata.permissions:
            perm_text = "Permissions: " + ", ".join([p.value for p in metadata.permissions])
            perm_label = QLabel(perm_text)
            perm_label.setStyleSheet("color: #FF9800; font-size: 8pt;")
            layout.addWidget(perm_label)
        
        self.setLayout(layout)
        self.setMinimumHeight(140)


class PluginManagerDialog(QDialog):
    """Dialog for managing plugins"""
    
    def __init__(self, plugin_manager: PluginManager, parent=None):
        super().__init__(parent)
        self.plugin_manager = plugin_manager
        self.setWindowTitle("Plugin Manager")
        self.resize(900, 700)
        
        self._init_ui()
        self._load_plugins()
    
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout()
        
        # Title
        title = QLabel("Manage Plugins")
        title.setStyleSheet("font-size: 16pt; font-weight: bold; padding: 10px;")
        layout.addWidget(title)
        
        # Info banner
        info = QLabel(
            "⚠️ Plugins can extend NC-KTV functionality. "
            "Only install plugins from trusted sources."
        )
        info.setStyleSheet("color: #FF9800; padding: 10px; background-color: #FFF3E0; border-radius: 5px;")
        info.setWordWrap(True)
        layout.addWidget(info)
        
        # Main content layout
        content_layout = QHBoxLayout()
        
        # Left: Plugin list
        left_panel = QVBoxLayout()
        
        list_label = QLabel("Installed Plugins:")
        list_label.setStyleSheet("font-weight: bold;")
        left_panel.addWidget(list_label)
        
        self.plugin_list = QListWidget()
        self.plugin_list.currentItemChanged.connect(self._on_plugin_selected)
        left_panel.addWidget(self.plugin_list)
        
        # Action buttons
        btn_layout = QHBoxLayout()
        
        btn_import = QPushButton("Install Plugin...")
        btn_import.clicked.connect(self._install_plugin)
        btn_import.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
        btn_layout.addWidget(btn_import)
        
        btn_uninstall = QPushButton("Uninstall")
        btn_uninstall.clicked.connect(self._uninstall_plugin)
        btn_layout.addWidget(btn_uninstall)
        
        btn_reload = QPushButton("Reload All")
        btn_reload.clicked.connect(self._reload_plugins)
        btn_layout.addWidget(btn_reload)
        
        left_panel.addLayout(btn_layout)
        
        # Right: Plugin details
        right_panel = QVBoxLayout()
        
        details_label = QLabel("Plugin Details:")
        details_label.setStyleSheet("font-weight: bold;")
        right_panel.addWidget(details_label)
        
        self.plugin_details = QTextEdit()
        self.plugin_details.setReadOnly(True)
        right_panel.addWidget(self.plugin_details)
        
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
    
    def _load_plugins(self):
        """Load and display available plugins"""
        self.plugin_list.clear()
        
        # Discover all plugins
        discovered = self.plugin_manager.discover_plugins()
        
        for metadata in discovered:
            item = QListWidgetItem(self.plugin_list)
            
            # Check if plugin is loaded and enabled
            plugin = self.plugin_manager.get_plugin(metadata.id)
            is_enabled = plugin.enabled if plugin else False
            
            # Create plugin card
            card = PluginCard(metadata, is_enabled)
            card.enabled_changed.connect(self._on_plugin_enabled_changed)
            
            item.setSizeHint(card.sizeHint())
            item.setData(Qt.ItemDataRole.UserRole, metadata.id)
            
            self.plugin_list.addItem(item)
            self.plugin_list.setItemWidget(item, card)
    
    def _on_plugin_selected(self, current, previous):
        """Handle plugin selection"""
        if not current:
            return
        
        plugin_id = current.data(Qt.ItemDataRole.UserRole)
        
        # Get metadata
        metadata = self.plugin_manager.metadata.get(plugin_id)
        if not metadata:
            # Try to load from discovered
            discovered = self.plugin_manager.discover_plugins()
            for m in discovered:
                if m.id == plugin_id:
                    metadata = m
                    break
        
        if not metadata:
            return
        
        # Display details
        details_html = f"""
        <h2>{metadata.name}</h2>
        <p><b>ID:</b> {metadata.id}</p>
        <p><b>Author:</b> {metadata.author}</p>
        <p><b>Version:</b> {metadata.version}</p>
        <p><b>Type:</b> {metadata.plugin_type.value}</p>
        <p><b>Description:</b> {metadata.description or 'No description provided'}</p>
        <p><b>Entry Point:</b> {metadata.entry_point}</p>
        
        <h3>Permissions</h3>
        <ul>
        """
        
        if metadata.permissions:
            for perm in metadata.permissions:
                details_html += f"<li>{perm.value}</li>"
        else:
            details_html += "<li>No special permissions required</li>"
        
        details_html += "</ul><h3>Dependencies</h3><ul>"
        
        if metadata.dependencies:
            for dep, version in metadata.dependencies.items():
                details_html += f"<li>{dep}: {version}</li>"
        else:
            details_html += "<li>No dependencies</li>"
        
        details_html += "</ul>"
        
        self.plugin_details.setHtml(details_html)
    
    def _on_plugin_enabled_changed(self, plugin_id: str, enabled: bool):
        """Handle plugin enable/disable"""
        if enabled:
            # Check if plugin is loaded
            if plugin_id not in self.plugin_manager.plugins:
                # Need to load first
                discovered = self.plugin_manager.discover_plugins()
                for metadata in discovered:
                    if metadata.id == plugin_id:
                        # Find plugin directory
                        for base_dir in [self.plugin_manager.INSTALLED_DIR, self.plugin_manager.EXAMPLES_DIR]:
                            plugin_dir = base_dir / plugin_id
                            if plugin_dir.exists():
                                if not self.plugin_manager.load_plugin(plugin_id, plugin_dir):
                                    QMessageBox.critical(
                                        self,
                                        "Load Failed",
                                        f"Failed to load plugin: {plugin_id}"
                                    )
                                    return
                                break
            
            self.plugin_manager.enable_plugin(plugin_id)
            QMessageBox.information(
                self,
                "Plugin Enabled",
                f"Plugin '{plugin_id}' has been enabled.\n\n"
                "Some changes may require restarting the application."
            )
        else:
            self.plugin_manager.disable_plugin(plugin_id)
            QMessageBox.information(self, "Plugin Disabled", f"Plugin '{plugin_id}' has been disabled.")
    
    def _install_plugin(self):
        """Install plugin package"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Install Plugin Package",
            "",
            "NC-KTV Plugin (*.nckplugin);;All Files (*)"
        )
        
        if file_path:
            if self.plugin_manager.install_plugin_package(Path(file_path)):
                QMessageBox.information(self, "Success", "Plugin installed successfully!")
                self._reload_plugins()
            else:
                QMessageBox.critical(self, "Error", "Failed to install plugin package.")
    
    def _uninstall_plugin(self):
        """Uninstall selected plugin"""
        current = self.plugin_list.currentItem()
        if not current:
            QMessageBox.warning(self, "No Plugin Selected", "Please select a plugin to uninstall.")
            return
        
        plugin_id = current.data(Qt.ItemDataRole.UserRole)
        
        # Check if it's an example plugin
        example_dir = self.plugin_manager.EXAMPLES_DIR / plugin_id
        if example_dir.exists():
            QMessageBox.warning(
                self,
                "Cannot Uninstall",
                "Example plugins cannot be uninstalled."
            )
            return
        
        reply = QMessageBox.question(
            self,
            "Confirm Uninstall",
            f"Are you sure you want to uninstall plugin '{plugin_id}'?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            if self.plugin_manager.uninstall_plugin(plugin_id):
                QMessageBox.information(self, "Success", "Plugin uninstalled successfully!")
                self._load_plugins()
            else:
                QMessageBox.critical(self, "Error", "Failed to uninstall plugin.")
    
    def _reload_plugins(self):
        """Reload all plugins"""
        self.plugin_manager.load_all_plugins()
        self._load_plugins()
        QMessageBox.information(self, "Success", "Plugins reloaded successfully!")
