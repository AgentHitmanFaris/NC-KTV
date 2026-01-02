"""
Effect Compositor for NC-KTV
Calculates effect values at any point in time using Bezier curves
"""

from typing import List, Tuple, Optional
from core.timeline_data import Effect, EffectType, EasingCurve, Clip
from PyQt6.QtGui import QColor


class EffectCompositor:
    """Computes effect values for rendering"""
    
    def __init__(self):
        # Cache for Bezier evaluations
        self._bezier_cache = {}
    
    def get_active_effects(self, clip: Clip, time: float) -> List[Effect]:
        """Get all effects active at the given time (relative to clip start)"""
        active = []
        relative_time = time - clip.start_time
        
        for effect in clip.effects:
            if effect.start_time <= relative_time <= effect.end_time:
                active.append(effect)
        
        return active
    
    def evaluate_curve(self, easing: EasingCurve, t: float, 
                      custom_points: Optional[List[Tuple[float, float]]] = None) -> float:
        """
        Evaluate easing curve at parameter t (0.0 to 1.0)
        Returns value from 0.0 to 1.0
        """
        t = max(0.0, min(1.0, t))  # Clamp to [0, 1]
        
        if easing == EasingCurve.LINEAR:
            return t
        
        elif easing == EasingCurve.EASE_IN:
            # Cubic ease-in: slow start, fast end
            return t * t * t
        
        elif easing == EasingCurve.EASE_OUT:
            # Cubic ease-out: fast start, slow end
            return 1.0 - pow(1.0 - t, 3)
        
        elif easing == EasingCurve.EASE_IN_OUT:
            # Cubic ease-in-out: slow both ends
            if t < 0.5:
                return 4.0 * t * t * t
            else:
                return 1.0 - pow(-2.0 * t + 2.0, 3) / 2.0
        
        elif easing == EasingCurve.CUSTOM_BEZIER and custom_points:
            return self._evaluate_bezier_curve(custom_points, t)
        
        else:
            # Fallback to linear
            return t
    
    def _evaluate_bezier_curve(self, points: List[Tuple[float, float]], t: float) -> float:
        """
        Evaluate cubic Bezier curve at parameter t
        Uses De Casteljau's algorithm for numerical stability
        """
        if len(points) != 4:
            return t  # Fallback
        
        # Cache key
        cache_key = (tuple(tuple(p) for p in points), round(t, 3))
        if cache_key in self._bezier_cache:
            return self._bezier_cache[cache_key]
        
        p0, p1, p2, p3 = points
        
        # Cubic Bezier formula for Y coordinate
        # B(t) = (1-t)³P₀ + 3(1-t)²tP₁ + 3(1-t)t²P₂ + t³P₃
        one_minus_t = 1.0 - t
        
        y = (one_minus_t**3 * p0[1] +
             3 * one_minus_t**2 * t * p1[1] +
             3 * one_minus_t * t**2 * p2[1] +
             t**3 * p3[1])
        
        # Clamp output
        result = max(0.0, min(1.0, y))
        
        # Cache result
        self._bezier_cache[cache_key] = result
        
        return result
    
    def calculate_effect_progress(self, effect: Effect, time: float) -> float:
        """Calculate normalized progress (0-1) through the effect"""
        if effect.duration <= 0:
            return 1.0
        
        progress = (time - effect.start_time) / effect.duration
        return max(0.0, min(1.0, progress))
    
    def calculate_fade_value(self, effect: Effect, time: float) -> float:
        """Calculate opacity for fade effects (0.0 to 1.0)"""
        progress = self.calculate_effect_progress(effect, time)
        curve_value = self.evaluate_curve(effect.easing, progress, effect.custom_curve_points)
        intensity = effect.properties.get('intensity', 1.0)
        
        if effect.effect_type == EffectType.FADE_IN:
            return curve_value * intensity
        elif effect.effect_type == EffectType.FADE_OUT:
            return (1.0 - curve_value) * intensity
        else:
            return 1.0
    
    def calculate_slide_offset(self, effect: Effect, time: float) -> Tuple[int, int]:
        """Calculate x,y offset for slide effects (in pixels)"""
        progress = self.calculate_effect_progress(effect, time)
        curve_value = self.evaluate_curve(effect.easing, progress, effect.custom_curve_points)
        distance = effect.properties.get('distance', 100)
        
        # Calculate how much to offset (starts at distance, ends at 0)
        offset = distance * (1.0 - curve_value)
        
        if effect.effect_type == EffectType.SLIDE_LEFT:
            return (-int(offset), 0)  # Slide from left (negative X)
        elif effect.effect_type == EffectType.SLIDE_RIGHT:
            return (int(offset), 0)   # Slide from right (positive X)
        else:
            return (0, 0)
    
    def calculate_zoom_scale(self, effect: Effect, time: float) -> float:
        """Calculate scale factor for zoom effects"""
        progress = self.calculate_effect_progress(effect, time)
        curve_value = self.evaluate_curve(effect.easing, progress, effect.custom_curve_points)
        scale_factor = effect.properties.get('scale', 1.5)
        
        if effect.effect_type == EffectType.ZOOM_IN:
            # Start at 1.0, end at scale_factor
            return 1.0 + curve_value * (scale_factor - 1.0)
        elif effect.effect_type == EffectType.ZOOM_OUT:
            # Start at scale_factor, end at 1.0
            return scale_factor - curve_value * (scale_factor - 1.0)
        else:
            return 1.0
    
    def calculate_blur_radius(self, effect: Effect, time: float) -> int:
        """Calculate blur radius in pixels"""
        if effect.effect_type != EffectType.BLUR:
            return 0
        
        progress = self.calculate_effect_progress(effect, time)
        curve_value = self.evaluate_curve(effect.easing, progress, effect.custom_curve_points)
        max_radius = effect.properties.get('radius', 10)
        
        return int(max_radius * curve_value)
    
    def calculate_color_blend(self, effect: Effect, time: float, 
                             original_color: QColor) -> QColor:
        """Calculate blended color for color shift effects"""
        if effect.effect_type != EffectType.COLOR_SHIFT:
            return original_color
        
        progress = self.calculate_effect_progress(effect, time)
        curve_value = self.evaluate_curve(effect.easing, progress, effect.custom_curve_points)
        
        target_color_hex = effect.properties.get('color', '#FF0000')
        target_color = QColor(target_color_hex)
        intensity = effect.properties.get('intensity', 0.5) * curve_value
        
        # Blend colors
        r = int(original_color.red() * (1 - intensity) + target_color.red() * intensity)
        g = int(original_color.green() * (1 - intensity) + target_color.green() * intensity)
        b = int(original_color.blue() * (1 - intensity) + target_color.blue() * intensity)
        
        return QColor(r, g, b)
    
    def apply_effects_to_painter(self, painter, clip: Clip, time: float, 
                                base_color: QColor = QColor(255, 255, 255)):
        """
        Apply all active effects to a QPainter
        Returns modified color and opacity
        """
        active_effects = self.get_active_effects(clip, time)
        
        # Start with defaults
        opacity = 1.0
        x_offset = 0
        y_offset = 0
        scale = 1.0
        color = base_color
        
        # Apply each effect
        for effect in active_effects:
            if effect.effect_type in [EffectType.FADE_IN, EffectType.FADE_OUT]:
                fade = self.calculate_fade_value(effect, time)
                opacity *= fade  # Multiplicative for stacking
            
            elif effect.effect_type in [EffectType.SLIDE_LEFT, EffectType.SLIDE_RIGHT]:
                dx, dy = self.calculate_slide_offset(effect, time)
                x_offset += dx
                y_offset += dy
            
            elif effect.effect_type in [EffectType.ZOOM_IN, EffectType.ZOOM_OUT]:
                zoom = self.calculate_zoom_scale(effect, time)
                scale *= zoom  # Multiplicative for stacking
            
            elif effect.effect_type == EffectType.COLOR_SHIFT:
                color = self.calculate_color_blend(effect, time, color)
        
        # Apply transformations to painter
        painter.setOpacity(opacity)
        
        if x_offset != 0 or y_offset != 0:
            painter.translate(x_offset, y_offset)
        
        if scale != 1.0:
            painter.scale(scale, scale)
        
        return color, opacity
    
    def clear_cache(self):
        """Clear Bezier evaluation cache"""
        self._bezier_cache.clear()
