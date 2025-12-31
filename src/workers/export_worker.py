"""
Export Worker for running FFmpeg in background thread
"""

from PyQt6.QtCore import QThread, pyqtSignal
import subprocess
import logging

logger = logging.getLogger(__name__)


class ExportWorker(QThread):
    """Worker thread for FFmpeg export"""
    
    progress = pyqtSignal(str)  # Status message
    finished = pyqtSignal(bool, str)  # Success, Message
    
    def __init__(self, cmd, output_path):
        super().__init__()
        self.cmd = cmd
        self.output_path = output_path
        
    def run(self):
        try:
            self.progress.emit("Starting FFmpeg...")
            
            # Run FFmpeg with progress parsing
            process = subprocess.Popen(
                self.cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                creationflags=subprocess.CREATE_NO_WINDOW
            )
            
            # Read output lines
            for line in process.stdout:
                line = line.strip()
                if line:
                    # Parse FFmpeg progress (look for time=)
                    if "time=" in line:
                        # Extract time portion
                        try:
                            time_part = line.split("time=")[1].split(" ")[0]
                            self.progress.emit(f"Encoding: {time_part}")
                        except:
                            self.progress.emit("Encoding...")
                    elif "frame=" in line:
                        self.progress.emit("Processing frames...")
            
            process.wait()
            
            if process.returncode == 0:
                self.finished.emit(True, str(self.output_path))
            else:
                self.finished.emit(False, f"FFmpeg exited with code {process.returncode}")
                
        except Exception as e:
            logger.error(f"Export error: {e}")
            self.finished.emit(False, str(e))
