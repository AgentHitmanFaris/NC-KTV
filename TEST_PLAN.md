# NC-KTV Comprehensive Test Plan

**Version**: 0.10.0  
**Last Updated**: 2026-01-07  
**Status**: Active Testing

---

## Table of Contents

1. [Overview](#overview)
2. [Test Strategy](#test-strategy)
3. [Test Categories](#test-categories)
4. [Security Testing](#security-testing)
5. [Test Execution](#test-execution)
6. [Test Coverage](#test-coverage)

---

## Overview

### Objectives

- **Functional Validation**: Verify all features work as designed
- **Security Hardening**: Identify and prevent vulnerabilities
- **Stability Testing**: Ensure app doesn't crash under stress
- **Edge Case Handling**: Test boundary conditions
- **Performance**: Validate resource usage

### Scope

**In Scope**:
- All core features (Phases 1-7)
- Plugin and theme systems
- Export pipeline
- AI integration (UVR, Whisper)
- Project serialization
- UI components

**Out of Scope**:
- Third-party library internals (FFmpeg, PyTorch)
- Operating system specific bugs
- Hardware driver issues

---

## Test Strategy

### Test Pyramid

```
        ┌──────────┐
        │   UI     │ 10% - End-to-end tests
        ├──────────┤
        │Integration│ 30% - Component interaction
        ├──────────┤
        │   Unit   │ 60% - Individual functions
        └──────────┘
```

### Test Types

1. **Unit Tests**: Test individual functions and classes
2. **Integration Tests**: Test component interactions
3. **Security Tests**: Test for vulnerabilities
4. **Stress Tests**: Test performance under load
5. **Fuzzing Tests**: Test with random/malformed inputs
6. **UI Tests**: Test user interface components

### Tools

- **pytest**: Primary test framework
- **hypothesis**: Property-based testing
- **pytest-qt**: Qt widget testing
- **pytest-cov**: Coverage reporting
- **bandit**: Security linting
- **safety**: Dependency vulnerability checking

---

## Test Categories

### 1. Unit Tests

#### 1.1 Configuration System (`src/utils/config.py`)

**Test Cases**:
- ✓ Load default configuration
- ✓ Load configuration from file
- ✓ Save configuration to file
- ✓ Get nested config values with dot notation
- ✓ Set nested config values
- ✓ Handle missing config file gracefully
- ✓ Handle corrupted YAML
- ✓ Validate type conversions
- ✓ Handle unicode in config values

**Edge Cases**:
- Empty config file
- Very large config file (>10MB)
- Config with circular references
- Config with special characters
- Config with extremely long keys (>1000 chars)

#### 1.2 Plugin Manager (`src/core/plugin_manager.py`)

**Test Cases**:
- ✓ Discover plugins in directories
- ✓ Load valid plugin
- ✓ Reject invalid plugin manifest
- ✓ Handle missing entry point
- ✓ Check permissions correctly
- ✓ Enable/disable plugin
- ✓ Unload plugin cleanly
- ✓ Install .nckplugin package
- ✓ Reject malformed packages
- ✓ Handle plugin initialization failure

**Security Tests**:
- Path traversal in plugin packages
- Malicious __init__.py execution
- Plugin trying to access parent directory
- Plugin trying to import restricted modules
- Plugin with extremely long ID
- Plugin manifest with billion laughs attack

#### 1.3 Theme Manager (`src/utils/theme_manager.py`)

**Test Cases**:
- ✓ Load YAML theme
- ✓ Parse color palette
- ✓ Substitute color variables
- ✓ Create built-in themes
- ✓ Install .ncktheme package
- ✓ Uninstall theme
- ✓ Set active theme
- ✓ Handle missing theme file
- ✓ Handle invalid YAML

**Security Tests**:
- YAML bombs (deeply nested structures)
- Billion laughs attack
- Path traversal in theme packages
- Malicious YAML with code execution
- Extremely large theme files (>100MB)

#### 1.4 Project Serialization (`src/core/project.py`)

**Test Cases**:
- ✓ Save project to .nctv file
- ✓ Load project from .nctv file
- ✓ Handle missing files gracefully
- ✓ Validate project data
- ✓ Migrate old versions
- ✓ Compress large projects
- ✓ Handle encryption correctly

**Security Tests**:
- Buffer overflow in project data
- Malicious pickle payloads
- Zip bombs in project files
- Path traversal in file references
- Extremely deep JSON nesting

#### 1.5 Audio Processing (`src/core/audio_processor.py`)

**Test Cases**:
- ✓ Load various audio formats
- ✓ Convert sample rates
- ✓ Handle mono/stereo
- ✓ Process short clips (<1s)
- ✓ Process long clips (>1hr)
- ✓ Handle corrupted audio files

**Edge Cases**:
- Zero-length audio
- Audio with extreme sample rates (1Hz, 384kHz)
- Audio with silence
- Audio with clipping

#### 1.6 Lyrics Data (`src/sync/sync_data.py`)

**Test Cases**:
- ✓ Add lyrics lines
- ✓ Sort by timestamp
- ✓ Handle word tokens
- ✓ Serialize/deserialize
- ✓ Import from various formats

**Edge Cases**:
- Empty lyrics
- 10,000+ lyrics lines
- Unicode lyrics (Chinese, Arabic, Emoji)
- Lines with negative timestamps
- Lines with duplicate timestamps

#### 1.7 ASS Generator (`src/utils/ass_generator.py`)

**Test Cases**:
- ✓ Generate valid ASS file
- ✓ Apply styles correctly
- ✓ Handle karaoke tags
- ✓ Format timestamps
- ✓ Escape special characters

**Edge Cases**:
- Lyrics with ASS control codes
- Extremely long lines (>1000 chars)
- Special characters in lyrics
- Unicode edge cases

---

### 2. Integration Tests

#### 2.1 Plugin Loading and Execution

**Scenarios**:
- Load multiple plugins simultaneously
- Enable plugin → Use plugin → Disable plugin
- Update plugin while enabled
- Plugin communication via API
- Plugin dependency resolution

#### 2.2 Theme Application

**Scenarios**:
- Apply theme → Verify UI updates
- Switch themes without restart
- Theme + Plugin interaction
- Theme with missing colors

#### 2.3 Export Pipeline

**Scenarios**:
- Full export: Audio → UVR → Lyrics → Video
- Export with plugins enabled
- Export with custom theme
- Export to different formats
- Export with missing files

#### 2.4 Project Lifecycle

**Scenarios**:
- Create → Edit → Save → Load → Export
- Autosave functionality
- Crash recovery
- Concurrent project access

---

### 3. Security Testing

#### 3.1 Input Validation

**Attack Vectors**:

**Path Traversal**:
```python
# Malicious plugin package
{
  "id": "../../../etc/passwd",
  "entry_point": "../../../malicious.py"
}
```

**Command Injection**:
```python
# Malicious project file
{
  "source_file": "; rm -rf / #",
  "instrumental_path": "$(malicious_command)"
}
```

**YAML Bombs**:
```yaml
# Billion laughs attack
a: &a ["a","a","a","a","a","a","a","a","a"]
b: &b [*a,*a,*a,*a,*a,*a,*a,*a,*a]
c: &c [*b,*b,*b,*b,*b,*b,*b,*b,*b]
d: &d [*c,*c,*c,*c,*c,*c,*c,*c,*c]
```

**Buffer Overflow**:
```python
# Extremely long strings
plugin_id = "A" * 1000000
theme_name = "B" * 10000000
```

**XML/JSON Injection**:
```json
{
  "lyrics": "'; DROP TABLE users; --"
}
```

#### 3.2 File Upload Security

**Tests**:
- Upload .nckplugin with executable
- Upload .ncktheme with malicious YAML
- Upload project file with path traversal
- Upload ZIP bomb
- Upload file with wrong extension

#### 3.3 Dependency Security

**Tests**:
- Check for known CVEs in dependencies
- Validate PyPI package integrity
- Test with outdated dependencies

---

### 4. Stress Testing

#### 4.1 Resource Limits

**Tests**:
- **Memory**: Load 1000 plugins
- **CPU**: Process 10 hour audio file
- **Disk**: Save 100GB project
- **Network**: Download large models

#### 4.2 Concurrent Operations

**Tests**:
- Multiple exports simultaneously
- Multiple projects open
- Rapid enable/disable of plugins
- Spam theme switching

#### 4.3 Long-Running Operations

**Tests**:
- 24-hour stress test
- Memory leak detection
- File handle leaks
- Thread leak detection

---

### 5. Edge Case Testing

#### 5.1 Invalid Inputs

**Tests**:
- Null/None values everywhere
- Empty strings
- Whitespace-only inputs
- Extremely long inputs (>1MB)
- Unicode edge cases (surrogate pairs, RTL text)

#### 5.2 File System Edge Cases

**Tests**:
- No write permissions
- Disk full
- File locked by another process
- Symbolic links
- Network drives
- Read-only file system

#### 5.3 System Limits

**Tests**:
- Maximum file descriptor limit
- Maximum thread count
- Maximum memory allocation
- Maximum path length (Windows: 260 chars)

---

### 6. Fuzzing Tests

#### 6.1 Config File Fuzzing

```python
# Generate random YAML
- Random keys with special chars
- Random nested structures
- Random data types
- Malformed YAML syntax
```

#### 6.2 Plugin Manifest Fuzzing

```python
# Mutate plugin.json
- Random JSON structures
- Invalid field types
- Missing required fields
- Extremely long values
```

#### 6.3 Audio File Fuzzing

```python
# Corrupt audio headers
- Flip random bits
- Truncate files
- Invalid sample rates
- Corrupted metadata
```

---

## Test Execution

### Setup

```powershell
# Install test dependencies
pip install pytest pytest-qt pytest-cov hypothesis bandit safety

# Run all tests
pytest tests/

# Run with coverage
pytest --cov=src --cov-report=html tests/

# Run specific category
pytest tests/unit/
pytest tests/integration/
pytest tests/security/
```

### Continuous Integration

```yaml
# .github/workflows/test.yml
name: Test Suite
on: [push, pull_request]
jobs:
  test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run tests
        run: pytest --cov=src tests/
      - name: Security check
        run: bandit -r src/
```

---

## Test Coverage

### Coverage Targets

- **Overall**: ≥80%
- **Core modules**: ≥90%
- **Security-critical**: 100%

### Priority Areas

**Critical** (Must have 100% coverage):
- Plugin loading and validation
- Theme parsing and security
- Project serialization
- File path handling
- User input validation

**High Priority** (≥90% coverage):
- Export pipeline
- Audio processing
- Timeline operations
- Configuration management

**Medium Priority** (≥80% coverage):
- UI components
- Utility functions
- Helper classes

---

## Test Data

### Sample Files

**Audio**:
- `test_short.mp3` (5 sec)
- `test_long.wav` (1 hour)
- `test_silence.mp3` (silence)
- `test_corrupted.mp3` (invalid)

**Videos**:
- `test_720p.mp4`
- `test_1080p.mp4`
- `test_4k.mp4`

**Lyrics**:
- `test_lyrics.txt`
- `test_unicode.lrc`
- `test_large.srt` (10,000 lines)

**Malicious Files**:
- `malicious_plugin.nckplugin` (path traversal)
- `yaml_bomb.yaml`
- `zip_bomb.zip`

---

## Reporting

### Test Reports

- **Coverage Report**: `htmlcov/index.html`
- **Test Results**: `test-report.xml` (JUnit format)
- **Security Scan**: `bandit-report.json`

### Bug Templates

```markdown
## Bug Report

**Category**: [Unit/Integration/Security/Performance]
**Severity**: [Critical/High/Medium/Low]
**Component**: [Config/Plugin/Theme/Export]

### Description
...

### Steps to Reproduce
1. ...
2. ...

### Expected Behavior
...

### Actual Behavior
...

### Environment
- OS: Windows 11
- Python: 3.10
- NC-KTV: 0.10.0
```

---

## Appendix

### Test Checklist

- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] Security scan shows no high/critical issues
- [ ] Coverage ≥80%
- [ ] No memory leaks detected
- [ ] Performance benchmarks met
- [ ] Documentation updated

### References

- [OWASP Testing Guide](https://owasp.org/www-project-web-security-testing-guide/)
- [Python Security Best Practices](https://python.readthedocs.io/en/stable/library/security_warnings.html)
- [pytest Documentation](https://docs.pytest.org/)

---

**Test Plan Owner**: Development Team  
**Review Date**: Quarterly  
**Next Review**: 2026-04-07
