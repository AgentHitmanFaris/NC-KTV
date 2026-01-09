# Hello World Effect Plugin

This is an example plugin demonstrating the NC-KTV effect plugin system.

## What it does

Displays customizable text with configurable size and color.

## Parameters

- **Display Text**: The text to show (default: "Hello, World!")
- **Font Size**: Size of the text (12-120, default: 48)  
- **Text Color**: Color of the text (default: #FFD700 - Gold)

## Installation

1. Copy the entire `hello_effect` folder to `plugins/installed/`
2. Restart NC-KTV or click "Reload All" in Plugin Manager
3. Enable the plugin in Settings → Plugins

## Usage

1. Open a project in the editor
2. Add a clip to the timeline
3. Right-click → Add Effect → Hello World
4. Configure parameters in the effect panel

## Development

This plugin demonstrates:
- Plugin manifest (`plugin.json`)
- Effect plugin interface implementation
- Parameter definitions (string, int, color)
- Basic logging via plugin API

Use this as a template for creating your own effects!

## License

MIT License - Feel free to modify and distribute
