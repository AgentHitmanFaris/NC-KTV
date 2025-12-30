"""
NC-KTV - Music Video Karaoke Maker
Main application entry point
"""

import sys
from pathlib import Path

from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import Qt

# Add src to path
sys.path.insert(0, str(Path(__file__).parent / "src"))

from gui.main_window import MainWindow
from utils.config import Config


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
    
    # Load configuration
    config = Config()
    
    # Create and show main window
    window = MainWindow(config)
    window.show()
    
    # Run application
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
