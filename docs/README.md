# Documentation

This directory contains additional documentation for NC-KTV.

## Setup Guides

- **[SETUP.md](../SETUP.md)** - Complete setup instructions for Python embedded environment
- **[PYTORCH_MANUAL_INSTALL.md](../PYTORCH_MANUAL_INSTALL.md)** - Manual PyTorch installation guide for offline setup

## Development Documentation

- **[CONTRIBUTING.md](../CONTRIBUTING.md)** - Guidelines for contributing to NC-KTV
- **[CHANGELOG.md](../CHANGELOG.md)** - Version history and release notes

## Scripts

Located in the root directory:

- **setup_python.ps1** - Automated Python environment setup script
- **install_torch_manual.ps1** - Manual PyTorch wheel installation helper
- **migrate_uvr_models.ps1** - UVR model migration from existing installation

## Project Structure

```
NC-KTV/
├── docs/                    # Additional documentation
├── models/                  # UVR model files (.onnx, .pth)
├── python_embed/           # Python 3.10.11 embedded distribution
├── src/                    # Source code
│   ├── core/              # Core processing logic
│   ├── gui/               # PyQt6 GUI components
│   ├── sync/              # Lyrics synchronization
│   ├── utils/             # Utilities and helpers
│   └── workers/           # Background processing workers
├── temp/                   # Temporary processing files
├── output/                 # Generated output files
├── config.yaml            # Application configuration
├── main.py                # Application entry point
└── requirements.txt       # Python dependencies
```

## Configuration

See [config.yaml](../config.yaml) for available settings:

- UVR model selection and paths
- Processing parameters (sample rate, output directory)
- Lyrics synchronization options
- GUI preferences (theme, startup mode)
- Advanced settings (batch processing, caching)

## API Documentation

Coming in future releases:

- Python API reference
- Plugin development guide
- Custom model integration guide

## FAQ

**Q: Why does the app freeze during processing?**  
A: Processing runs in background threads, but very large files may cause temporary UI lag. This will be optimized in future releases.

**Q: Can I use my own UVR models?**  
A: Yes! Place any `.onnx` or `.pth` UVR model files in the `models/` directory. They will appear in the model selection dropdown.

**Q: Does this work on Linux/Mac?**  
A: Currently Windows-only. Cross-platform support is planned for future releases.

**Q: How do I report bugs?**  
A: Open an issue on [GitHub](https://github.com/AgentHitmanFaris/NC-KTV/issues) with steps to reproduce, error messages, and system info.

## Resources

### External Documentation

- [UVR Model Repository](https://github.com/TRvlvr/model_repo)
- [audio-separator Documentation](https://github.com/nomadkaraoke/python-audio-separator)
- [PyTorch Documentation](https://pytorch.org/docs/stable/index.html)
- [PyQt6 Documentation](https://www.riverbankcomputing.com/static/Docs/PyQt6/)

### Video Tutorials

Coming soon!

## License

NC-KTV is licensed under the MIT License. See [LICENSE](../LICENSE) for details.
