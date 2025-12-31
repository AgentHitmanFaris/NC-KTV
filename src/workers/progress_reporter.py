"""
Progress reporter for multi-stage processing
"""

from dataclasses import dataclass
from typing import Dict, Optional
import time


@dataclass
class Stage:
    """Processing stage"""
    name: str
    weight: float  # Percentage of total (0-100)
    started: bool = False
    completed: bool = False
    progress: float = 0.0  # 0-100


class ProgressReporter:
    """Track and report progress across multiple stages"""
    
    def __init__(self):
        """Initialize progress reporter"""
        self.stages: Dict[str, Stage] = {}
        self.current_stage: Optional[str] = None
        self.start_time: Optional[float] = None
        self.total_weight = 0.0
    
    def add_stage(self, name: str, weight: float):
        """Add a processing stage
        
        Args:
            name: Stage name
            weight: Stage weight (percentage of total)
        """
        self.stages[name] = Stage(name=name, weight=weight)
        self.total_weight += weight
    
    def start_stage(self, name: str):
        """Start a stage
        
        Args:
            name: Stage name
        """
        if name not in self.stages:
            raise ValueError(f"Unknown stage: {name}")
        
        self.current_stage = name
        self.stages[name].started = True
        
        if self.start_time is None:
            self.start_time = time.time()
    
    def update_stage(self, progress: float):
        """Update current stage progress
        
        Args:
            progress: Progress percentage (0-100)
        """
        if not self.current_stage:
            return
        
        self.stages[self.current_stage].progress = min(100.0, max(0.0, progress))
    
    def complete_stage(self):
        """Mark current stage as complete"""
        if not self.current_stage:
            return
        
        stage = self.stages[self.current_stage]
        stage.completed = True
        stage.progress = 100.0
    
    def get_overall_progress(self) -> float:
        """Get overall progress percentage
        
        Returns:
            Progress percentage (0-100)
        """
        if not self.stages or self.total_weight == 0:
            return 0.0
        
        total_progress = 0.0
        
        for stage in self.stages.values():
            stage_progress = stage.progress if stage.started else 0.0
            total_progress += (stage_progress / 100.0) * stage.weight
        
        return min(100.0, (total_progress / self.total_weight) * 100.0)
    
    def get_eta_seconds(self) -> Optional[float]:
        """Estimate time remaining in seconds
        
        Returns:
            Estimated seconds remaining, or None if can't estimate
        """
        if not self.start_time:
            return None
        
        overall_progress = self.get_overall_progress()
        
        if overall_progress <= 0:
            return None
        
        elapsed = time.time() - self.start_time
        total_estimated = elapsed / (overall_progress / 100.0)
        remaining = total_estimated - elapsed
        
        return max(0, remaining)
    
    def get_status_message(self) -> str:
        """Get current status message
        
        Returns:
            Human-readable status message
        """
        if not self.current_stage:
            return "Idle"
        
        stage = self.stages[self.current_stage]
        progress = stage.progress
        
        # User requested no percentage and no timing
        return f"{stage.name}..."
    
    @staticmethod
    def _format_time(seconds: float) -> str:
        """Format seconds as human-readable time
        
        Args:
            seconds: Time in seconds
        
        Returns:
            Formatted time string
        """
        if seconds < 60:
            return f"{int(seconds)}s"
        elif seconds < 3600:
            minutes = int(seconds / 60)
            secs = int(seconds % 60)
            return f"{minutes}m {secs}s"
        else:
            hours = int(seconds / 3600)
            minutes = int((seconds % 3600) / 60)
            return f"{hours}h {minutes}m"
    
    def reset(self):
        """Reset all progress"""
        self.stages.clear()
        self.current_stage = None
        self.start_time = None
        self.total_weight = 0.0
