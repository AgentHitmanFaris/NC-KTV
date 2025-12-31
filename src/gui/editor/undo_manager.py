"""
Undo Manager for Lyrics Editor
Implements a simple undo/redo system using state snapshots
"""

from core.lyrics import LyricsData
import logging

logger = logging.getLogger(__name__)


class UndoManager:
    """
    Manages undo/redo operations for lyrics editing.
    Uses snapshot-based approach for simplicity.
    """
    
    def __init__(self, max_history: int = 50):
        self.undo_stack = []  # List of LyricsData snapshots
        self.redo_stack = []
        self.max_history = max_history
        self._current_lyrics = None
        
    def set_initial_state(self, lyrics: LyricsData):
        """Set the initial state (should be called when editor opens)"""
        self._current_lyrics = lyrics
        self.undo_stack.clear()
        self.redo_stack.clear()
        
    def push_state(self, lyrics: LyricsData):
        """Push current state before making changes"""
        if self._current_lyrics is None:
            self._current_lyrics = lyrics
            return
            
        # Save current state to undo stack
        snapshot = self._clone_lyrics(self._current_lyrics)
        self.undo_stack.append(snapshot)
        
        # Limit history
        if len(self.undo_stack) > self.max_history:
            self.undo_stack.pop(0)
        
        # Clear redo stack on new action
        self.redo_stack.clear()
        
        # Update current
        self._current_lyrics = lyrics
        
        logger.debug(f"Pushed state. Undo stack size: {len(self.undo_stack)}")
        
    def can_undo(self) -> bool:
        return len(self.undo_stack) > 0
        
    def can_redo(self) -> bool:
        return len(self.redo_stack) > 0
        
    def undo(self, current_lyrics: LyricsData) -> LyricsData:
        """Undo last action, returns previous state"""
        if not self.can_undo():
            return current_lyrics
            
        # Save current to redo stack
        self.redo_stack.append(self._clone_lyrics(current_lyrics))
        
        # Pop from undo stack
        previous = self.undo_stack.pop()
        self._current_lyrics = previous
        
        logger.debug(f"Undo. Undo stack: {len(self.undo_stack)}, Redo stack: {len(self.redo_stack)}")
        return previous
        
    def redo(self, current_lyrics: LyricsData) -> LyricsData:
        """Redo last undone action, returns next state"""
        if not self.can_redo():
            return current_lyrics
            
        # Save current to undo stack
        self.undo_stack.append(self._clone_lyrics(current_lyrics))
        
        # Pop from redo stack
        next_state = self.redo_stack.pop()
        self._current_lyrics = next_state
        
        logger.debug(f"Redo. Undo stack: {len(self.undo_stack)}, Redo stack: {len(self.redo_stack)}")
        return next_state
        
    def _clone_lyrics(self, lyrics: LyricsData) -> LyricsData:
        """Create a deep copy of lyrics data"""
        return LyricsData.from_dict(lyrics.to_dict())
        
    def get_undo_count(self) -> int:
        return len(self.undo_stack)
        
    def get_redo_count(self) -> int:
        return len(self.redo_stack)
