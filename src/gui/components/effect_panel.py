"""
Effect Panel Dialog for NC-KTV
Allows users to add and configure effects on timeline clips
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QFormLayout,
    QPushButton, QComboBox, QLabel, QDoubleSpinBox,
    QColorDialog, QGroupBox, QListWidget, QListWidgetItem,
    QPushButton, QMessageBox
)
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QColor

from core.timeline_data import Effect, EffectType, EasingCurve, Clip


class EffectPanelDialog(QDialog):
    """Dialog for adding and managing effects on a clip"""
    
    def __init__(self, clip: Clip, parent=None):
        super().__init__(parent)
        self.clip = clip
        self.current_effect_index = -1
        self.setWindowTitle(f"Effects - {clip.clip_id}")
        self.setMinimumWidth(600)
        self.setMinimumHeight(500)
        
        self._init_ui()
        self._load_effects()
    
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        
        # Top section: Effect list
        list_group = QGroupBox("Current Effects")
        list_layout = QVBoxLayout()
        
        self.effect_list = QListWidget()
        self.effect_list.currentRowChanged.connect(self._on_effect_selected)
        list_layout.addWidget(self.effect_list)
        
        # Effect list buttons
        list_btn_layout = QHBoxLayout()
        btn_add = QPushButton("+ Add Effect")
        btn_add.clicked.connect(self._add_new_effect)
        list_btn_layout.addWidget(btn_add)
        
        btn_remove = QPushButton("- Remove")
        btn_remove.clicked.connect(self._remove_effect)
        list_btn_layout.addWidget(btn_remove)
        
        list_btn_layout.addStretch()
        list_layout.addLayout(list_btn_layout)
        
        list_group.setLayout(list_layout)
        layout.addWidget(list_group)
        
        # Middle section: Effect properties
        props_group = QGroupBox("Effect Properties")
        props_layout = QFormLayout()
        
        # Effect Type
        self.combo_type = QComboBox()
        self.combo_type.addItems([e.value for e in EffectType])
        self.combo_type.currentTextChanged.connect(self._on_type_changed)
        props_layout.addRow("Effect Type:", self.combo_type)
        
        # Start Time (relative to clip start)
        self.spin_start = QDoubleSpinBox()
        self.spin_start.setRange(0.0, self.clip.duration)
        self.spin_start.setSingleStep(0.1)
        self.spin_start.setDecimals(2)
        self.spin_start.setSuffix(" s")
        props_layout.addRow("Start Time:", self.spin_start)
        
        # Duration
        self.spin_duration = QDoubleSpinBox()
        self.spin_duration.setRange(0.1, self.clip.duration)
        self.spin_duration.setSingleStep(0.1)
        self.spin_duration.setDecimals(2)
        self.spin_duration.setSuffix(" s")
        self.spin_duration.setValue(1.0)
        props_layout.addRow("Duration:", self.spin_duration)
        
        # Easing Curve
        self.combo_easing = QComboBox()
        self.combo_easing.addItems([e.value for e in EasingCurve])
        self.combo_easing.currentTextChanged.connect(self._on_easing_changed)
        props_layout.addRow("Easing:", self.combo_easing)
        
        # Custom curve button
        self.btn_curve = QPushButton("Edit Custom Curve...")
        self.btn_curve.clicked.connect(self._open_curve_editor)
        self.btn_curve.setEnabled(False)
        props_layout.addRow("", self.btn_curve)
        
        props_group.setLayout(props_layout)
        layout.addWidget(props_group)
        
        # Effect-specific properties group
        self.specific_group = QGroupBox("Effect-Specific Properties")
        self.specific_layout = QFormLayout()
        self.specific_group.setLayout(self.specific_layout)
        layout.addWidget(self.specific_group)
        
        # Bottom buttons
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        
        btn_apply = QPushButton("Apply Changes")
        btn_apply.clicked.connect(self._apply_changes)
        btn_layout.addWidget(btn_apply)
        
        btn_close = QPushButton("Close")
        btn_close.clicked.connect(self.accept)
        btn_layout.addWidget(btn_close)
        
        layout.addLayout(btn_layout)
        
        # Initialize specific properties
        self._update_specific_properties()
    
    def _load_effects(self):
        """Load existing effects from clip"""
        self.effect_list.clear()
        for i, effect in enumerate(self.clip.effects):
            item_text = f"{effect.effect_type.value} ({effect.start_time:.2f}s - {effect.end_time:.2f}s)"
            self.effect_list.addItem(item_text)
    
    def _on_effect_selected(self, index):
        """Load selected effect into UI"""
        if index < 0 or index >= len(self.clip.effects):
            return
        
        self.current_effect_index = index
        effect = self.clip.effects[index]
        
        # Update UI
        self.combo_type.setCurrentText(effect.effect_type.value)
        self.spin_start.setValue(effect.start_time)
        self.spin_duration.setValue(effect.duration)
        self.combo_easing.setCurrentText(effect.easing.value)
        
        # Update specific properties
        self._update_specific_properties()
        self._load_specific_properties(effect)
    
    def _add_new_effect(self):
        """Add a new effect to the clip"""
        # Create default effect
        effect = Effect(
            effect_type=EffectType.FADE_IN,
            start_time=0.0,
            duration=1.0,
            easing=EasingCurve.LINEAR
        )
        
        self.clip.add_effect(effect)
        self._load_effects()
        
        # Select the new effect
        self.effect_list.setCurrentRow(len(self.clip.effects) - 1)
    
    def _remove_effect(self):
        """Remove selected effect"""
        if self.current_effect_index < 0:
            return
        
        reply = QMessageBox.question(
            self,
            "Remove Effect",
            "Are you sure you want to remove this effect?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            self.clip.remove_effect(self.current_effect_index)
            self._load_effects()
            self.current_effect_index = -1
    
    def _on_type_changed(self, type_str):
        """Handle effect type change"""
        self._update_specific_properties()
    
    def _on_easing_changed(self, easing_str):
        """Handle easing curve change"""
        is_custom = easing_str == EasingCurve.CUSTOM_BEZIER.value
        self.btn_curve.setEnabled(is_custom)
    
    def _update_specific_properties(self):
        """Update effect-specific property widgets based on type"""
        # Clear existing widgets
        while self.specific_layout.rowCount() > 0:
            self.specific_layout.removeRow(0)
        
        effect_type = self.combo_type.currentText()
        
        if effect_type in [EffectType.FADE_IN.value, EffectType.FADE_OUT.value]:
            # Fade effects: intensity
            spin_intensity = QDoubleSpinBox()
            spin_intensity.setRange(0.0, 1.0)
            spin_intensity.setSingleStep(0.1)
            spin_intensity.setValue(1.0)
            spin_intensity.setObjectName("intensity")
            self.specific_layout.addRow("Intensity:", spin_intensity)
        
        elif effect_type in [EffectType.SLIDE_LEFT.value, EffectType.SLIDE_RIGHT.value]:
            # Slide effects: distance
            spin_distance = QDoubleSpinBox()
            spin_distance.setRange(0, 1920)
            spin_distance.setSingleStep(10)
            spin_distance.setValue(100)
            spin_distance.setSuffix(" px")
            spin_distance.setObjectName("distance")
            self.specific_layout.addRow("Distance:", spin_distance)
        
        elif effect_type in [EffectType.ZOOM_IN.value, EffectType.ZOOM_OUT.value]:
            # Zoom effects: scale factor
            spin_scale = QDoubleSpinBox()
            spin_scale.setRange(0.1, 5.0)
            spin_scale.setSingleStep(0.1)
            spin_scale.setValue(1.5)
            spin_scale.setObjectName("scale")
            self.specific_layout.addRow("Scale Factor:", spin_scale)
        
        elif effect_type == EffectType.BLUR.value:
            # Blur: radius
            spin_radius = QDoubleSpinBox()
            spin_radius.setRange(0, 100)
            spin_radius.setSingleStep(1)
            spin_radius.setValue(10)
            spin_radius.setSuffix(" px")
            spin_radius.setObjectName("radius")
            self.specific_layout.addRow("Blur Radius:", spin_radius)
        
        elif effect_type == EffectType.COLOR_SHIFT.value:
            # Color shift: target color and intensity
            btn_color = QPushButton("Choose Color...")
            btn_color.setObjectName("color_button")
            btn_color.clicked.connect(self._pick_color)
            self.specific_layout.addRow("Target Color:", btn_color)
            
            spin_intensity = QDoubleSpinBox()
            spin_intensity.setRange(0.0, 1.0)
            spin_intensity.setSingleStep(0.1)
            spin_intensity.setValue(0.5)
            spin_intensity.setObjectName("intensity")
            self.specific_layout.addRow("Intensity:", spin_intensity)
    
    def _load_specific_properties(self, effect: Effect):
        """Load effect-specific properties into widgets"""
        for i in range(self.specific_layout.rowCount()):
            widget = self.specific_layout.itemAt(i, QFormLayout.ItemRole.FieldRole).widget()
            if not widget:
                continue
            
            obj_name = widget.objectName()
            if obj_name in effect.properties:
                if isinstance(widget, QDoubleSpinBox):
                    widget.setValue(effect.properties[obj_name])
    
    def _pick_color(self):
        """Open color picker for color shift effect"""
        color = QColorDialog.getColor()
        if color.isValid():
            # Store color as hex string
            sender = self.sender()
            sender.setStyleSheet(f"background-color: {color.name()};")
            sender.setProperty("selected_color", color.name())
    
    def _apply_changes(self):
        """Apply changes to selected effect"""
        if self.current_effect_index < 0 or self.current_effect_index >= len(self.clip.effects):
            QMessageBox.warning(self, "No Effect Selected", "Please select an effect to modify.")
            return
        
        effect = self.clip.effects[self.current_effect_index]
        
        # Update basic properties
        effect.effect_type = EffectType(self.combo_type.currentText())
        effect.start_time = self.spin_start.value()
        effect.duration = self.spin_duration.value()
        effect.easing = EasingCurve(self.combo_easing.currentText())
        
        # Update specific properties
        effect.properties.clear()
        for i in range(self.specific_layout.rowCount()):
            widget = self.specific_layout.itemAt(i, QFormLayout.ItemRole.FieldRole).widget()
            if not widget:
                continue
            
            obj_name = widget.objectName()
            if isinstance(widget, QDoubleSpinBox):
                effect.properties[obj_name] = widget.value()
            elif obj_name == "color_button" and widget.property("selected_color"):
                effect.properties["color"] = widget.property("selected_color")
        
        # Re-sort effects
        self.clip.effects.sort(key=lambda e: e.start_time)
        
        # Reload list
        self._load_effects()
        
        QMessageBox.information(self, "Success", "Effect updated successfully!")
    
    def _open_curve_editor(self):
        """Open the curve editor dialog"""
        from gui.components.curve_editor import CurveEditorDialog
        
        if self.current_effect_index < 0:
            return
        
        effect = self.clip.effects[self.current_effect_index]
        
        dialog = CurveEditorDialog(effect.custom_curve_points, self)
        if dialog.exec():
            effect.custom_curve_points = dialog.get_curve_points()
            QMessageBox.information(self, "Success", "Custom curve applied!")
    
    def get_modified_clip(self) -> Clip:
        """Return the modified clip"""
        return self.clip
