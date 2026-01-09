"""
Hello World Effect Plugin
Example plugin demonstrating the effect plugin interface
"""

from core.plugin_base import EffectPlugin, Parameter, RenderContext, PluginMetadata
from typing import List, Dict, Any


class Plugin(EffectPlugin):
    """Simple hello world effect"""
    
    def initialize(self, api) -> bool:
        """Initialize the plugin"""
        self.api = api
        self.api.log(f"Hello Effect initialized: {self.metadata.name}")
        return True
    
    def cleanup(self):
        """Cleanup resources"""
        self.api.log("Hello Effect cleaned up")
    
    def get_name(self) -> str:
        """Get effect name"""
        return "Hello World"
    
    def get_parameters(self) -> List[Parameter]:
        """Get effect parameters"""
        return [
            Parameter(
                name="text",
                display_name="Display Text",
                param_type="string",
                default_value="Hello, World!",
                description="Text to display"
            ),
            Parameter(
                name="size",
                display_name="Font Size",
                param_type="int",
                default_value=48,
                min_value=12,
                max_value=120,
                description="Text font size"
            ),
            Parameter(
                name="color",
                display_name="Text Color",
                param_type="color",
                default_value="#FFD700",
                description="Text color"
            )
        ]
    
    def render_frame(self, context: RenderContext, params: Dict[str, Any]) -> Any:
        """
        Render the effect for a single frame
        
        In a real implementation, this would return frame data.
        For this example, we just log the parameters.
        """
        text = params.get('text', 'Hello, World!')
        size = params.get('size', 48)
        color = params.get('color', '#FFD700')
        
        # Log effect application (in real plugin, would manipulate pixels)
        self.api.log(
            f"Rendering Hello Effect: '{text}' at size {size} with color {color}",
            level="debug"
        )
        
        # Return placeholder (in real implementation, return frame data)
        return {
            'text': text,
            'size': size,
            'color': color,
            'time': context.time
        }
    
    def supports_gpu(self) -> bool:
        """This effect doesn't require GPU"""
        return False
