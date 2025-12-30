"""
Audio processing module for NC-KTV
Handles audio extraction, conversion, and metadata retrieval
"""

import subprocess
from pathlib import Path
from typing import Dict, Optional, Tuple
import json

import librosa
import soundfile as sf


class AudioProcessor:
    """Process audio files - extract, convert, and analyze"""
    
    def __init__(self, temp_dir: Path = None):
        """Initialize audio processor
        
        Args:
            temp_dir: Directory for temporary files
        """
        self.temp_dir = temp_dir or Path("temp")
        self.temp_dir.mkdir(parents=True, exist_ok=True)
    
    def extract_audio_from_video(self, video_path: Path, output_path: Path = None) -> Path:
        """Extract audio from video file using FFmpeg
        
        Args:
            video_path: Path to video file
            output_path: Optional output path (auto-generated if None)
        
        Returns:
            Path to extracted audio file
        
        Raises:
            RuntimeError: If FFmpeg extraction fails
        """
        video_path = Path(video_path)
        
        if output_path is None:
            output_path = self.temp_dir / f"{video_path.stem}_audio.wav"
        else:
            output_path = Path(output_path)
        
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        # FFmpeg command for audio extraction
        cmd = [
            'ffmpeg',
            '-i', str(video_path),
            '-vn',  # No video
            '-acodec', 'pcm_s16le',  # WAV format
            '-ar', '44100',  # 44.1kHz sample rate
            '-ac', '2',  # Stereo
            '-y',  # Overwrite output
            str(output_path)
        ]
        
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True
            )
            return output_path
        except subprocess.CalledProcessError as e:
            raise RuntimeError(f"FFmpeg audio extraction failed: {e.stderr}")
    
    def convert_audio(
        self,
        input_path: Path,
        output_path: Path,
        sample_rate: int = 44100,
        channels: int = 2
    ) -> Path:
        """Convert audio to specified format
        
        Args:
            input_path: Input audio file
            output_path: Output audio file
            sample_rate: Target sample rate
            channels: Number of channels (1=mono, 2=stereo)
        
        Returns:
            Path to converted audio file
        """
        input_path = Path(input_path)
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Load audio with librosa
        audio, sr = librosa.load(input_path, sr=sample_rate, mono=(channels == 1))
        
        # Save with soundfile
        sf.write(output_path, audio.T if audio.ndim > 1 else audio, sample_rate)
        
        return output_path
    
    def get_audio_info(self, audio_path: Path) -> Dict[str, any]:
        """Get audio file metadata using FFprobe
        
        Args:
            audio_path: Path to audio file
        
        Returns:
            Dictionary with audio metadata
        """
        audio_path = Path(audio_path)
        
        cmd = [
            'ffprobe',
            '-v', 'quiet',
            '-print_format', 'json',
            '-show_format',
            '-show_streams',
            str(audio_path)
        ]
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            data = json.loads(result.stdout)
            
            # Extract relevant info
            format_info = data.get('format', {})
            audio_stream = next(
                (s for s in data.get('streams', []) if s.get('codec_type') == 'audio'),
                {}
            )
            
            return {
                'duration': float(format_info.get('duration', 0)),
                'size_bytes': int(format_info.get('size', 0)),
                'bit_rate': int(format_info.get('bit_rate', 0)),
                'sample_rate': int(audio_stream.get('sample_rate', 0)),
                'channels': int(audio_stream.get('channels', 0)),
                'codec': audio_stream.get('codec_name', 'unknown')
            }
        except (subprocess.CalledProcessError, json.JSONDecodeError, KeyError) as e:
            # Fallback to librosa for basic info
            audio, sr = librosa.load(audio_path, sr=None)
            duration = librosa.get_duration(y=audio, sr=sr)
            
            return {
                'duration': duration,
                'size_bytes': audio_path.stat().st_size,
                'bit_rate': 0,
                'sample_rate': sr,
                'channels': 1 if audio.ndim == 1 else audio.shape[0],
                'codec': 'unknown'
            }
    
    def get_duration(self, audio_path: Path) -> float:
        """Get audio duration in seconds
        
        Args:
            audio_path: Path to audio file
        
        Returns:
            Duration in seconds
        """
        info = self.get_audio_info(audio_path)
        return info['duration']
    
    def validate_audio_file(self, file_path: Path) -> Tuple[bool, str]:
        """Validate if file is a supported audio/video format
        
        Args:
            file_path: Path to file
        
        Returns:
            Tuple of (is_valid, message)
        """
        file_path = Path(file_path)
        
        if not file_path.exists():
            return False, "File does not exist"
        
        # Supported extensions
        audio_exts = {'.mp3', '.wav', '.flac', '.m4a', '.aac', '.ogg', '.wma'}
        video_exts = {'.mp4', '.avi', '.mkv', '.mov', '.wmv', '.flv', '.webm'}
        
        ext = file_path.suffix.lower()
        
        if ext in audio_exts:
            return True, "Valid audio file"
        elif ext in video_exts:
            return True, "Valid video file (audio will be extracted)"
        else:
            return False, f"Unsupported format: {ext}"
