"""
Configuration management for NC-KTV
Handles loading and saving application settings
"""

import yaml
from pathlib import Path
from typing import Any, Dict


class Config:
    """Application configuration manager"""
    
    DEFAULT_CONFIG_PATH = Path("config.yaml")
    
    def __init__(self, config_path: Path = None):
        """Initialize configuration
        
        Args:
            config_path: Path to config file (defaults to config.yaml)
        """
        self.config_path = config_path or self.DEFAULT_CONFIG_PATH
        self.settings: Dict[str, Any] = {}
        self.load()
    
    def load(self):
        """Load configuration from file"""
        if self.config_path.exists():
            with open(self.config_path, 'r', encoding='utf-8') as f:
                self.settings = yaml.safe_load(f) or {}
        else:
            # Use default settings
            self.settings = self._get_defaults()
            self.save()
    
    def save(self):
        """Save configuration to file"""
        self.config_path.parent.mkdir(parents=True, exist_ok=True)
        with open(self.config_path, 'w', encoding='utf-8') as f:
            yaml.dump(self.settings, f, default_flow_style=False)
    
    def get(self, key: str, default=None) -> Any:
        """Get configuration value
        
        Args:
            key: Configuration key (supports dot notation, e.g. 'uvr.use_gpu')
            default: Default value if key not found
        
        Returns:
            Configuration value
        """
        keys = key.split('.')
        value = self.settings
        
        for k in keys:
            if isinstance(value, dict) and k in value:
                value = value[k]
            else:
                return default
        
        return value
        
    def get_available_models(self, model_type: str = 'whisper'):
        """Scan for available models in configured directories"""
        import os
        models = []
        
        # Determine scan path
        if model_type == 'whisper':
           base_path = Path("models/whisper")
           exts = ['.pt', '.bin']
           
           if base_path.exists():
                # 1. Standard .pt files
                for f in base_path.glob('*.pt'):
                    models.append(f.name)
                
                # 2. Faster Whisper directories (recursive search for model.bin)
                # This finds folders like "models--Systran--faster-whisper-large-v3"
                for p in base_path.rglob('model.bin'):
                    # We store the relative path string
                    try:
                        rel_path = p.parent.relative_to(base_path)
                        # We prepend a prefix or just format it so we know it's a path
                        # Actually, storing the full relative path string is safer
                        models.append(str(p.parent))
                    except ValueError:
                        models.append(str(p.parent))

        elif model_type == 'uvr':
           base_path = Path("models")
           exts = ['.pth', '.onnx']
           if base_path.exists():
                for f in base_path.glob('*'):
                    if f.suffix in exts:
                        models.append(f.name)
        
        return sorted(list(set(models)))
    
    def set(self, key: str, value: Any):
        """Set configuration value
        
        Args:
            key: Configuration key (supports dot notation)
            value: Value to set
        """
        keys = key.split('.')
        config = self.settings
        
        for k in keys[:-1]:
            if k not in config or not isinstance(config[k], dict):
                config[k] = {}
            config = config[k]
        
        config[keys[-1]] = value
    
    def _get_defaults(self) -> Dict[str, Any]:
        """Get default configuration"""
        return {
            'uvr': {
                'models_path': 'models/uvr',
                'default_model': 'MDX_Net',
                'use_gpu': True,
                'gpu_device': 0,
                'batch_size': 4
            },
            'processing': {
                'temp_dir': 'temp/',
                'output_dir': 'output/',
                'audio_format': 'wav',
                'sample_rate': 44100,
                'num_workers': 4
            },
            'lyrics': {
                'whisper_model': 'base',
                'language': 'auto',
                'word_level_timing': True,
                'confidence_threshold': 0.5
            },
            'karaoke': {
                'default_style': 'Bouncing Ball',
                'font_family': 'Arial',
                'font_size': 48,
                'primary_color': '#FFFFFF',
                'highlight_color': '#FFD700',
                'outline_color': '#000000',
                'outline_width': 2,
                'position': 'bottom'
            },
            'export': {
                'video_codec': 'libx264',
                'video_quality': 'high',
                'audio_bitrate': '320k',
                'output_format': 'mp4'
            },
            'gui': {
                'theme': 'builtin-dark',
                'start_mode': 'editor',
                'show_tooltips': True,
                'autosave': True,
                'autosave_interval': 300
            },
            'plugins': {
                'enabled': []  # List of enabled plugin IDs
            },
            'advanced': {
                'enable_batch_processing': False,
                'max_video_length': 3600,
                'cache_waveforms': True,
                'cache_size_mb': 1024
            }
        }
