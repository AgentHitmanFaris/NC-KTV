"""
Cross-Project Import Dialog for NC-KTV
Allows importing components from other .nctv files
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, 
    QPushButton, QCheckBox, QGroupBox, QFileDialog,
    QMessageBox, QLineEdit
)
from PyQt6.QtCore import Qt
from pathlib import Path
from typing import Optional

from core.project import Project


class ImportDialog(QDialog):
    """Dialog for importing components from another project"""
    
    def __init__(self, current_project: Project, parent=None):
        super().__init__(parent)
        self.current_project = current_project
        self.import_project: Optional[Project] = None
        
        self.setWindowTitle("Import from Project")
        self.setModal(True)
        self.resize(500, 400)
        
        self._init_ui()
    
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        
        # File selection
        file_group = QGroupBox("Source Project")
        file_layout = QHBoxLayout()
        
        self.file_path_edit = QLineEdit()
        self.file_path_edit.setReadOnly(True)
        self.file_path_edit.setPlaceholderText("No file selected...")
        file_layout.addWidget(self.file_path_edit)
        
        btn_browse = QPushButton("📁 Browse...")
        btn_browse.clicked.connect(self._browse_file)
        file_layout.addWidget(btn_browse)
        
        file_group.setLayout(file_layout)
        layout.addWidget(file_group)
        
        # Import options
        options_group = QGroupBox("Components to Import")
        options_layout = QVBoxLayout()
        
        self.cb_lyrics = QCheckBox("Lyrics (text and timestamps)")
        self.cb_lyrics.setChecked(True)
        options_layout.addWidget(self.cb_lyrics)
        
        self.cb_audio = QCheckBox("Audio files (instrumental, vocals)")
        options_layout.addWidget(self.cb_audio)
        
        self.cb_metadata = QCheckBox("Metadata (settings, dates)")
        options_layout.addWidget(self.cb_metadata)
        
        options_layout.addStretch()
        options_group.setLayout(options_layout)
        layout.addWidget(options_group)
        
        # Merge strategy
        merge_group = QGroupBox("Merge Strategy")
        merge_layout = QVBoxLayout()
        
        info_label = QLabel(
            "⚠️ Importing will overwrite existing data for selected components.\n"
            "This action cannot be undone."
        )
        info_label.setStyleSheet("color: #FF9800; padding: 10px;")
        info_label.setWordWrap(True)
        merge_layout.addWidget(info_label)
        
        merge_group.setLayout(merge_layout)
        layout.addWidget(merge_group)
        
        # Buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        btn_cancel = QPushButton("Cancel")
        btn_cancel.clicked.connect(self.reject)
        button_layout.addWidget(btn_cancel)
        
        self.btn_import = QPushButton("✅ Import")
        self.btn_import.clicked.connect(self._perform_import)
        self.btn_import.setEnabled(False)
        self.btn_import.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
        button_layout.addWidget(self.btn_import)
        
        layout.addLayout(button_layout)
    
    def _browse_file(self):
        """Open file browser to select project"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Project to Import",
            "",
            "NC-KTV Project (*.nctv)"
        )
        
        if file_path:
            try:
                # Load project
                self.import_project = Project.load(Path(file_path))
                self.file_path_edit.setText(file_path)
                self.btn_import.setEnabled(True)
                
                QMessageBox.information(
                    self,
                    "Success",
                    f"Loaded project: {self.import_project.project_name}"
                )
            except Exception as e:
                QMessageBox.critical(
                    self,
                    "Error",
                    f"Failed to load project:\n{e}"
                )
                self.import_project = None
                self.file_path_edit.clear()
                self.btn_import.setEnabled(False)
    
    def _perform_import(self):
        """Import selected components"""
        if not self.import_project:
            return
        
        # Confirm import
        reply = QMessageBox.question(
            self,
            "Confirm Import",
            "Are you sure you want to import the selected components?\n"
            "This will overwrite existing data.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply != QMessageBox.StandardButton.Yes:
            return
        
        try:
            # Import lyrics
            if self.cb_lyrics.isChecked():
                self.current_project.lyrics = self.import_project.lyrics
            
            # Import audio files
            if self.cb_audio.isChecked():
                self.current_project.instrumental_file = self.import_project.instrumental_file
                self.current_project.vocals_file = self.import_project.vocals_file
            
            # Import metadata
            if self.cb_metadata.isChecked():
                self.current_project.settings = self.import_project.settings
                # Note: We don't overwrite dates to preserve current project's timeline
            
            QMessageBox.information(
                self,
                "Success",
                "Components imported successfully!"
            )
            
            self.accept()
            
        except Exception as e:
            QMessageBox.critical(
                self,
                "Error",
                f"Import failed:\n{e}"
            )
