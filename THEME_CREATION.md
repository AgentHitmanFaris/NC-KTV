# Theme Creation Guide

Create custom themes to personalize NC-KTV's appearance and karaoke video styling!

## Table of Contents

1. [Overview](#overview)
2. [Theme File Format](#theme-file-format)
3. [UI Styling](#ui-styling)
4. [Karaoke Styles](#karaoke-styles)
5. [Color Guidelines](#color-guidelines)
6. [Packaging and Sharing](#packaging-and-sharing)
7. [Examples](#examples)

---

## Overview

NC-KTV themes control:
- **UI Colors**: Application interface colors
- **Fonts**: Typography settings
- **Widget Styles**: Button, dialog, and component styling
- **Karaoke Styles**: Video export subtitle styling

### Quick Start

1. Create `mytheme.yaml` in `themes/` directory
2. Define metadata, UI colors, and karaoke styles
3. Load in **Settings → Themes**

---

## Theme File Format

Themes are defined in YAML format with three main sections:

```yaml
metadata:
  # Theme information
  
ui:
  # Application interface styling
  
karaoke:
  # Video export styles
```

### Complete Template

```yaml
metadata:
  id: "my-theme"
  name: "My Awesome Theme"
  author: "Your Name"
  version: "1.0.0"
  description: "Brief description of your theme"

ui:
  colors:
    background: "#1a1a1a"
    foreground: "#ffffff"
    primary: "#2196F3"
    accent: "#FF9800"
    success: "#4CAF50"
    warning: "#FF9800"
    error: "#F44336"
  
  fonts:
    family: "Arial"
    size: 10
  
  styles:
    button_primary: "background-color: {colors.success}; color: white; font-weight: bold;"
    button_export: "background-color: #E91E63; color: white; font-weight: bold;"
    button_auto: "background-color: #673AB7; color: white; font-weight: bold;"

karaoke:
  styles:
    - name: "My Style"
      font_family: "Arial"
      font_size: 60
      active_color: "#FFD700"
      inactive_color: "#FFFFFF"
      outline_color: "#000000"
      outline_width: 2.0
      animation: "linear_wipe"
```

---

## UI Styling

### Color Palette

Define a consistent color scheme for the interface.

#### Required Colors

| Color | Usage |
|-------|-------|
| `background` | Window backgrounds, panels |
| `foreground` | Text and icons |
| `primary` | Primary actions, highlights |
| `accent` | Secondary highlights |
| `success` | Success states, confirmations |
| `warning` | Warnings, cautions |
| `error` | Errors, destructive actions |

#### Example Palettes

**Dark Theme**:
```yaml
colors:
  background: "#1a1a1a"
  foreground: "#ffffff"
  primary: "#2196F3"
  accent: "#FF9800"
  success: "#4CAF50"
  warning: "#FF9800"
  error: "#F44336"
```

**Light Theme**:
```yaml
colors:
  background: "#f5f5f5"
  foreground: "#212121"
  primary: "#1976D2"
  accent: "#FFA000"
  success: "#388E3C"
  warning: "#F57C00"
  error: "#D32F2F"
```

**Cyberpunk Theme**:
```yaml
colors:
  background: "#0a0e27"
  foreground: "#00ff9f"
  primary: "#ff006e"
  accent: "#8338ec"
  success: "#00f5d4"
  warning: "#ffbe0b"
  error: "#ff006e"
```

### Fonts

Customize typography:

```yaml
fonts:
  family: "Segoe UI"  # System font
  size: 10            # Base font size
```

**Recommended Fonts**:
- **Sans-Serif**: Arial, Helvetica, Segoe UI, Roboto
- **Monospace**: Consolas, Courier New, Monaco
- **Modern**: Inter, Poppins, Montserrat

### Widget Styles

Define Qt stylesheets for specific UI elements.

#### Available Style Keys

| Key | Description |
|-----|-------------|
| `button_primary` | Primary action buttons |
| `button_export` | Export button |
| `button_auto` | Auto-transcribe button |

#### Style Templates

Use `{colors.name}` placeholders to reference your color palette:

```yaml
styles:
  button_primary: |
    background-color: {colors.success};
    color: white;
    font-weight: bold;
    border-radius: 5px;
    padding: 8px 16px;
  
  button_export: |
    background-color: {colors.accent};
    color: white;
    font-weight: bold;
    border: none;
```

---

## Karaoke Styles

Define how lyrics appear in exported videos.

### Style Properties

| Property | Type | Description |
|----------|------|-------------|
| `name` | string | Display name for the style |
| `font_family` | string | Font to use (e.g., "Arial") |
| `font_size` | int | Font size in points |
| `active_color` | hex | Color of sung lyrics |
| `inactive_color` | hex | Color of upcoming lyrics |
| `outline_color` | hex | Text outline color |
| `outline_width` | float | Outline thickness |
| `animation` | string | Animation type |

### Animation Types

- `linear_wipe` - Classic left-to-right fill
- `syllable_step` - Instant word fill
- `glow_pulse` - Pulsing glow effect
- `fade_in` - Fade from transparent
- `bouncing_ball` - Classic karaoke ball

### Multiple Styles

You can define multiple karaoke styles in one theme:

```yaml
karaoke:
  styles:
    - name: "Classic Gold"
      font_family: "Arial"
      font_size: 60
      active_color: "#FFD700"
      inactive_color: "#FFFFFF"
      outline_color: "#000000"
      outline_width: 2.0
      animation: "linear_wipe"
    
    - name: "Neon Glow"
      font_family: "Arial"
      font_size: 64
      active_color: "#00FFFF"
      inactive_color: "#FFFFFF"
      outline_color: "#FF00FF"
      outline_width: 3.0
      animation: "glow_pulse"
    
    - name: "Minimalist"
      font_family: "Helvetica"
      font_size: 48
      active_color: "#333333"
      inactive_color: "#CCCCCC"
      outline_color: "#FFFFFF"
      outline_width: 1.5
      animation: "fade_in"
```

---

## Color Guidelines

### Contrast Ratios

Ensure text is readable:

- **Normal text**: 4.5:1 minimum
- **Large text**: 3:1 minimum
- **UI elements**: 3:1 minimum

Use a [contrast checker](https://webaim.org/resources/contrastchecker/) to verify.

### Color Psychology

| Color | Emotion | Use For |
|-------|---------|---------|
| Blue | Trust, calm | Primary actions |
| Green | Success, growth | Confirmations |
| Red | Energy, urgency | Errors, destructive |
| Orange | Warning, attention | Warnings |
| Purple | Creativity | Special features |
| Yellow | Optimism | Highlights |

### Accessibility

**Color Blindness Considerations**:
- Don't rely solely on color
- Provide text labels
- Use patterns/shapes
- Test with simulators

---

## Packaging and Sharing

### Create `.ncktheme` Package

1. **Prepare Files**:
   ```
   mytheme/
   ├── theme.yaml
   ├── preview.png (optional)
   └── assets/ (optional fonts, images)
   ```

2. **Create ZIP Package**:
   ```powershell
   Compress-Archive -Path mytheme\* -DestinationPath mytheme.ncktheme
   ```

3. **Share**:
   - Upload to community forum
   - Share on GitHub
   - Distribute directly

### Installation

Users install themes via:
1. **Settings → Themes**
2. Click **Import...**
3. Select `.ncktheme` file
4. Apply the theme

---

## Examples

### Example 1: Dracula Theme

```yaml
metadata:
  id: "dracula"
  name: "Dracula"
  author: "Community"
  version: "1.0.0"
  description: "Dark theme with vibrant colors"

ui:
  colors:
    background: "#282a36"
    foreground: "#f8f8f2"
    primary: "#bd93f9"
    accent: "#ff79c6"
    success: "#50fa7b"
    warning: "#ffb86c"
    error: "#ff5555"
  
  fonts:
    family: "Fira Code"
    size: 10
  
  styles:
    button_primary: "background-color: {colors.success}; color: #282a36; font-weight: bold;"
    button_export: "background-color: {colors.accent}; color: white; font-weight: bold;"

karaoke:
  styles:
    - name: "Dracula Karaoke"
      font_family: "Arial"
      font_size: 60
      active_color: "#ff79c6"
      inactive_color: "#f8f8f2"
      outline_color: "#bd93f9"
      outline_width: 2.5
      animation: "glow_pulse"
```

### Example 2: Minimal Light

```yaml
metadata:
  id: "minimal-light"
  name: "Minimal Light"
  author: "Community"
  version: "1.0.0"
  description: "Clean, minimal light theme"

ui:
  colors:
    background: "#ffffff"
    foreground: "#333333"
    primary: "#0066cc"
    accent: "#ff6600"
    success: "#00aa44"
    warning: "#ff9900"
    error: "#cc0000"
  
  fonts:
    family: "Inter"
    size: 10
  
  styles:
    button_primary: |
      background-color: {colors.primary};
      color: white;
      font-weight: 500;
      border: none;
      border-radius: 4px;
      padding: 6px 12px;

karaoke:
  styles:
    - name: "Clean Subtitle"
      font_family: "Helvetica"
      font_size: 48
      active_color: "#0066cc"
      inactive_color: "#666666"
      outline_color: "#ffffff"
      outline_width: 2.0
      animation: "linear_wipe"
```

### Example 3: Retro 80s

```yaml
metadata:
  id: "retro-80s"
  name: "Retro 80s"
  author: "Community"
  version: "1.0.0"
  description: "Neon vibes from the 1980s"

ui:
  colors:
    background: "#1a1a2e"
    foreground: "#eaeaea"
    primary: "#ff00ff"
    accent: "#00ffff"
    success: "#00ff00"
    warning: "#ffff00"
    error: "#ff0066"
  
  fonts:
    family: "Press Start 2P"
    size: 10

karaoke:
  styles:
    - name: "80s Neon"
      font_family: "Impact"
      font_size: 72
      active_color: "#ff00ff"
      inactive_color: "#00ffff"
      outline_color: "#000000"
      outline_width: 4.0
      animation: "glow_pulse"
```

---

## Testing Your Theme

1. Save `mytheme.yaml` to `themes/` directory
2. Open NC-KTV
3. Go to **Settings → Themes**
4. Select your theme from the list
5. Click **Apply**
6. Test UI appearance
7. Export a test video to check karaoke styles

---

## Best Practices

1. **Start Simple**: Begin with built-in themes as templates
2. **Test Thoroughly**: Check both light and dark environments
3. **Document Choices**: Add comments in YAML for complex styles
4. **Version Properly**: Update version for each release
5. **Get Feedback**: Share with community for testing

---

**Happy Theming! 🎨**

For theme showcase and sharing, visit: https://github.com/AgentHitmanFaris/NC-KTV
