# Plugin Development Guide

Welcome to NC-KTV plugin development! This guide will help you create custom plugins to extend NC-KTV's functionality.

## Table of Contents

1. [Overview](#overview)
2. [Plugin Types](#plugin-types)
3. [Getting Started](#getting-started)
4. [Plugin Manifest](#plugin-manifest)
5. [Effect Plugins](#effect-plugins)
6. [Export Template Plugins](#export-template-plugins)
7. [Plugin API Reference](#plugin-api-reference)
8. [Packaging and Distribution](#packaging-and-distribution)
9. [Best Practices](#best-practices)
10. [Debugging Tips](#debugging-tips)

---

## Overview

NC-KTV plugins allow you to:
- Create custom video effects and animations
- Add custom export templates for different platforms
- Extend the UI with new features

### Architecture

```
NC-KTV
├── Plugin Manager (Discovery & Loading)
├── Plugin API (Safe access to app features)
└── Your Plugin
    ├── plugin.json (Manifest)
    ├── main.py (Implementation)
    └── assets/ (Optional resources)
```

---

## Plugin Types

### 1. Effect Plugins
Add custom visual effects to timeline clips.

**Use cases**: Particle effects, custom transitions, text animations

### 2. Export Template Plugins
Create custom subtitle/video export formats.

**Use cases**: Platform-specific formats (YouTube, TikTok), custom ASS styles

### 3. UI Extension Plugins
Add new widgets and tools to the interface.

**Use cases**: Custom editors, batch processors, analytics

---

## Getting Started

### Step 1: Create Plugin Directory

```powershell
mkdir plugins/installed/my-plugin
cd plugins/installed/my-plugin
```

### Step 2: Create Manifest

Create `plugin.json`:

```json
{
  "id": "com.yourname.my-plugin",
  "name": "My Awesome Plugin",
  "version": "1.0.0",
  "author": "Your Name",
  "description": "Brief description of what your plugin does",
  "type": "effect",
  "permissions": ["render"],
  "entry_point": "main.py",
  "dependencies": {
    "nc-ktv": ">=0.10.0"
  }
}
```

### Step 3: Implement Plugin

Create `main.py`:

```python
from core.plugin_base import EffectPlugin, Parameter, RenderContext
from typing import List, Dict, Any


class Plugin(EffectPlugin):
    def initialize(self, api) -> bool:
        self.api = api
        return True
    
    def cleanup(self):
        pass
    
    def get_name(self) -> str:
        return "My Effect"
    
    def get_parameters(self) -> List[Parameter]:
        return []
    
    def render_frame(self, context: RenderContext, params: Dict[str, Any]) -> Any:
        # Your rendering logic here
        return {}
```

### Step 4: Test Plugin

1. Open NC-KTV
2. Go to **Settings → Plugins**
3. Your plugin should appear in the list
4. Enable it and test functionality

---

## Plugin Manifest

The `plugin.json` file defines your plugin's metadata and requirements.

### Required Fields

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique identifier (reverse domain notation) |
| `name` | string | Display name |
| `version` | string | Semantic version (e.g., "1.0.0") |
| `author` | string | Your name or organization |
| `type` | string | Plugin type: "effect", "export_template", "ui_extension" |

### Optional Fields

| Field | Type | Description |
|-------|------|-------------|
| `description` | string | Brief description |
| `permissions` | array | Requested permissions |
| `entry_point` | string | Main file (default: "main.py") |
| `dependencies` | object | Required versions |

### Permissions

Request only the permissions you need:

- `file_read` - Read files from disk
- `file_write` - Write files to disk
- `gpu` - Access GPU for acceleration
- `network` - Make network requests
- `render` - Access rendering pipeline

---

## Effect Plugins

Effect plugins process video frames or add visual elements.

### Full Example

```python
from core.plugin_base import EffectPlugin, Parameter, RenderContext
from typing import List, Dict, Any


class Plugin(EffectPlugin):
    def initialize(self, api) -> bool:
        \"\"\"Called when plugin is loaded\"\"\"
        self.api = api
        self.api.log("My Effect initialized")
        return True
    
    def cleanup(self):
        \"\"\"Called when plugin is unloaded\"\"\"
        pass
    
    def get_name(self) -> str:
        \"\"\"Display name in effect browser\"\"\"
        return "Rainbow Pulse"
    
    def get_parameters(self) -> List[Parameter]:
        \"\"\"Define configurable parameters\"\"\"
        return [
            Parameter(
                name="speed",
                display_name="Pulse Speed",
                param_type="float",
                default_value=1.0,
                min_value=0.1,
                max_value=5.0,
                description="Speed of the rainbow pulse"
            ),
            Parameter(
                name="intensity",
                display_name="Intensity",
                param_type="float",
                default_value=0.5,
                min_value=0.0,
                max_value=1.0
            ),
            Parameter(
                name="enable_glow",
                display_name="Enable Glow",
                param_type="bool",
                default_value=True
            )
        ]
    
    def render_frame(self, context: RenderContext, params: Dict[str, Any]) -> Any:
        \"\"\"
        Render effect for a single frame
        
        Args:
            context: Frame information (time, dimensions, etc.)
            params: User-configured parameter values
        \"\"\"
        speed = params.get('speed', 1.0)
        intensity = params.get('intensity', 0.5)
        enable_glow = params.get('enable_glow', True)
        
        # Calculate rainbow hue based on time
        hue = (context.time * speed) % 1.0
        
        # Your rendering logic here
        # Return frame data or transformation
        
        return {
            'hue': hue,
            'intensity': intensity,
            'glow': enable_glow
        }
    
    def supports_gpu(self) -> bool:
        \"\"\"Whether this effect can use GPU acceleration\"\"\"
        return True  # If you implement GPU code
```

### Parameter Types

| Type | Description | Additional Fields |
|------|-------------|-------------------|
| `float` | Decimal number | `min_value`, `max_value` |
| `int` | Whole number | `min_value`, `max_value` |
| `bool` | True/False toggle | - |
| `color` | Color picker | - |
| `string` | Text input | - |
| `choice` | Dropdown selection | `choices` (array) |

---

## Export Template Plugins

Create custom export formats for different platforms.

### Example

```python
from core.plugin_base import ExportTemplatePlugin, Parameter, ExportContext
from typing import List, Dict, Any
from pathlib import Path


class Plugin(ExportTemplatePlugin):
    def initialize(self, api) -> bool:
        self.api = api
        return True
    
    def cleanup(self):
        pass
    
    def get_template_name(self) -> str:
        return "YouTube Shorts"
    
    def get_description(self) -> str:
        return "Optimized format for YouTube Shorts (9:16 vertical)"
    
    def get_parameters(self) -> List[Parameter]:
        return [
            Parameter(
                name="font_size",
                display_name="Font Size",
                param_type="int",
                default_value=72,
                min_value=24,
                max_value=120
            ),
            Parameter(
                name="position",
                display_name="Text Position",
                param_type="choice",
                default_value="center",
                choices=["top", "center", "bottom"]
            )
        ]
    
    def generate_subtitle_file(self, context: ExportContext, params: Dict[str, Any]) -> str:
        \"\"\"
        Generate ASS subtitle file
        
        Returns:
            Path to generated .ass file
        \"\"\"
        font_size = params.get('font_size', 72)
        position = params.get('position', 'center')
        
        # Generate ASS content
        ass_content = self._create_ass(context, font_size, position)
        
        # Write to file
        output_path = Path(context.output_path).parent / "subtitles.ass"
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(ass_content)
        
        return str(output_path)
    
    def _create_ass(self, context, font_size, position):
        # Your ASS generation logic
        return f\"\"\"[Script Info]
Title: YouTube Shorts
...
\"\"\"
```

---

## Plugin API Reference

The `PluginAPI` object gives you safe access to NC-KTV features.

### Configuration

```python
# Get config value
value = self.api.get_config_value('export.video_codec', 'libx264')

# Set config value (plugin namespace only)
self.api.set_config_value('my_setting', 'value')
```

### Project Access

```python
# Get current project
project = self.api.get_current_project()
if project:
    print(f"Project: {project.name}")
```

### Logging

```python
# Log messages
self.api.log("Info message", level="info")
self.api.log("Warning!", level="warning")
self.api.log("Error occurred", level="error")
self.api.log("Debug info", level="debug")
```

### User Interaction

```python
# Show message dialog
self.api.show_message("Success", "Operation completed!", msg_type="info")
self.api.show_message("Warning", "Check your settings", msg_type="warning")
self.api.show_message("Error", "Something went wrong", msg_type="error")
```

---

## Packaging and Distribution

### Create `.nckplugin` Package

1. **Prepare Files**:
   ```
   my-plugin/
   ├── plugin.json
   ├── main.py
   ├── README.md (optional)
   └── assets/ (optional)
   ```

2. **Create ZIP**:
   ```powershell
   Compress-Archive -Path my-plugin\* -DestinationPath my-plugin.nckplugin
   ```

3. **Distribute**:
   - Share the `.nckplugin` file
   - Users install via **Settings → Plugins → Install Plugin**

### Version Your Plugin

Use semantic versioning: `MAJOR.MINOR.PATCH`

- **MAJOR**: Breaking changes
- **MINOR**: New features (backward compatible)
- **PATCH**: Bug fixes

---

## Best Practices

### 1. Error Handling

```python
def render_frame(self, context, params):
    try:
        # Your code here
        return result
    except Exception as e:
        self.api.log(f"Render error: {e}", level="error")
        return {}  # Return safe fallback
```

### 2. Resource Management

```python
def initialize(self, api):
    self.api = api
    self.resources = []  # Track resources
    return True

def cleanup(self):
    # Clean up resources
    for resource in self.resources:
        resource.release()
```

### 3. Performance

- Use GPU acceleration when possible
- Cache expensive computations
- Profile your code
- Log performance metrics

### 4. Testing

- Test with various project types
- Test enable/disable cycles
- Test parameter edge cases
- Test with GPU and CPU modes

---

## Debugging Tips

### Enable Debug Logging

```python
def initialize(self, api):
    self.api = api
    api.log("Plugin initialized", level="debug")
    api.log(f"Parameters: {self.get_parameters()}", level="debug")
    return True
```

### Check Plugin Status

1. Open **Settings → Plugins**
2. Check if plugin appears in list
3. Enable and check logs
4. Test functionality

### Common Issues

**Plugin Not Loading**:
- Check `plugin.json` syntax (use JSON validator)
- Verify `id` is unique
- Check `entry_point` path

**Import Errors**:
- Ensure all imports are available
- Check Python version compatibility
- Verify NC-KTV version requirement

**Effect Not Appearing**:
- Check plugin is enabled
- Verify plugin type matches usage
- Restart NC-KTV

---

## Next Steps

1. Browse `plugins/examples/` for more examples
2. Join the community to share plugins
3. Read the [Theme Creation Guide](THEME_CREATION.md)
4. Contribute to the NC-KTV ecosystem!

---

**Happy Plugin Development! 🎉**

For questions and support, visit the NC-KTV repository: https://github.com/AgentHitmanFaris/NC-KTV
