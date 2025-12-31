"""
NC-KTV - Music Video Karaoke Maker
Main application entry point
"""

import sys
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent / "src"))

# Add portable FFmpeg to PATH if exists
import os
ffmpeg_dir = Path(__file__).parent / "ffmpeg"
if ffmpeg_dir.exists():
    os.environ["PATH"] = str(ffmpeg_dir) + os.pathsep + os.environ.get("PATH", "")

# Add PyTorch lib to PATH (for ONNX Runtime GPU)
import site
site_packages = Path(site.getsitepackages()[0])
torch_lib = site_packages / "torch" / "lib"
if torch_lib.exists():
    os.environ["PATH"] = str(torch_lib) + os.pathsep + os.environ.get("PATH", "")

# Set local cache directories
cache_dir = Path(__file__).parent / ".cache"
cache_dir.mkdir(exist_ok=True)
os.environ["HF_HOME"] = str(cache_dir / "huggingface")
os.environ["TORCH_HOME"] = str(cache_dir / "torch")
os.environ["NUMBA_CACHE_DIR"] = str(cache_dir / "numba")

from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import Qt

from gui.main_window import MainWindow
from utils.config import Config
import logging

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[logging.StreamHandler(sys.stdout)]
)

def main():
    """Main application entry point"""
    
    # Enable high DPI scaling
    QApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
    )
    
    # Create application
    app = QApplication(sys.argv)
    app.setApplicationName("NC-KTV")
    app.setOrganizationName("NC-KTV")
    
    # Set App Icon
    from PyQt6.QtGui import QIcon
    icon_path = Path(__file__).parent / "assets" / "logo.png"
    if icon_path.exists():
        app.setWindowIcon(QIcon(str(icon_path)))
    
    # Load configuration
    config = Config()
    
    # Create and show main window
    window = MainWindow(config)
    window.show()
    
    # Run application
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
