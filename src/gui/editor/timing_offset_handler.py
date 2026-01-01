# Placeholder for timing offset method - will add to editor_mode.py

def _apply_timing_offset(self, offset_seconds: float):
    """Apply global timing offset to all lyrics"""
    if not hasattr(self, '_original_lyrics_times'):
        # Store original times on first offset
        self._original_lyrics_times = []
        for line in self.lyrics_data.lines:
            self._original_lyrics_times.append((line.start_time, line.end_time))
    
    # Apply offset to all lines
    for i, line in enumerate(self.lyrics_data.lines):
        if i < len(self._original_lyrics_times):
            orig_start, orig_end = self._original_lyrics_times[i]
            line.start_time = max(0.0, orig_start + offset_seconds)
            line.end_time = max(0.0, orig_end + offset_seconds)
    
    # Refresh table display
    self._refresh_table()
    self.is_dirty = True
    
    # Show feedback
    if offset_seconds != 0:
        direction = "earlier" if offset_seconds < 0 else "later"
        self.status_bar.setText(f"Timing shifted {abs(offset_seconds):.1f}s {direction}")
