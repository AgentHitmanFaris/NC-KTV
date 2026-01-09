"""
Plugin Manager for NC-KTV
Handles plugin discovery, loading, validation, and lifecycle management
"""

import json
import importlib.util
import logging
from pathlib import Path
from typing import Dict, List, Optional, Type
from zipfile import ZipFile
import tempfile
import shutil

from core.plugin_base import (
    Plugin, PluginMetadata, PluginType, PluginPermission,
    EffectPlugin, ExportTemplatePlugin, UIExtensionPlugin, PluginAPI
)

logger = logging.getLogger(__name__)


class PluginLoadError(Exception):
    """Exception raised when plugin loading fails"""
    pass


class PluginManager:
    """Manages plugin discovery, loading, and lifecycle"""
    
    PLUGIN_DIR = Path("plugins")
    INSTALLED_DIR = PLUGIN_DIR / "installed"
    EXAMPLES_DIR = PLUGIN_DIR / "examples"
    
    def __init__(self, config, project_manager=None):
        """
        Initialize plugin manager
        
        Args:
            config: Application configuration
            project_manager: Project manager instance (optional)
        """
        self.config = config
        self.project_manager = project_manager
        self.api = PluginAPI(config, project_manager)
        
        # Plugin storage
        self.plugins: Dict[str, Plugin] = {}
        self.metadata: Dict[str, PluginMetadata] = {}
        
        # Ensure plugin directories exist
        self.PLUGIN_DIR.mkdir(exist_ok=True)
        self.INSTALLED_DIR.mkdir(exist_ok=True)
        self.EXAMPLES_DIR.mkdir(exist_ok=True)
        
        logger.info("Plugin manager initialized")
    
    def discover_plugins(self) -> List[PluginMetadata]:
        """
        Scan plugin directories and discover available plugins
        
        Returns:
            List of plugin metadata
        """
        discovered = []
        
        # Scan both installed and examples directories
        for plugin_dir in [self.INSTALLED_DIR, self.EXAMPLES_DIR]:
            if not plugin_dir.exists():
                continue
            
            for item in plugin_dir.iterdir():
                if item.is_dir():
                    manifest_path = item / "plugin.json"
                    if manifest_path.exists():
                        try:
                            metadata = self._load_manifest(manifest_path)
                            discovered.append(metadata)
                            logger.info(f"Discovered plugin: {metadata.name} ({metadata.id})")
                        except Exception as e:
                            logger.error(f"Failed to load manifest from {manifest_path}: {e}")
        
        return discovered
    
    def _load_manifest(self, manifest_path: Path) -> PluginMetadata:
        """Load and validate plugin manifest"""
        with open(manifest_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        # Validate required fields
        required = ['id', 'name', 'version', 'author', 'type']
        for field in required:
            if field not in data:
                raise PluginLoadError(f"Missing required field: {field}")
        
        return PluginMetadata.from_dict(data)
    
    def load_plugin(self, plugin_id: str, plugin_dir: Path) -> bool:
        """
        Load a plugin from directory
        
        Args:
            plugin_id: Unique plugin identifier
            plugin_dir: Path to plugin directory
            
        Returns:
            True if loaded successfully, False otherwise
        """
        try:
            # Load manifest
            manifest_path = plugin_dir / "plugin.json"
            if not manifest_path.exists():
                raise PluginLoadError(f"Manifest not found: {manifest_path}")
            
            metadata = self._load_manifest(manifest_path)
            
            # Check if already loaded
            if plugin_id in self.plugins:
                logger.warning(f"Plugin {plugin_id} already loaded")
                return False
            
            # Check permissions
            if not self._check_permissions(metadata):
                logger.warning(f"User denied permissions for plugin: {plugin_id}")
                return False
            
            # Load Python module
            entry_point = plugin_dir / metadata.entry_point
            if not entry_point.exists():
                raise PluginLoadError(f"Entry point not found: {entry_point}")
            
            plugin_instance = self._load_module(entry_point, metadata)
            
            # Initialize plugin
            if not plugin_instance.initialize(self.api):
                raise PluginLoadError(f"Plugin initialization failed: {plugin_id}")
            
            # Store plugin
            self.plugins[plugin_id] = plugin_instance
            self.metadata[plugin_id] = metadata
            
            # Enable if configured
            if self._is_plugin_enabled(plugin_id):
                plugin_instance.enable()
            
            logger.info(f"Loaded plugin: {metadata.name} ({plugin_id})")
            return True
            
        except Exception as e:
            logger.error(f"Failed to load plugin {plugin_id}: {e}")
            return False
    
    def _load_module(self, entry_point: Path, metadata: PluginMetadata) -> Plugin:
        """Load Python module and instantiate plugin"""
        # Load module dynamically
        spec = importlib.util.spec_from_file_location(metadata.id, entry_point)
        if spec is None or spec.loader is None:
            raise PluginLoadError(f"Failed to load module spec: {entry_point}")
        
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        
        # Find plugin class
        # Convention: Plugin class should be named after the plugin type
        class_name_map = {
            PluginType.EFFECT: 'EffectPlugin',
            PluginType.EXPORT_TEMPLATE: 'ExportTemplatePlugin',
            PluginType.UI_EXTENSION: 'UIExtensionPlugin'
        }
        
        # Try to find the plugin class
        plugin_class = None
        expected_class = class_name_map.get(metadata.plugin_type)
        
        # First, try to find a class with the expected name or "Plugin"
        for attr_name in dir(module):
            attr = getattr(module, attr_name)
            if isinstance(attr, type) and issubclass(attr, Plugin) and attr != Plugin:
                # Prefer exact match
                if attr_name == expected_class or attr_name == 'Plugin':
                    plugin_class = attr
                    break
                # Fallback to first Plugin subclass found
                if plugin_class is None:
                    plugin_class = attr
        
        if plugin_class is None:
            raise PluginLoadError(f"No Plugin class found in {entry_point}")
        
        # Instantiate
        return plugin_class(metadata)
    
    def _check_permissions(self, metadata: PluginMetadata) -> bool:
        """
        Check if user grants requested permissions
        
        Args:
            metadata: Plugin metadata with permissions
            
        Returns:
            True if permissions granted, False otherwise
        """
        # For now, auto-approve (in production, show dialog)
        # TODO: Implement permission dialog
        if metadata.permissions:
            logger.info(f"Plugin {metadata.id} requests: {[p.value for p in metadata.permissions]}")
        return True
    
    def _is_plugin_enabled(self, plugin_id: str) -> bool:
        """Check if plugin is enabled in config"""
        enabled_plugins = self.config.get('plugins.enabled', [])
        return plugin_id in enabled_plugins
    
    def unload_plugin(self, plugin_id: str) -> bool:
        """
        Unload a plugin
        
        Args:
            plugin_id: Plugin identifier
            
        Returns:
            True if unloaded successfully
        """
        if plugin_id not in self.plugins:
            logger.warning(f"Plugin not loaded: {plugin_id}")
            return False
        
        try:
            plugin = self.plugins[plugin_id]
            plugin.cleanup()
            
            del self.plugins[plugin_id]
            del self.metadata[plugin_id]
            
            logger.info(f"Unloaded plugin: {plugin_id}")
            return True
        except Exception as e:
            logger.error(f"Failed to unload plugin {plugin_id}: {e}")
            return False
    
    def enable_plugin(self, plugin_id: str):
        """Enable a loaded plugin"""
        if plugin_id in self.plugins:
            self.plugins[plugin_id].enable()
            # Update config
            enabled = self.config.get('plugins.enabled', [])
            if plugin_id not in enabled:
                enabled.append(plugin_id)
                self.config.set('plugins.enabled', enabled)
                self.config.save()
            logger.info(f"Enabled plugin: {plugin_id}")
    
    def disable_plugin(self, plugin_id: str):
        """Disable a loaded plugin"""
        if plugin_id in self.plugins:
            self.plugins[plugin_id].disable()
            # Update config
            enabled = self.config.get('plugins.enabled', [])
            if plugin_id in enabled:
                enabled.remove(plugin_id)
                self.config.set('plugins.enabled', enabled)
                self.config.save()
            logger.info(f"Disabled plugin: {plugin_id}")
    
    def get_plugins_by_type(self, plugin_type: PluginType) -> List[Plugin]:
        """Get all enabled plugins of a specific type"""
        return [
            plugin for plugin_id, plugin in self.plugins.items()
            if self.metadata[plugin_id].plugin_type == plugin_type and plugin.enabled
        ]
    
    def get_plugin(self, plugin_id: str) -> Optional[Plugin]:
        """Get plugin by ID"""
        return self.plugins.get(plugin_id)
    
    def get_all_plugins(self) -> Dict[str, Plugin]:
        """Get all loaded plugins"""
        return self.plugins.copy()
    
    def install_plugin_package(self, package_path: Path) -> bool:
        """
        Install plugin from .nckplugin package
        
        Args:
            package_path: Path to .nckplugin file
            
        Returns:
            True if installed successfully
        """
        try:
            # Extract to temporary directory
            with tempfile.TemporaryDirectory() as temp_dir:
                temp_path = Path(temp_dir)
                
                # Extract zip
                with ZipFile(package_path, 'r') as zip_ref:
                    zip_ref.extractall(temp_path)
                
                # Validate manifest
                manifest_path = temp_path / "plugin.json"
                if not manifest_path.exists():
                    raise PluginLoadError("Package missing plugin.json")
                
                metadata = self._load_manifest(manifest_path)
                
                # Check if plugin directory already exists
                target_dir = self.INSTALLED_DIR / metadata.id
                if target_dir.exists():
                    logger.warning(f"Plugin {metadata.id} already installed, will overwrite")
                    shutil.rmtree(target_dir)
                
                # Copy to installed directory
                shutil.copytree(temp_path, target_dir)
                
                logger.info(f"Installed plugin: {metadata.name} ({metadata.id})")
                return True
                
        except Exception as e:
            logger.error(f"Failed to install plugin package: {e}")
            return False
    
    def uninstall_plugin(self, plugin_id: str) -> bool:
        """
        Uninstall a plugin
        
        Args:
            plugin_id: Plugin identifier
            
        Returns:
            True if uninstalled successfully
        """
        # Unload first
        if plugin_id in self.plugins:
            self.unload_plugin(plugin_id)
        
        # Remove directory
        plugin_dir = self.INSTALLED_DIR / plugin_id
        if plugin_dir.exists():
            try:
                shutil.rmtree(plugin_dir)
                logger.info(f"Uninstalled plugin: {plugin_id}")
                return True
            except Exception as e:
                logger.error(f"Failed to uninstall plugin {plugin_id}: {e}")
                return False
        else:
            logger.warning(f"Plugin directory not found: {plugin_dir}")
            return False
    
    def load_all_plugins(self):
        """Discover and load all available plugins"""
        discovered = self.discover_plugins()
        
        for metadata in discovered:
            # Determine plugin directory
            plugin_dir = None
            for base_dir in [self.INSTALLED_DIR, self.EXAMPLES_DIR]:
                candidate = base_dir / metadata.id
                if candidate.exists():
                    plugin_dir = candidate
                    break
            
            if plugin_dir:
                self.load_plugin(metadata.id, plugin_dir)
