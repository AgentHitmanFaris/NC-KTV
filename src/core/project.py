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
from core.timeline_data import TimelineData
from core.audio_clock import AudioClock, validate_sample_rates


@dataclass
class ProjectSettings:
    """Project-specific settings"""
    uvr_model: str = "UVR_MDXNET_KARA_2.onnx"
    use_gpu: bool = True
    sample_rate: int = 44100
    karaoke_style: str = "classic"
    output_format: str = "mp4"

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'ProjectSettings':
        """Create settings processing only known fields"""
        # Filter out unknown keys (like 'auto_transcribe' from older versions)
        known_keys = cls.__annotations__.keys()
        filtered_data = {k: v for k, v in data.items() if k in known_keys}
        return cls(**filtered_data)


class Project:
    """NC-KTV Project management"""
    
    PROJECT_VERSION = "0.7"  # Bumped for timeline support
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
        
        # Timeline data (Phase 6)
        self.timeline = TimelineData()
        
        # Audio clock for timing sync (Phase 6.3 - Timing Fix)
        self.audio_clock = AudioClock()
        self.timing_metadata = {
            'sample_rate_validated': False,
            'transcription_source': 'original',  # 'original' or 'vocals'
            'uvr_delay_measured': 0.0,
            'global_offset': 0.0
        }
        
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
            'timeline': self.timeline.to_dict(),  # Phase 6: Timeline data
            'audio_clock': self.audio_clock.to_dict(),  # Phase 6.3: Timing sync
            'timing_metadata': self.timing_metadata,
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
            project.settings = ProjectSettings.from_dict(settings_data)
        
        # Load lyrics
        if 'lyrics' in data:
            project.lyrics = LyricsData.from_dict(data['lyrics'])
        
        # Load timeline (Phase 6) - backward compatible with v0.6
        if 'timeline' in data:
            project.timeline = TimelineData.from_dict(data['timeline'])
        else:
            # Legacy projects without timeline - create empty timeline
            project.timeline = TimelineData()
        
        # Load audio clock (Phase 6.3) - backward compatible
        if 'audio_clock' in data:
            project.audio_clock = AudioClock.from_dict(data['audio_clock'])
        else:
            project.audio_clock = AudioClock()
        
        # Load timing metadata
        project.timing_metadata = data.get('timing_metadata', {
            'sample_rate_validated': False,
            'transcription_source': 'original',
            'uvr_delay_measured': 0.0,
            'global_offset': 0.0
        })
        
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
        """Save project to encrypted .nctv file
        
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
        
        # Save using encrypted binary format
        from core.nctv_format import NCTVFormat
        NCTVFormat.pack(self, file_path)
    
    @classmethod
    def load(cls, file_path: Path) -> 'Project':
        """Load project from file (supports both .nctv binary and legacy JSON)
        
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
        
        # Try new binary format first
        try:
            # Check for NCTV magic bytes
            with open(file_path, 'rb') as f:
                magic = f.read(4)
            
            if magic == b'NCTV':
                # New encrypted binary format
                from core.nctv_format import NCTVFormat
                project = NCTVFormat.unpack(file_path)
                project.project_file = file_path
                return project
        except Exception as e:
            # Only fallback if it wasn't an NCTV file
            # If it WAS an NCTV file (magic match) but failed to unpack, we should raise the error
            # so the user sees the real reason (e.g. corruption, password), not a JSON decode error.
            if 'magic' in locals() and magic == b'NCTV':
                raise ValueError(f"Failed to load NCTV project: {e}") from e
                
            # If binary format detection fails (not NCTV), try JSON fallback
            import logging
            logging.getLogger(__name__).debug(f"Binary format failed, trying JSON: {e}")
        
        # Fallback: Legacy JSON format
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
    
    def validate_sample_rates_sync(self) -> Dict:
        """Validate sample rates across all audio files"""
        audio_files = {
            'source': self.source_file,
            'instrumental': self.instrumental_file,
            'vocals': self.vocals_file
        }
        
        results = validate_sample_rates(audio_files)
        
        if results['valid']:
            # Update audio clock sample rate
            if results['recommended_rate']:
                self.audio_clock.set_sample_rate(results['recommended_rate'])
                self.timing_metadata['sample_rate_validated'] = True
        
        return results
