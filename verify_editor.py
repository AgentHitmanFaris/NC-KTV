
import sys
import os
from pathlib import Path

# Add src to pythonpath
src_path = Path(r"d:\Document\NC-KTV\src")
sys.path.append(str(src_path))

try:
    from PyQt6.QtWidgets import QApplication
    from utils.config import Config
    from core.project import Project
    from gui.editor.editor_mode import EditorMode
    
    # Mock resources
    app = QApplication(sys.argv)
    config = Config()
    project = Project("Test Project", "test_path")
    
    print("Attempting to instantiate EditorMode...")
    editor = EditorMode(config, project)
    print("EditorMode instantiated successfully!")
    
    # Check if we can show it (creates window handle)
    editor.show()
    print("EditorMode shown successfully!")
    
    # Clean up
    editor.close()
    print("Verification Complete.")
    
except Exception as e:
    print(f"FAILED to instantiate EditorMode: {e}")
    import traceback
    traceback.print_exc()
