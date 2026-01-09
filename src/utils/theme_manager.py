"""
Theme Manager for NC-KTV
Handles theme loading, parsing, and application to UI and karaoke output
"""

import yaml
import logging
from pathlib import Path
from typing import Dict, Any, Optional, List
from dataclasses import dataclass
from PyQt6.QtGui import QColor
from zipfile import ZipFile
import tempfile
import shutil

logger = logging.getLogger(__name__)


@dataclass
class ThemeMetadata:
    """Theme metadata"""
    id: str
    name: str
    author: str
    version: str
    description: str = ""


@dataclass
class ThemeColors:
    """Theme color palette"""
    background: str
    foreground: str
    primary: str
    accent: str
    success: str
    warning: str
    error: str
    
    def to_dict(self) -> Dict[str, str]:
        """Convert to dictionary"""
        return {
            'background': self.background,
            'foreground': self.foreground,
            'primary': self.primary,
            'accent': self.accent,
            'success': self.success,
            'warning': self.warning,
            'error': self.error
        }


@dataclass
class KaraokeStyle:
    """Karaoke video style definition"""
    name: str
    font_family: str
    font_size: int
    active_color: str
    inactive_color: str
    outline_color: str
    outline_width: float
    animation: str = "linear_wipe"


class Theme:
    """Represents a loaded theme"""
    
    def __init__(self, metadata: ThemeMetadata, ui_config: Dict[str, Any], 
                 karaoke_styles: List[KaraokeStyle]):
        self.metadata = metadata
        self.ui_config = ui_config
        self.karaoke_styles = karaoke_styles
        self.colors = self._parse_colors(ui_config.get('colors', {}))
        self.fonts = ui_config.get('fonts', {})
        self.styles = ui_config.get('styles', {})
    
    def _parse_colors(self, colors_dict: Dict[str, str]) -> ThemeColors:
        """Parse colors from config"""
        return ThemeColors(
            background=colors_dict.get('background', '#1a1a1a'),
            foreground=colors_dict.get('foreground', '#ffffff'),
            primary=colors_dict.get('primary', '#2196F3'),
            accent=colors_dict.get('accent', '#FF9800'),
            success=colors_dict.get('success', '#4CAF50'),
            warning=colors_dict.get('warning', '#FF9800'),
            error=colors_dict.get('error', '#F44336')
        )
    
    def get_color(self, color_name: str) -> str:
        """Get color by name"""
        return getattr(self.colors, color_name, '#000000')
    
    def get_style(self, style_name: str) -> str:
        """Get stylesheet for a named element"""
        style_template = self.styles.get(style_name, '')
        # Replace color placeholders
        for color_name, color_value in self.colors.to_dict().items():
            style_template = style_template.replace(f'{{colors.{color_name}}}', color_value)
        return style_template
    
    def get_karaoke_style(self, style_name: str) -> Optional[KaraokeStyle]:
        """Get karaoke style by name"""
        for style in self.karaoke_styles:
            if style.name == style_name:
                return style
        return None


