"""
Audio Clock - Sample-accurate timing reference for NC-KTV
Prevents temporal drift between processing stages
"""

import logging
from pathlib import Path
from typing import Dict, Optional

logger = logging.getLogger(__name__)


class AudioClock:
    """
    Sample-accurate master clock for all audio processing.
    Ensures consistent timestamps across UVR separation, transcription, and playback.
    """
    
    def __init__(self, sample_rate: int = 48000):
        self.sample_rate = sample_rate
        self.master_offset = 0.0  # Global calibration offset in seconds
        
        # Known latency corrections per stage (in seconds)
        self.latency_corrections = {
            'uvr': 0.0,           # UVR is the reference point
            'transcription': 0.0,  # Will be measured if using separated vocals
            'playback': 0.023,     # Qt QMediaPlayer typical latency on Windows
            'preview': 0.008,      # Preview rendering ~1 frame at 60fps
        }
        
        logger.info(f"AudioClock initialized: sample_rate={sample_rate}Hz, offset={self.master_offset}s")
    
    def set_master_offset(self, offset: float):
        """Set the global timing offset (user calibration)"""
        self.master_offset = offset
        logger.info(f"Master offset updated: {offset:.3f}s")
    
    def set_sample_rate(self, sample_rate: int):
        """Update the reference sample rate"""
        self.sample_rate = sample_rate
        logger.info(f"Sample rate updated: {sample_rate}Hz")
    
    def samples_to_seconds(self, samples: int) -> float:
        """Convert sample count to seconds with offset correction"""
        return (samples / self.sample_rate) + self.master_offset
    
    def seconds_to_samples(self, seconds: float) -> int:
        """Convert seconds to sample count (removing offset)"""
        return int((seconds - self.master_offset) * self.sample_rate)
    
    def sync_point(self, timestamp: float, source: str) -> float:
        """
        Apply sync correction for a timestamp from a specific source.
        
        Args:
            timestamp: Raw timestamp in seconds
            source: Processing stage ('uvr', 'transcription', 'playback', 'preview')
        
        Returns:
            Corrected timestamp aligned to master clock
        """
        correction = self.latency_corrections.get(source, 0.0)
        corrected = timestamp + correction + self.master_offset
        
        logger.debug(f"Sync point: {timestamp:.3f}s ({source}) -> {corrected:.3f}s (correction: {correction + self.master_offset:.3f}s)")
        return corrected
    
    def set_latency(self, source: str, latency: float):
        """Update latency correction for a specific source"""
        self.latency_corrections[source] = latency
        logger.info(f"Latency for '{source}' set to {latency*1000:.1f}ms")
    
    def get_drift_at_time(self, seconds: float) -> float:
        """
        Calculate accumulated drift at a given time.
        Useful for diagnostics.
        """
        # Drift is zero with perfect sync, non-zero with sample rate mismatch
        return 0.0  # Placeholder - would need actual implementation
    
    def to_dict(self) -> Dict:
        """Serialize clock settings for project save"""
        return {
            'sample_rate': self.sample_rate,
            'master_offset': self.master_offset,
            'latency_corrections': self.latency_corrections.copy()
        }
    
    @classmethod
    def from_dict(cls, data: Dict) -> 'AudioClock':
        """Deserialize clock settings from project load"""
        clock = cls(sample_rate=data.get('sample_rate', 48000))
        clock.master_offset = data.get('master_offset', 0.0)
        clock.latency_corrections.update(data.get('latency_corrections', {}))
        return clock


def validate_sample_rates(audio_files: Dict[str, Optional[Path]]) -> Dict:
    """
    Validate that all audio files have matching sample rates.
    
    Args:
        audio_files: Dict of {'name': Path} for audio files to check
    
    Returns:
        Dict with validation results
    """
    import soundfile as sf
    
    results = {
        'valid': True,
        'sample_rates': {},
        'mismatches': [],
        'recommended_rate': None
    }
    
    sample_rates = []
    
    for name, path in audio_files.items():
        if not path or not Path(path).exists():
            continue
        
        try:
            info = sf.info(str(path))
            sr = info.samplerate
            results['sample_rates'][name] = sr
            sample_rates.append(sr)
            logger.info(f"{name}: {sr}Hz")
        except Exception as e:
            logger.error(f"Failed to read {name}: {e}")
            results['sample_rates'][name] = None
    
    if sample_rates:
        unique_rates = set(sample_rates)
        
        if len(unique_rates) > 1:
            results['valid'] = False
            results['mismatches'] = list(unique_rates)
            # Recommend highest rate
            results['recommended_rate'] = max(sample_rates)
            logger.warning(f"Sample rate mismatch detected! Found: {unique_rates}")
        else:
            results['recommended_rate'] = sample_rates[0]
            logger.info(f"✅ All files match: {results['recommended_rate']}Hz")
    
    return results


def measure_uvr_delay(original_path: Path, separated_path: Path, 
                      duration: float = 5.0) -> float:
    """
    Measure the delay introduced by UVR vocal separation using cross-correlation.
    
    Args:
        original_path: Path to original audio
        separated_path: Path to separated vocal track
        duration: Seconds of audio to analyze (default 5s)
    
    Returns:
        Delay in seconds (positive means separated is delayed)
    """
    try:
        import librosa
        import numpy as np
        
        # Load first few seconds for comparison
        y_orig, sr = librosa.load(original_path, duration=duration, sr=None, mono=True)
        y_sep, sr = librosa.load(separated_path, duration=duration, sr=None, mono=True)
        
        # Ensure same length
        min_len = min(len(y_orig), len(y_sep))
        y_orig = y_orig[:min_len]
        y_sep = y_sep[:min_len]
        
        # Cross-correlation to find delay
        correlation = np.correlate(y_orig, y_sep, mode='full')
        lag_samples = correlation.argmax() - len(y_sep)
        
        # Convert to seconds
        delay = lag_samples / sr
        
        logger.info(f"UVR delay measured: {delay*1000:.2f}ms")
        return delay
        
    except Exception as e:
        logger.error(f"Failed to measure UVR delay: {e}")
        return 0.0
