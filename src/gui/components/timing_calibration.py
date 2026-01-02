"""
Timing Calibration Dialog for NC-KTV
Helps users automatically detect and fix timing drift
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QPushButton,
    QLabel, QSpinBox, QDoubleSpinBox, QGroupBox, QMessageBox
)
from PyQt6.QtCore import Qt
import logging

logger = logging.getLogger(__name__)


class TimingCalibrationDialog(QDialog):
    """Dialog for automatic timing calibration"""
    
    def __init__(self, project, parent=None):
        super().__init__(parent)
        self.project = project
        self.setWindowTitle("Timing Calibration")
        self.setMinimumWidth(500)
        
        self._init_ui()
        self._check_sample_rates()
    
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        
        # Title
        title = QLabel("🎯 Timing Synchronization")
        title.setStyleSheet("font-size: 16pt; font-weight: bold;")
        layout.addWidget(title)
        
        # Info
        info = QLabel(
            "This tool helps eliminate timing drift between vocal separation, "
            "transcription, and playback."
        )
        info.setWordWrap(True)
        layout.addWidget(info)
        
        layout.addSpacing(15)
        
        # Sample Rate Check
        sample_group = QGroupBox("1. Sample Rate Validation")
        sample_layout = QVBoxLayout()
        
        self.sample_status_label = QLabel("Checking...")
        sample_layout.addWidget(self.sample_status_label)
        
        self.sample_detail_label = QLabel("")
        self.sample_detail_label.setWordWrap(True)
        self.sample_detail_label.setStyleSheet("color: #666; font-size: 9pt;")
        sample_layout.addWidget(self.sample_detail_label)
        
        sample_group.setLayout(sample_layout)
        layout.addWidget(sample_group)
        
        # Global Offset
        offset_group = QGroupBox("2. Global Timing Offset")
        offset_layout = QVBoxLayout()
        
        offset_info = QLabel("Adjust if lyrics consistently lag or lead vocals:")
        offset_layout.addWidget(offset_info)
        
        offset_controls = QHBoxLayout()
        offset_controls.addWidget(QLabel("Offset (seconds):"))
        
        self.offset_spinbox = QDoubleSpinBox()
        self.offset_spinbox.setRange(-5.0, 5.0)
        self.offset_spinbox.setSingleStep(0.01)
        self.offset_spinbox.setDecimals(3)
        self.offset_spinbox.setValue(
            self.project.timing_metadata.get('global_offset', 0.0)
        )
        offset_controls.addWidget(self.offset_spinbox)
        
        offset_controls.addWidget(QLabel("(negative = earlier, positive = later)"))
        offset_controls.addStretch()
        offset_layout.addLayout(offset_controls)
        
        # Auto-calibrate button
        self.auto_btn = QPushButton("🎵 Auto-Calibrate")
        self.auto_btn.setToolTip(
            "Play first lyrics line and press SPACE when you hear it. "
            "The offset will be calculated automatically."
        )
        self.auto_btn.clicked.connect(self._start_auto_calibrate)
        offset_layout.addWidget(self.auto_btn)
        
        offset_group.setLayout(offset_layout)
        layout.addWidget(offset_group)
        
        # Transcription Source
        transcription_group = QGroupBox("3. Transcription Source")
        transcription_layout = QVBoxLayout()
        
        transcription_info = QLabel(
            "✅ RECOMMENDED: Use 'Original Audio' for transcription\n"
            "This eliminates timing offset from vocal separation."
        )
        transcription_info.setStyleSheet("color: #0a0; font-weight: bold;")
        transcription_layout.addWidget(transcription_info)
        
        current_source = self.project.timing_metadata.get('transcription_source', 'unknown')
        source_label = QLabel(f"Current setting: {current_source}")
        transcription_layout.addWidget(source_label)
        
        transcription_group.setLayout(transcription_layout)
        layout.addWidget(transcription_group)
        
        layout.addSpacing(15)
        
        # Buttons
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        
        btn_apply = QPushButton("Apply")
        btn_apply.clicked.connect(self.accept)
        btn_layout.addWidget(btn_apply)
        
        btn_cancel = QPushButton("Cancel")
        btn_cancel.clicked.connect(self.reject)
        btn_layout.addWidget(btn_cancel)
        
        layout.addLayout(btn_layout)
    
    def _check_sample_rates(self):
        """Check sample rates of all audio files"""
        try:
            results = self.project.validate_sample_rates_sync()
            
            if results['valid']:
                sr = results['recommended_rate']
                self.sample_status_label.setText(f"✅ All files match: {sr} Hz")
                self.sample_status_label.setStyleSheet("color: green; font-weight: bold;")
                
                files = results['sample_rates']
                details = "Files checked:\n" + "\n".join(
                    f"  • {name}: {rate} Hz" 
                    for name, rate in files.items() 
                    if rate
                )
                self.sample_detail_label.setText(details)
            else:
                mismatches = results['mismatches']
                self.sample_status_label.setText(f"⚠️ Sample rate mismatch detected!")
                self.sample_status_label.setStyleSheet("color: orange; font-weight: bold;")
                
                files = results['sample_rates']
                details = (
                    f"Found different rates: {mismatches}\n\n"
                    "Files:\n" + "\n".join(
                        f"  • {name}: {rate} Hz" 
                        for name, rate in files.items() 
                        if rate
                    ) + f"\n\nRecommended: Resample all to {results['recommended_rate']} Hz"
                )
                self.sample_detail_label.setText(details)
                
        except Exception as e:
            logger.error(f"Sample rate check failed: {e}")
            self.sample_status_label.setText("⚠️ Unable to check sample rates")
            self.sample_detail_label.setText(str(e))
    
    def _start_auto_calibrate(self):
        """Start auto-calibration process"""
        QMessageBox.information(
            self,
            "Auto-Calibration",
            "Auto-calibration feature coming soon!\n\n"
            "For now, manually adjust the offset:\n"
            "• If lyrics appear LATE: Use negative offset (e.g., -0.5)\n"
            "• If lyrics appear EARLY: Use positive offset (e.g., +0.5)\n\n"
            "Start with small adjustments (±0.1s) and fine-tune."
        )
    
    def get_global_offset(self) -> float:
        """Get the configured global offset"""
        return self.offset_spinbox.value()
    
    def apply_settings(self):
        """Apply timing settings to project"""
        offset = self.get_global_offset()
        self.project.timing_metadata['global_offset'] = offset
        self.project.audio_clock.set_master_offset(offset)
        logger.info(f"Applied global offset: {offset:.3f}s")
