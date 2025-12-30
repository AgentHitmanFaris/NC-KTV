# Contributing to NC-KTV

Thank you for your interest in contributing to NC-KTV! This document provides guidelines and instructions for contributing.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [How to Contribute](#how-to-contribute)
- [Coding Standards](#coding-standards)
- [Commit Guidelines](#commit-guidelines)
- [Pull Request Process](#pull-request-process)
- [Testing](#testing)

---

## 📜 Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inclusive environment for everyone, regardless of background or identity.

### Expected Behavior

- Be respectful and considerate
- Welcome newcomers and help them get started
- Focus on constructive criticism
- Acknowledge different viewpoints and experiences

### Unacceptable Behavior

- Harassment, discrimination, or offensive comments
- Personal attacks or trolling
- Publishing others' private information
- Any conduct that could reasonably be considered inappropriate

---

## 🚀 Getting Started

### Prerequisites

- Windows 10/11 (64-bit)
- Python 3.10.11
- Git
- FFmpeg
- NVIDIA GPU with CUDA support (optional but recommended)

### Areas to Contribute

1. **🐛 Bug Fixes** - Help identify and fix bugs
2. **✨ Features** - Implement planned features from roadmap
3. **📝 Documentation** - Improve docs, add tutorials
4. **🌍 Localization** - Translate UI to other languages
5. **🎨 UI/UX** - Enhance user interface and experience
6. **⚡ Performance** - Optimize processing speed
7. **🧪 Testing** - Write unit/integration tests

---

## 💻 Development Setup

### 1. Fork and Clone

```powershell
# Fork the repository on GitHub, then clone your fork
git clone https://github.com/YOUR_USERNAME/NC-KTV.git
cd NC-KTV

# Add upstream remote
git remote add upstream https://github.com/AgentHitmanFaris/NC-KTV.git
```

### 2. Set Up Environment

```powershell
# Download Python 3.10.11 embedded
# Extract to python_embed/

# Run setup
.\setup_python.ps1
```

### 3. Download Models

```powershell
# Place UVR models in models/ directory
# See README.md for model download links
```

### 4. Create Feature Branch

```powershell
# Update your fork
git checkout Stable
git pull upstream Stable

# Create feature branch
git checkout -b feature/your-feature-name
```

---

## 🔧 How to Contribute

### Reporting Bugs

**Before submitting:**
1. Check existing issues to avoid duplicates
2. Verify the bug on the latest `Stable` branch

**Bug report should include:**
- Clear, descriptive title
- Steps to reproduce
- Expected vs actual behavior
- Error messages/screenshots
- System information (OS, Python version, GPU)

### Suggesting Features

**Feature requests should include:**
- Clear description of the feature
- Use case and benefits
- Possible implementation approach
- Links to related features/tools

### Contributing Code

1. **Pick an issue** - Comment to claim it
2. **Discuss approach** - For large changes, discuss first
3. **Write code** - Follow coding standards
4. **Test thoroughly** - Ensure nothing breaks
5. **Submit PR** - Follow PR template

---

## 📐 Coding Standards

### Python Style

Follow [PEP 8](https://pep8.org/) with these specifics:

```python
# Imports
import standard_library
import third_party
from project import module

# Line length: 100 characters max
# Indentation: 4 spaces

# Type hints
def process_audio(file_path: Path, sample_rate: int = 44100) -> dict:
    """Process audio file.
    
    Args:
        file_path: Path to audio file
        sample_rate: Target sample rate
        
    Returns:
        Dictionary with processing results
    """
    pass

# Class naming: PascalCase
class AudioProcessor:
    pass

# Function/variable naming: snake_case
def extract_audio_from_video():
    audio_path = Path("output/audio.wav")
```

### Qt/GUI Code

```python
# Signal naming: past_tense_verb
processing_complete = pyqtSignal(dict)
error_occurred = pyqtSignal(str)

# Slot naming: _on_signal_name
def _on_processing_complete(self, result: dict):
    pass

# Private methods: _leading_underscore
def _update_progress(self):
    pass
```

### Documentation

```python
# Docstrings for all public functions/classes
def separate_vocals(audio_path: Path, model_name: str) -> dict:
    """Separate vocals from audio using UVR model.
    
    Args:
        audio_path: Path to input audio file
        model_name: UVR model filename
        
    Returns:
        Dictionary with paths to separated stems:
        {
            'instrumental': Path,
            'vocals': Path,
            'model_used': str
        }
        
    Raises:
        FileNotFoundError: If audio file doesn't exist
        RuntimeError: If separation fails
    """
```

---

## 💾 Commit Guidelines

### Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style (formatting, no logic change)
- `refactor`: Code refactoring
- `perf`: Performance improvement
- `test`: Adding/updating tests
- `chore`: Build process, dependencies

### Examples

```bash
feat(vocal-remover): add support for Demucs models

- Add Demucs model detection
- Implement Demucs-specific processing
- Update model selection UI

Closes #42

fix(wizard): prevent crash when no models found

Previously crashed with IndexError when models/ directory empty.
Now shows friendly error message.

Fixes #15

docs(readme): update installation instructions

Add troubleshooting section for common FFmpeg issues
```

---

## 🔀 Pull Request Process

### Before Submitting

1. **Update from upstream**
   ```powershell
   git checkout Stable
   git pull upstream Stable
   git checkout feature/your-feature
   git rebase Stable
   ```

2. **Test your changes**
   ```powershell
   python_embed\python.exe main.py
   pytest tests/  # If tests exist
   ```

3. **Update documentation**
   - Update README.md if needed
   - Add/update docstrings
   - Update CHANGELOG.md under [Unreleased]

### PR Requirements

- [ ] Descriptive title and description
- [ ] Linked to related issue (#123)
- [ ] Code follows style guidelines
- [ ] Tests pass (when implemented)
- [ ] Documentation updated
- [ ] No merge conflicts
- [ ] Screenshots for UI changes

### PR Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation update
- [ ] Performance improvement

## Related Issue
Closes #(issue number)

## Testing
How to test these changes

## Screenshots (if UI changes)
Before/After screenshots

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-reviewed code
- [ ] Commented complex code
- [ ] Updated documentation
- [ ] No new warnings
```

---

## 🧪 Testing

### Manual Testing

```powershell
# Test basic workflow
python_embed\python.exe main.py

# Test with different file formats
# Test GPU vs CPU mode
# Test error handling (invalid files, missing models)
```

### Unit Tests (Coming Soon)

```powershell
# Run tests
pytest tests/ -v

# Run specific test
pytest tests/test_vocal_remover.py -v

# Generate coverage report
pytest --cov=src tests/
```

### Testing Guidelines

- Test happy path and error cases
- Test with various file formats
- Test with/without GPU
- Test cancellation during processing
- Verify no memory leaks with long sessions

---

## 📞 Communication

- **GitHub Issues** - Bug reports, feature requests
- **GitHub Discussions** - Questions, ideas
- **Pull Requests** - Code reviews, feedback

---

## 🏆 Recognition

Contributors will be recognized in:
- README.md acknowledgments
- CHANGELOG.md for their contributions
- GitHub contributor graph

---

## ❓ Questions?

If you have questions about contributing, feel free to:
- Open a discussion on GitHub
- Comment on relevant issues
- Reach out to maintainers

Thank you for contributing to NC-KTV! 🎉
