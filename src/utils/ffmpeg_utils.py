"""
FFmpeg utilities for audio/video processing
"""

import subprocess
import json
from pathlib import Path
from typing import Dict, Optional, List


class FFmpegError(Exception):
    """FFmpeg operation failed"""
    pass


def check_ffmpeg() -> bool:
    """Check if FFmpeg is installed and accessible
    
    Returns:
        True if FFmpeg is available
    """
    try:
        result = subprocess.run(
            ['ffmpeg', '-version'],
            capture_output=True,
            text=True,
            timeout=5
        )
        return result.returncode == 0
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return False


def get_media_info(file_path: Path) -> Dict:
    """Get media file information using ffprobe
    
    Args:
        file_path: Path to media file
    
    Returns:
        Dictionary with media information
    
    Raises:
        FFmpegError: If ffprobe fails
    """
    if not file_path.exists():
        raise FFmpegError(f"File not found: {file_path}")
    
    cmd = [
        'ffprobe',
        '-v', 'quiet',
        '-print_format', 'json',
        '-show_format',
        '-show_streams',
        str(file_path)
    ]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        if result.returncode != 0:
            raise FFmpegError(f"ffprobe failed: {result.stderr}")
        
        return json.loads(result.stdout)
    
    except subprocess.TimeoutExpired:
        raise FFmpegError("ffprobe timeout")
    except json.JSONDecodeError as e:
        raise FFmpegError(f"Failed to parse ffprobe output: {e}")


def extract_audio(
    input_path: Path,
    output_path: Path,
    sample_rate: int = 44100,
    channels: int = 2,
    format: str = 'wav'
) -> bool:
    """Extract audio from video file or convert audio format
    
    Args:
        input_path: Input file path
        output_path: Output audio file path
        sample_rate: Output sample rate
        channels: Number of audio channels
        format: Output format (wav, mp3, etc.)
    
    Returns:
        True if successful
    
    Raises:
        FFmpegError: If extraction fails
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    cmd = [
        'ffmpeg',
        '-i', str(input_path),
        '-vn',  # No video
        '-ar', str(sample_rate),
        '-ac', str(channels),
        '-f', format,
        '-y',  # Overwrite output
        str(output_path)
    ]
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=300  # 5 minutes max
        )
        
        if result.returncode != 0:
            raise FFmpegError(f"Audio extraction failed: {result.stderr}")
        
        return output_path.exists()
    
    except subprocess.TimeoutExpired:
        raise FFmpegError("Audio extraction timeout")


def merge_audio_video(
    video_path: Path,
    audio_path: Path,
    output_path: Path,
    video_codec: str = 'copy',
    audio_codec: str = 'aac',
    audio_bitrate: str = '320k'
) -> bool:
    """Merge audio and video files
    
    Args:
        video_path: Input video file
        audio_path: Input audio file
        output_path: Output video file
        video_codec: Video codec (copy to avoid re-encoding)
        audio_codec: Audio codec
        audio_bitrate: Audio bitrate
    
    Returns:
        True if successful
    
    Raises:
        FFmpegError: If merge fails
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    cmd = [
        'ffmpeg',
        '-i', str(video_path),
        '-i', str(audio_path),
        '-map', '0:v:0',  # Video from first input
        '-map', '1:a:0',  # Audio from second input
        '-c:v', video_codec,
        '-c:a', audio_codec,
        '-b:a', audio_bitrate,
        '-y',  # Overwrite output
        str(output_path)
    ]
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=600  # 10 minutes max
        )
        
        if result.returncode != 0:
            raise FFmpegError(f"Merge failed: {result.stderr}")
        
        return output_path.exists()
    
    except subprocess.TimeoutExpired:
        raise FFmpegError("Merge timeout")


def add_subtitles(
    video_path: Path,
    subtitle_path: Path,
    output_path: Path,
    video_codec: str = 'libx264',
    audio_codec: str = 'copy'
) -> bool:
    """Burn subtitles into video
    
    Args:
        video_path: Input video file
        subtitle_path: Subtitle file (.ass or .srt)
        output_path: Output video file
        video_codec: Video codec
        audio_codec: Audio codec (copy to avoid re-encoding)
    
    Returns:
        True if successful
    
    Raises:
        FFmpegError: If subtitle addition fails
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    # Use subtitles filter to burn in
    subtitle_filter = f"subtitles='{str(subtitle_path).replace('\\', '/')}'"
    
    cmd = [
        'ffmpeg',
        '-i', str(video_path),
        '-vf', subtitle_filter,
        '-c:v', video_codec,
        '-c:a', audio_codec,
        '-y',  # Overwrite output
        str(output_path)
    ]
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=900  # 15 minutes max
        )
        
        if result.returncode != 0:
            raise FFmpegError(f"Subtitle addition failed: {result.stderr}")
        
        return output_path.exists()
    
    except subprocess.TimeoutExpired:
        raise FFmpegError("Subtitle addition timeout")
