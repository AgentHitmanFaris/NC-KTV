"""
Project data structure for NC-KTV
Handles project metadata, settings, and file management
"""

import json
from pathlib import Path
from typing import Optional, Dict, Any
from dataclasses import dataclass, asdict
from datetime import datetime

from sync.sync_data import LyricsData


@dataclass
class ProjectSettings:
    """Project-specific settings"""
    uvr_model: str = "UVR_MDXNET_KARA_2.onnx"
    use_gpu: bool = True
    sample_rate: int = 44100
    karaoke_style: str = "classic"
    output_format: str = "mp4"


class Project:
    """NC-KTV Project management"""
    
    PROJECT_VERSION = "1.0"
    PROJECT_EXT = ".nctv"
    
    def __init__(self, source_file: Optional[Path] = None):
        """Initialize project
        
        Args:
            source_file: Source audio/video file
        """
        self.source_file = Path(source_file) if source_file else None
        self.project_name = self.source_file.stem if source_file else "Untitled"
        self.settings = ProjectSettings()
        self.lyrics = LyricsData()
        
        # File paths
        self.audio_file: Optional[Path] = None
        self.instrumental_file: Optional[Path] = None
        self.vocals_file: Optional[Path] = None
        self.output_video: Optional[Path] = None
        
        # Metadata
        self.created_date = datetime.now().isoformat()
        self.modified_date = datetime.now().isoformat()
        self.project_file: Optional[Path] = None
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert project to dictionary for serialization
        
        Returns:
            Dictionary representation of project
        """
        return {
            'version': self.PROJECT_VERSION,
            'project_name': self.project_name,
            'source_file': str(self.source_file) if self.source_file else None,
            'settings': asdict(self.settings),
            'lyrics': self.lyrics.to_dict(),
            'files': {
                'audio': str(self.audio_file) if self.audio_file else None,
                'instrumental': str(self.instrumental_file) if self.instrumental_file else None,
                'vocals': str(self.vocals_file) if self.vocals_file else None,
                'output_video': str(self.output_video) if self.output_video else None
            },
            'metadata': {
                'created': self.created_date,
                'modified': self.modified_date
            }
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Project':
        """Create project from dictionary
        
        Args:
            data: Dictionary with project data
        
        Returns:
            Project instance
        """
        # Create project
        source_file = data.get('source_file')
        project = cls(source_file=source_file if source_file else None)
        
        # Load settings
        if 'settings' in data:
            settings_data = data['settings']
            project.settings = ProjectSettings(**settings_data)
        
        # Load lyrics
        if 'lyrics' in data:
            project.lyrics = LyricsData.from_dict(data['lyrics'])
        
        # Load file paths
        files = data.get('files', {})
        project.audio_file = Path(files['audio']) if files.get('audio') else None
        project.instrumental_file = Path(files['instrumental']) if files.get('instrumental') else None
        project.vocals_file = Path(files['vocals']) if files.get('vocals') else None
        project.output_video = Path(files['output_video']) if files.get('output_video') else None
        
        # Metadata
        metadata = data.get('metadata', {})
        project.created_date = metadata.get('created', project.created_date)
        project.modified_date = metadata.get('modified', project.modified_date)
        project.project_name = data.get('project_name', project.project_name)
        
        return project
    
    def save(self, file_path: Path):
        """Save project to file
        
        Args:
            file_path: Path to save project file
        """
        file_path = Path(file_path)
        
        # Ensure extension
        if file_path.suffix != self.PROJECT_EXT:
            file_path = file_path.with_suffix(self.PROJECT_EXT)
        
        # Update modified date
        self.modified_date = datetime.now().isoformat()
        self.project_file = file_path
        
        # Create parent directory
        file_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Save as JSON
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
    
    @classmethod
    def load(cls, file_path: Path) -> 'Project':
        """Load project from file
        
        Args:
            file_path: Path to project file
        
        Returns:
            Loaded project
        
        Raises:
            FileNotFoundError: If file doesn't exist
            ValueError: If file format is invalid
        """
        file_path = Path(file_path)
        
        if not file_path.exists():
            raise FileNotFoundError(f"Project file not found: {file_path}")
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            project = cls.from_dict(data)
            project.project_file = file_path
            
            return project
            
        except json.JSONDecodeError as e:
            raise ValueError(f"Invalid project file format: {e}")
    
    def get_temp_dir(self) -> Path:
        """Get temporary directory for this project
        
        Returns:
            Path to temp directory
        """
        temp_dir = Path("temp") / self.project_name
        temp_dir.mkdir(parents=True, exist_ok=True)
        return temp_dir
    
    def cleanup_temp_files(self):
        """Remove temporary files"""
        temp_dir = self.get_temp_dir()
        
        if temp_dir.exists():
            import shutil
            shutil.rmtree(temp_dir)
    
    @property
    def name(self) -> str:
        """Alias for project_name for compatibility"""
        return self.project_name
        
    def validate(self) -> tuple[bool, str]:
        """Validate project state
        
        Returns:
            Tuple of (is_valid, error_message)
        """
        if not self.source_file:
            return False, "No source file specified"
        
        if not self.source_file.exists():
            return False, f"Source file not found: {self.source_file}"
        
        return True, "OK"