class ThemeManager:
    """Manages theme loading and application"""
    
    THEMES_DIR = Path("themes")
    INSTALLED_DIR = THEMES_DIR / "installed"
    BUILTIN_DIR = THEMES_DIR
    
    def __init__(self, config):
        """
        Initialize theme manager
        
        Args:
            config: Application configuration
        """
        self.config = config
        self.themes: Dict[str, Theme] = {}
        self.active_theme: Optional[Theme] = None
        
        # Ensure theme directories exist
        self.THEMES_DIR.mkdir(exist_ok=True)
        self.INSTALLED_DIR.mkdir(exist_ok=True)
        
        # Create built-in themes if they don't exist
        self._create_builtin_themes()
        
        logger.info("Theme manager initialized")
    
    def _create_builtin_themes(self):
        """Create default built-in themes"""
        # Dark theme
        dark_theme_path = self.BUILTIN_DIR / "dark.yaml"
        if not dark_theme_path.exists():
            dark_theme = {
                'metadata': {
                    'id': 'builtin-dark',
                    'name': 'Dark',
                    'author': 'NC-KTV Team',
                    'version': '1.0.0',
                    'description': 'Default dark theme'
                },
                'ui': {
                    'colors': {
                        'background': '#1a1a1a',
                        'foreground': '#ffffff',
                        'primary': '#2196F3',
                        'accent': '#FF9800',
                        'success': '#4CAF50',
                        'warning': '#FF9800',
                        'error': '#F44336'
                    },
                    'fonts': {
                        'family': 'Arial',
                        'size': 10
                    },
                    'styles': {
                        'button_primary': 'background-color: {colors.success}; color: white; font-weight: bold;',
                        'button_export': 'background-color: #E91E63; color: white; font-weight: bold;',
                        'button_auto': 'background-color: #673AB7; color: white; font-weight: bold;'
                    }
                },
                'karaoke': {
                    'styles': [
                        {
                            'name': 'Neon Gold',
                            'font_family': 'Arial',
                            'font_size': 60,
                            'active_color': '#FFD700',
                            'inactive_color': '#FFFFFF',
                            'outline_color': '#FFD700',
                            'outline_width': 2.0,
                            'animation': 'linear_wipe'
                        }
                    ]
                }
            }
            with open(dark_theme_path, 'w', encoding='utf-8') as f:
                yaml.dump(dark_theme, f, default_flow_style=False)
        
        # Light theme
        light_theme_path = self.BUILTIN_DIR / "light.yaml"
        if not light_theme_path.exists():
            light_theme = {
                'metadata': {
                    'id': 'builtin-light',
                    'name': 'Light',
                    'author': 'NC-KTV Team',
                    'version': '1.0.0',
                    'description': 'Light theme for bright environments'
                },
                'ui': {
                    'colors': {
                        'background': '#f5f5f5',
                        'foreground': '#212121',
                        'primary': '#1976D2',
                        'accent': '#FFA000',
                        'success': '#388E3C',
                        'warning': '#F57C00',
                        'error': '#D32F2F'
                    },
                    'fonts': {
                        'family': 'Arial',
                        'size': 10
                    },
                    'styles': {
                        'button_primary': 'background-color: {colors.success}; color: white; font-weight: bold;',
                        'button_export': 'background-color: #C2185B; color: white; font-weight: bold;',
                        'button_auto': 'background-color: #512DA8; color: white; font-weight: bold;'
                    }
                },
                'karaoke': {
                    'styles': [
                        {
                            'name': 'Classic Blue',
                            'font_family': 'Arial',
                            'font_size': 60,
                            'active_color': '#0000FF',
                            'inactive_color': '#FFFFFF',
                            'outline_color': '#400000',
                            'outline_width': 2.0,
                            'animation': 'linear_wipe'
                        }
                    ]
                }
            }
            with open(light_theme_path, 'w', encoding='utf-8') as f:
                yaml.dump(light_theme, f, default_flow_style=False)
    
    def discover_themes(self) -> List[ThemeMetadata]:
        """
        Scan theme directories and discover available themes
        
        Returns:
            List of theme metadata
        """
        discovered = []
        
        # Scan both builtin and installed directories
        for theme_dir in [self.BUILTIN_DIR, self.INSTALLED_DIR]:
            if not theme_dir.exists():
                continue
            
            for theme_file in theme_dir.glob('*.yaml'):
                try:
                    with open(theme_file, 'r', encoding='utf-8') as f:
                        data = yaml.safe_load(f)
                        if data and 'metadata' in data:
                            metadata = ThemeMetadata(**data['metadata'])
                            discovered.append(metadata)
                            logger.info(f"Discovered theme: {metadata.name} ({metadata.id})")
                except Exception as e:
                    logger.error(f"Failed to load theme from {theme_file}: {e}")
        
        return discovered
    
    def load_theme(self, theme_id: str) -> bool:
        """
        Load a theme by ID
        
        Args:
            theme_id: Theme identifier
            
        Returns:
            True if loaded successfully
        """
        try:
            # Find theme file
            theme_file = None
            for theme_dir in [self.BUILTIN_DIR, self.INSTALLED_DIR]:
                for candidate in theme_dir.glob('*.yaml'):
                    with open(candidate, 'r', encoding='utf-8') as f:
                        data = yaml.safe_load(f)
                        if data and data.get('metadata', {}).get('id') == theme_id:
                            theme_file = candidate
                            break
                if theme_file:
                    break
            
            if not theme_file:
                logger.error(f"Theme not found: {theme_id}")
                return False
            
            # Load theme
            with open(theme_file, 'r', encoding='utf-8') as f:
                data = yaml.safe_load(f)
            
            metadata = ThemeMetadata(**data['metadata'])
            ui_config = data.get('ui', {})
            
            # Parse karaoke styles
            karaoke_styles = []
            for style_data in data.get('karaoke', {}).get('styles', []):
                karaoke_styles.append(KaraokeStyle(**style_data))
            
            theme = Theme(metadata, ui_config, karaoke_styles)
            self.themes[theme_id] = theme
            
            logger.info(f"Loaded theme: {metadata.name} ({theme_id})")
            return True
            
        except Exception as e:
            logger.error(f"Failed to load theme {theme_id}: {e}")
            return False
    
    def set_active_theme(self, theme_id: str) -> bool:
        """
        Set the active theme
        
        Args:
            theme_id: Theme identifier
            
        Returns:
            True if set successfully
        """
        # Load if not already loaded
        if theme_id not in self.themes:
            if not self.load_theme(theme_id):
                return False
        
        self.active_theme = self.themes[theme_id]
        
        # Save to config
        self.config.set('gui.theme', theme_id)
        self.config.save()
        
        logger.info(f"Active theme set to: {theme_id}")
        return True
    
    def get_active_theme(self) -> Optional[Theme]:
        """Get current active theme"""
        return self.active_theme
    
    def get_color(self, color_name: str, default: str = '#000000') -> str:
        """Get color from active theme"""
        if self.active_theme:
            return self.active_theme.get_color(color_name)
        return default
    
    def get_style(self, style_name: str, default: str = '') -> str:
        """Get stylesheet from active theme"""
        if self.active_theme:
            return self.active_theme.get_style(style_name)
        return default
    
    def get_karaoke_styles(self) -> List[KaraokeStyle]:
        """Get all karaoke styles from active theme"""
        if self.active_theme:
            return self.active_theme.karaoke_styles
        return []
    
    def get_all_themes(self) -> Dict[str, Theme]:
        """Get all loaded themes"""
        return self.themes.copy()
    
    def install_theme_package(self, package_path: Path) -> bool:
        """
        Install theme from .ncktheme package
        
        Args:
            package_path: Path to .ncktheme file
            
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
                
                # Find theme.yaml
                theme_file = temp_path / "theme.yaml"
                if not theme_file.exists():
                    raise ValueError("Package missing theme.yaml")
                
                # Load and validate
                with open(theme_file, 'r', encoding='utf-8') as f:
                    data = yaml.safe_load(f)
                    if not data or 'metadata' not in data:
                        raise ValueError("Invalid theme.yaml format")
                    
                    metadata = ThemeMetadata(**data['metadata'])
                
                # Copy to installed directory
                target_file = self.INSTALLED_DIR / f"{metadata.id}.yaml"
                shutil.copy(theme_file, target_file)
                
                # Copy any additional assets
                assets_dir = temp_path / "assets"
                if assets_dir.exists():
                    target_assets = self.INSTALLED_DIR / metadata.id / "assets"
                    target_assets.parent.mkdir(exist_ok=True)
                    shutil.copytree(assets_dir, target_assets, dirs_exist_ok=True)
                
                logger.info(f"Installed theme: {metadata.name} ({metadata.id})")
                return True
                
        except Exception as e:
            logger.error(f"Failed to install theme package: {e}")
            return False
    
    def uninstall_theme(self, theme_id: str) -> bool:
        """
        Uninstall a theme
        
        Args:
            theme_id: Theme identifier
            
        Returns:
            True if uninstalled successfully
        """
        # Remove from loaded themes
        if theme_id in self.themes:
            del self.themes[theme_id]
        
        # Remove file
        theme_file = self.INSTALLED_DIR / f"{theme_id}.yaml"
        if theme_file.exists():
            try:
                theme_file.unlink()
                logger.info(f"Uninstalled theme: {theme_id}")
                return True
            except Exception as e:
                logger.error(f"Failed to uninstall theme {theme_id}: {e}")
                return False
        else:
            logger.warning(f"Theme file not found: {theme_file}")
            return False
    
    def initialize_default_theme(self):
        """Load and set default theme on startup"""
        # Get theme from config or use default
        theme_id = self.config.get('gui.theme', 'builtin-dark')
        
        # Load all available themes first
        discovered = self.discover_themes()
        for metadata in discovered:
            self.load_theme(metadata.id)
        
        # Set active theme
        if theme_id in self.themes:
            self.active_theme = self.themes[theme_id]
        elif 'builtin-dark' in self.themes:
            self.active_theme = self.themes['builtin-dark']
        elif self.themes:
            # Fallback to first available theme
            self.active_theme = list(self.themes.values())[0]
        
        if self.active_theme:
            logger.info(f"Initialized with theme: {self.active_theme.metadata.name}")
