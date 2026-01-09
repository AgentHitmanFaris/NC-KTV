"""
Base classes and interfaces for NC-KTV plugins
Provides the foundation for creating custom effects, export templates, and UI extensions
"""

from abc import ABC, abstractmethod
from typing import List, Dict, Any, Optional
from dataclasses import dataclass
from enum import Enum
from PyQt6.QtGui import QPixmap


class PluginType(Enum):
    """Types of plugins supported by NC-KTV"""
    EFFECT = "effect"
    EXPORT_TEMPLATE = "export_template"
    UI_EXTENSION = "ui_extension"


class PluginPermission(Enum):
    """Permissions that plugins can request"""
    FILE_READ = "file_read"
    FILE_WRITE = "file_write"
    GPU = "gpu"
    NETWORK = "network"
    RENDER = "render"


@dataclass
class PluginMetadata:
    """Plugin metadata from manifest"""
    id: str
    name: str
    version: str
    author: str
    description: str
    plugin_type: PluginType
    permissions: List[PluginPermission]
    dependencies: Dict[str, str]
    entry_point: str
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'PluginMetadata':
        """Create metadata from manifest dictionary"""
        return cls(
            id=data['id'],
            name=data['name'],
            version=data['version'],
            author=data['author'],
            description=data.get('description', ''),
            plugin_type=PluginType(data['type']),
            permissions=[PluginPermission(p) for p in data.get('permissions', [])],
            dependencies=data.get('dependencies', {}),
            entry_point=data.get('entry_point', 'main.py')
        )


@dataclass
class Parameter:
    """Plugin parameter definition"""
    name: str
    display_name: str
    param_type: str  # 'float', 'int', 'bool', 'color', 'string', 'choice'
    default_value: Any
    min_value: Optional[float] = None
    max_value: Optional[float] = None
    choices: Optional[List[str]] = None
    description: Optional[str] = None


class Plugin(ABC):
    """Base class for all plugins"""
    
    def __init__(self, metadata: PluginMetadata):
        self.metadata = metadata
        self._enabled = False
    
    @abstractmethod
    def initialize(self, api) -> bool:
        """
        Initialize the plugin with NC-KTV API access
        
        Args:
            api: NC-KTV plugin API instance
            
        Returns:
            True if initialization successful, False otherwise
        """
        pass
    
    @abstractmethod
    def cleanup(self):
        """Cleanup resources when plugin is unloaded"""
        pass
    
    def enable(self):
        """Enable the plugin"""
        self._enabled = True
    
    def disable(self):
        """Disable the plugin"""
        self._enabled = False
    
    @property
    def enabled(self) -> bool:
        """Check if plugin is enabled"""
        return self._enabled


# ============================================================================
# EFFECT PLUGIN
# ============================================================================

@dataclass
class RenderContext:
    """Context information for effect rendering"""
    time: float  # Current time in seconds
    duration: float  # Total duration
    width: int
    height: int
    fps: float
    project_data: Dict[str, Any]


class EffectPlugin(Plugin):
    """Base class for custom effect plugins"""
    
    @abstractmethod
    def get_name(self) -> str:
        """Get display name of the effect"""
        pass
    
    @abstractmethod
    def get_parameters(self) -> List[Parameter]:
        """Get list of configurable parameters"""
        pass
    
    @abstractmethod
    def render_frame(self, context: RenderContext, params: Dict[str, Any]) -> Any:
        """
        Render a single frame of the effect
        
        Args:
            context: Rendering context with time, dimensions, etc.
            params: Parameter values set by user
            
        Returns:
            Frame data (format depends on integration)
        """
        pass
    
    def get_preview_thumbnail(self) -> Optional[QPixmap]:
        """
        Get preview thumbnail for effect browser
        
        Returns:
            QPixmap thumbnail or None for auto-generated
        """
        return None
    
    def supports_gpu(self) -> bool:
        """Check if effect can use GPU acceleration"""
        return False


# ============================================================================
# EXPORT TEMPLATE PLUGIN
# ============================================================================

@dataclass
class ExportContext:
    """Context for export template rendering"""
    lyrics_data: Any  # LyricsData object
    output_path: str
    video_width: int
    video_height: int
    fps: float
    audio_track: str
    video_track: Optional[str]
    background_color: Optional[str]


class ExportTemplatePlugin(Plugin):
    """Base class for custom export template plugins"""
    
    @abstractmethod
    def get_template_name(self) -> str:
        """Get display name of the template"""
        pass
    
    @abstractmethod
    def get_description(self) -> str:
        """Get description of what this template does"""
        pass
    
    @abstractmethod
    def get_parameters(self) -> List[Parameter]:
        """Get list of configurable parameters for this template"""
        pass
    
    @abstractmethod
    def generate_subtitle_file(self, context: ExportContext, params: Dict[str, Any]) -> str:
        """
        Generate subtitle file (ASS, SRT, etc.)
        
        Args:
            context: Export context with lyrics, dimensions, etc.
            params: User-configured parameter values
            
        Returns:
            Path to generated subtitle file
        """
        pass
    
    def get_supported_formats(self) -> List[str]:
        """
        Get list of supported output formats
        
        Returns:
            List of format extensions (e.g., ['ass', 'srt'])
        """
        return ['ass']
    
    def get_preview(self, params: Dict[str, Any]) -> Optional[QPixmap]:
        """
        Generate preview of the template styling
        
        Args:
            params: Current parameter values
            
        Returns:
            Preview image or None
        """
        return None


# ============================================================================
# UI EXTENSION PLUGIN
# ============================================================================

class UIExtensionPlugin(Plugin):
    """Base class for UI extension plugins"""
    
    @abstractmethod
    def get_widget_name(self) -> str:
        """Get name of the UI widget/extension"""
        pass
    
    @abstractmethod
    def create_widget(self, parent):
        """
        Create the UI widget
        
        Args:
            parent: Parent Qt widget
            
        Returns:
            QWidget instance
        """
        pass
    
    @abstractmethod
    def get_menu_location(self) -> str:
        """
        Get menu location for the widget
        
        Returns:
            Menu path (e.g., "Tools/My Extension")
        """
        pass


# ============================================================================
# PLUGIN API
# ============================================================================

class PluginAPI:
    """
    API provided to plugins for accessing NC-KTV functionality
    Provides safe, controlled access to application features
    """
    
    def __init__(self, config, project_manager):
        self.config = config
        self.project_manager = project_manager
    
    def get_config_value(self, key: str, default=None) -> Any:
        """Get configuration value"""
        return self.config.get(key, default)
    
    def set_config_value(self, key: str, value: Any):
        """Set configuration value (plugin namespace only)"""
        # Restrict to plugin namespace to prevent plugins from breaking core config
        plugin_key = f"plugins.{key}"
        self.config.set(plugin_key, value)
    
    def get_current_project(self):
        """Get current active project"""
        return self.project_manager.current_project
    
    def log(self, message: str, level: str = "info"):
        """Log message to application log"""
        import logging
        logger = logging.getLogger('PluginAPI')
        getattr(logger, level)(message)
    
    def show_message(self, title: str, message: str, msg_type: str = "info"):
        """Show message dialog to user"""
        from PyQt6.QtWidgets import QMessageBox
        if msg_type == "info":
            QMessageBox.information(None, title, message)
        elif msg_type == "warning":
            QMessageBox.warning(None, title, message)
        elif msg_type == "error":
            QMessageBox.critical(None, title, message)
