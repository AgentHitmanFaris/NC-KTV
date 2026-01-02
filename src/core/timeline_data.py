"""
Timeline data structures for NC-KTV
Manages multi-track timeline with clips, effects, and animation curves
"""

from dataclasses import dataclass, field
from typing import List, Optional, Dict, Any, Literal
from enum import Enum
import json


class TrackType(Enum):
    """Types of timeline tracks"""
    AUDIO = "audio"
    VIDEO = "video"
    EFFECTS = "effects"
    LYRICS = "lyrics"


class EffectType(Enum):
    """Types of animation effects"""
    FADE_IN = "fade_in"
    FADE_OUT = "fade_out"
    SLIDE_LEFT = "slide_left"
    SLIDE_RIGHT = "slide_right"
    ZOOM_IN = "zoom_in"
    ZOOM_OUT = "zoom_out"
    BLUR = "blur"
    COLOR_SHIFT = "color_shift"
    CUSTOM = "custom"


class EasingCurve(Enum):
    """Animation easing curves"""
    LINEAR = "linear"
    EASE_IN = "ease_in"
    EASE_OUT = "ease_out"
    EASE_IN_OUT = "ease_in_out"
    CUSTOM_BEZIER = "custom_bezier"


@dataclass
class Effect:
    """Represents an animation effect with easing curve"""
    effect_type: EffectType
    start_time: float  # Seconds
    duration: float  # Seconds
    easing: EasingCurve = EasingCurve.LINEAR
    custom_curve_points: Optional[List[tuple]] = None  # Bezier control points [(x, y), ...]
    properties: Dict[str, Any] = field(default_factory=dict)  # Effect-specific properties
    
    @property
    def end_time(self) -> float:
        """Calculate end time of effect"""
        return self.start_time + self.duration
    
    def to_dict(self) -> dict:
        """Convert to dictionary"""
        return {
            'effect_type': self.effect_type.value,
            'start_time': self.start_time,
            'duration': self.duration,
            'easing': self.easing.value,
            'custom_curve_points': self.custom_curve_points,
            'properties': self.properties
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'Effect':
        """Create from dictionary"""
        return cls(
            effect_type=EffectType(data['effect_type']),
            start_time=data['start_time'],
            duration=data['duration'],
            easing=EasingCurve(data.get('easing', 'linear')),
            custom_curve_points=data.get('custom_curve_points'),
            properties=data.get('properties', {})
        )


@dataclass
class Clip:
    """Represents a segment on a timeline track"""
    clip_id: str
    track_id: str
    start_time: float  # Position on timeline (seconds)
    duration: float  # Clip duration (seconds)
    source_file: Optional[str] = None  # Source file path for audio/video clips
    source_start: float = 0.0  # Start position in source file (for trimming)
    source_end: Optional[float] = None  # End position in source file
    effects: List[Effect] = field(default_factory=list)
    properties: Dict[str, Any] = field(default_factory=dict)  # Clip-specific properties
    
    @property
    def end_time(self) -> float:
        """Calculate end time on timeline"""
        return self.start_time + self.duration
    
    def move_to(self, new_start_time: float):
        """Move clip to new position on timeline"""
        if new_start_time < 0:
            new_start_time = 0.0
        self.start_time = new_start_time
    
    def resize(self, new_duration: float, from_start: bool = False):
        """
        Resize clip duration
        
        Args:
            new_duration: New duration in seconds
            from_start: If True, adjust start time instead of end time
        """
        MIN_DURATION = 0.1  # Minimum 100ms
        new_duration = max(MIN_DURATION, new_duration)
        
        if from_start:
            # Resizing from left edge - adjust start time and duration
            time_diff = self.duration - new_duration
            self.start_time += time_diff
            self.source_start += time_diff
        
        self.duration = new_duration
        
        # Update source_end if it exists
        if self.source_end is not None:
            self.source_end = self.source_start + new_duration
    
    def split_at(self, time: float) -> Optional['Clip']:
        """
        Split clip at specified time, returns new clip (right portion)
        
        Args:
            time: Time to split at (must be within clip bounds)
            
        Returns:
            New clip representing the right portion, or None if invalid
        """
        if time <= self.start_time or time >= self.end_time:
            return None
        
        # Calculate durations
        left_duration = time - self.start_time
        right_duration = self.end_time - time
        
        # Create right clip
        import uuid
        right_clip = Clip(
            clip_id=f"{self.clip_id}_split_{uuid.uuid4().hex[:8]}",
            track_id=self.track_id,
            start_time=time,
            duration=right_duration,
            source_file=self.source_file,
            source_start=self.source_start + left_duration,
            source_end=self.source_end,
            properties=self.properties.copy()
        )
        
        # Copy effects that occur in the right portion
        for effect in self.effects:
            effect_time = self.start_time + effect.start_time
            if effect_time >= time:
                # Effect is in right clip
                new_effect = Effect(
                    effect_type=effect.effect_type,
                    start_time=effect.start_time - left_duration,
                    duration=effect.duration,
                    easing=effect.easing,
                    custom_curve_points=effect.custom_curve_points,
                    properties=effect.properties.copy()
                )
                right_clip.effects.append(new_effect)
        
        # Adjust this (left) clip
        self.duration = left_duration
        if self.source_end is not None:
            self.source_end = self.source_start + left_duration
        
        # Remove effects that are now in right clip
        self.effects = [e for e in self.effects 
                       if (self.start_time + e.start_time) < time]
        
        return right_clip
    
    def add_effect(self, effect: Effect):
        """Add an effect to this clip"""
        self.effects.append(effect)
        # Sort effects by start time for consistent rendering
        self.effects.sort(key=lambda e: e.start_time)
    
    def remove_effect(self, effect_index: int) -> bool:
        """
        Remove effect by index
        
        Returns:
            True if effect was removed, False if index invalid
        """
        if 0 <= effect_index < len(self.effects):
            self.effects.pop(effect_index)
            return True
        return False
    
    def get_effect_at_time(self, time: float) -> List[Effect]:
        """Get all effects active at specified time (relative to clip start)"""
        return [e for e in self.effects 
                if e.start_time <= time <= e.end_time]
    
    def to_dict(self) -> dict:
        """Convert to dictionary"""
        return {
            'clip_id': self.clip_id,
            'track_id': self.track_id,
            'start_time': self.start_time,
            'duration': self.duration,
            'source_file': self.source_file,
            'source_start': self.source_start,
            'source_end': self.source_end,
            'effects': [e.to_dict() for e in self.effects],
            'properties': self.properties
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'Clip':
        """Create from dictionary"""
        clip = cls(
            clip_id=data['clip_id'],
            track_id=data['track_id'],
            start_time=data['start_time'],
            duration=data['duration'],
            source_file=data.get('source_file'),
            source_start=data.get('source_start', 0.0),
            source_end=data.get('source_end'),
            properties=data.get('properties', {})
        )
        if 'effects' in data:
            clip.effects = [Effect.from_dict(e) for e in data['effects']]
        return clip


@dataclass
class Track:
    """Represents a timeline track that holds clips"""
    track_id: str
    track_type: TrackType
    name: str
    clips: List[Clip] = field(default_factory=list)
    is_muted: bool = False
    is_solo: bool = False
    is_locked: bool = False
    is_visible: bool = True
    height: int = 60  # Track height in pixels
    properties: Dict[str, Any] = field(default_factory=dict)
    
    def add_clip(self, clip: Clip):
        """Add a clip to this track"""
        clip.track_id = self.track_id
        self.clips.append(clip)
        # Sort clips by start time
        self.clips.sort(key=lambda c: c.start_time)
    
    def remove_clip(self, clip_id: str) -> bool:
        """Remove a clip by ID"""
        for i, clip in enumerate(self.clips):
            if clip.clip_id == clip_id:
                self.clips.pop(i)
                return True
        return False
    
    def get_clip(self, clip_id: str) -> Optional[Clip]:
        """Get clip by ID"""
        for clip in self.clips:
            if clip.clip_id == clip_id:
                return clip
        return None
    
    def get_clips_at_time(self, time: float) -> List[Clip]:
        """Get all clips active at a given time"""
        return [c for c in self.clips if c.start_time <= time <= c.end_time]
    
    def check_overlap(self, clip: Clip, exclude_clip_id: Optional[str] = None) -> bool:
        """
        Check if clip would overlap with any existing clips
        
        Args:
            clip: Clip to check
            exclude_clip_id: Clip ID to exclude from check (for moving existing clips)
            
        Returns:
            True if overlap detected, False otherwise
        """
        for existing_clip in self.clips:
            if exclude_clip_id and existing_clip.clip_id == exclude_clip_id:
                continue
            
            # Check for overlap
            if not (clip.end_time <= existing_clip.start_time or 
                   clip.start_time >= existing_clip.end_time):
                return True
        
        return False
    
    def find_next_free_slot(self, duration: float, after_time: float = 0.0) -> float:
        """
        Find the next available time slot for a clip of given duration
        
        Args:
            duration: Clip duration in seconds
            after_time: Start searching after this time
            
        Returns:
            Start time for the next free slot
        """
        candidate_time = after_time
        
        # Sort clips by start time
        sorted_clips = sorted(self.clips, key=lambda c: c.start_time)
        
        for clip in sorted_clips:
            if clip.start_time >= candidate_time:
                # Check if there's enough space before this clip
                if clip.start_time - candidate_time >= duration:
                    return candidate_time
                # Move past this clip
                candidate_time = clip.end_time
        
        # No clips or space after all clips
        return candidate_time
    
    def to_dict(self) -> dict:
        """Convert to dictionary"""
        return {
            'track_id': self.track_id,
            'track_type': self.track_type.value,
            'name': self.name,
            'clips': [c.to_dict() for c in self.clips],
            'is_muted': self.is_muted,
            'is_solo': self.is_solo,
            'is_locked': self.is_locked,
            'is_visible': self.is_visible,
            'height': self.height,
            'properties': self.properties
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'Track':
        """Create from dictionary"""
        track = cls(
            track_id=data['track_id'],
            track_type=TrackType(data['track_type']),
            name=data['name'],
            is_muted=data.get('is_muted', False),
            is_solo=data.get('is_solo', False),
            is_locked=data.get('is_locked', False),
            is_visible=data.get('is_visible', True),
            height=data.get('height', 60),
            properties=data.get('properties', {})
        )
        if 'clips' in data:
            track.clips = [Clip.from_dict(c) for c in data['clips']]
        return track


class TimelineData:
    """Manages the complete timeline with all tracks"""
    
    def __init__(self):
        self.tracks: List[Track] = []
        self.duration: float = 0.0  # Total timeline duration
        self.playhead_position: float = 0.0
        self.zoom_level: float = 1.0  # Pixels per second
        self.snap_to_grid: bool = True
        self.grid_interval: float = 1.0  # Seconds
        self.metadata: Dict[str, Any] = {}
    
    def add_track(self, track: Track):
        """Add a track to the timeline"""
        self.tracks.append(track)
        self._update_duration()
    
    def remove_track(self, track_id: str) -> bool:
        """Remove a track by ID"""
        for i, track in enumerate(self.tracks):
            if track.track_id == track_id:
                self.tracks.pop(i)
                self._update_duration()
                return True
        return False
    
    def get_track(self, track_id: str) -> Optional[Track]:
        """Get track by ID"""
        for track in self.tracks:
            if track.track_id == track_id:
                return track
        return None
    
    def get_tracks_by_type(self, track_type: TrackType) -> List[Track]:
        """Get all tracks of a specific type"""
        return [t for t in self.tracks if t.track_type == track_type]
    
    def _update_duration(self):
        """Update timeline duration based on all clips"""
        max_end = 0.0
        for track in self.tracks:
            for clip in track.clips:
                max_end = max(max_end, clip.end_time)
        self.duration = max_end
    
    def snap_time(self, time: float) -> float:
        """Snap time to grid if enabled"""
        if not self.snap_to_grid:
            return time
        return round(time / self.grid_interval) * self.grid_interval
    
    def to_dict(self) -> dict:
        """Convert to dictionary"""
        return {
            'tracks': [t.to_dict() for t in self.tracks],
            'duration': self.duration,
            'playhead_position': self.playhead_position,
            'zoom_level': self.zoom_level,
            'snap_to_grid': self.snap_to_grid,
            'grid_interval': self.grid_interval,
            'metadata': self.metadata
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'TimelineData':
        """Create from dictionary"""
        timeline = cls()
        timeline.tracks = [Track.from_dict(t) for t in data.get('tracks', [])]
        timeline.duration = data.get('duration', 0.0)
        timeline.playhead_position = data.get('playhead_position', 0.0)
        timeline.zoom_level = data.get('zoom_level', 1.0)
        timeline.snap_to_grid = data.get('snap_to_grid', True)
        timeline.grid_interval = data.get('grid_interval', 1.0)
        timeline.metadata = data.get('metadata', {})
        return timeline
    
    def clear(self):
        """Clear all tracks and reset timeline"""
        self.tracks = []
        self.duration = 0.0
        self.playhead_position = 0.0
