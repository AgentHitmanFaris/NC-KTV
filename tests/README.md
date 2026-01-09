# NC-KTV Test Suite - Execution Guide

## Quick Start

### Install Test Dependencies

```powershell
pip install -r tests/requirements.txt
```

### Run All Tests

```powershell
.\run_tests.ps1
```

Then select option 5 (All Tests).

---

## Test Categories

### 1. Unit Tests (`tests/unit/`)

Test individual components in isolation.

**Run Command**:
```powershell
pytest tests/unit/ -v
```

**Coverage**:
- Configuration system (`test_config.py`)
- Plugin manager (`test_plugin_manager.py`)
- Theme manager
- Project serialization
- Audio processing
- Lyrics data

**Estimated Time**: 2-5 minutes

---

### 2. Integration Tests (`tests/integration/`)

Test component interactions.

**Run Command**:
```powershell
pytest tests/integration/ -v
```

**Coverage**:
- Plugin loading and execution
- Theme application
- Export pipeline
- Project save/load cycle

**Estimated Time**: 5-10 minutes

---

### 3. Security Tests (`tests/security/`)

Test for vulnerabilities and attack vectors.

**Run Command**:
```powershell
pytest tests/security/ -v
```

**Coverage**:
- Path traversal attacks (`test_vulnerabilities.py`)
- Code injection
- Buffer overflows
- YAML/ZIP bombs
- Fuzzing tests (`test_fuzzing.py`)

**Estimated Time**: 5-10 minutes

**Security Scan**:
```powershell
bandit -r src/ -f text
```

---

### 4. Stress Tests (`tests/stress/`)

Test performance and stability under load.

**Run Command**:
```powershell
pytest tests/stress/ -v -m "not slow"
```

**Coverage**:
- Memory stress tests
- CPU stress tests
- Disk I/O stress
- Concurrent operations
- Resource exhaustion

**Estimated Time**: 10-20 minutes

**Long-running tests**:
```powershell
pytest tests/stress/ -v  # Includes 24-hour tests
```

---

## Coverage Report

Generate full coverage report:

```powershell
pytest --cov=src --cov-report=html --cov-report=term tests/
```

Open report:
```powershell
start htmlcov/index.html
```

**Coverage Targets**:
- Overall: ≥80%
- Security-critical: 100%
- Core modules: ≥90%

---

## Continuous Integration

### GitHub Actions Example

```yaml
name: Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Set up Python
        uses: actions/setup-python@v2
        with:
          python-version: '3.10'
      
      - name: Install dependencies
        run: |
          pip install -r requirements.txt
          pip install -r tests/requirements.txt
      
      - name: Run tests
        run: pytest --cov=src --cov-report=xml tests/
      
      - name: Security scan
        run: bandit -r src/ -f json -o bandit-report.json
      
      - name: Upload coverage
        uses: codecov/codecov-action@v2
```

---

## Test Markers

Mark tests with pytest markers:

```python
@pytest.mark.slow  # Long-running tests
@pytest.mark.security  # Security tests
@pytest.mark.integration  # Integration tests
```

Run specific markers:
```powershell
pytest -m "not slow"  # Skip slow tests
pytest -m "security"  # Only security tests
```

---

## Writing New Tests

### Test Structure

```python
# tests/unit/test_myfeature.py

import pytest
from mymodule import MyClass

class TestMyClass:
    """Test MyClass functionality"""
    
    def test_basic_functionality(self):
        """Test basic use case"""
        obj = MyClass()
        result = obj.method()
        assert result == expected
    
    def test_edge_case(self):
        """Test edge case"""
        obj = MyClass()
        with pytest.raises(ValueError):
            obj.method(invalid_input)
```

### Use Fixtures

```python
@pytest.fixture
def my_fixture():
    """Reusable test data"""
    return {"key": "value"}

def test_with_fixture(my_fixture):
    assert my_fixture["key"] == "value"
```

---

## Debugging Failed Tests

### Run in verbose mode

```powershell
pytest -vv tests/unit/test_config.py
```

### Show print statements

```powershell
pytest -s tests/unit/test_config.py
```

### Run single test

```powershell
pytest tests/unit/test_config.py::TestConfig::test_default_config_creation
```

### Drop to debugger on failure

```powershell
pytest --pdb tests/unit/test_config.py
```

---

## Performance Profiling

### Profile tests

```powershell
pytest --profile tests/stress/
```

### Memory profiling

```powershell
python -m memory_profiler tests/stress/test_stress.py
```

---

## Common Issues

### Import Errors

**Solution**: Ensure `src/` is in Python path:
```python
import sys
sys.path.insert(0, 'src')
```

### Resource Cleanup

**Solution**: Use fixtures with cleanup:
```python
@pytest.fixture
def resource():
    r = create_resource()
    yield r
    r.cleanup()
```

### Flaky Tests

**Solution**: Add retries:
```python
@pytest.mark.flaky(reruns=3)
def test_flaky_operation():
    ...
```

---

## Best Practices

1. **Test Independence**: Tests should not depend on each other
2. **Use Fixtures**: Share setup code with fixtures
3. **Clear Names**: Test names should describe what they test
4. **One Assert**: Prefer one assertion per test
5. **Fast Tests**: Keep unit tests fast (< 100ms)
6. **Mock External**: Mock external dependencies (network, filesystem)

---

## Test Data

Test data is located in `tests/fixtures/`:
- Sample audio files
- Sample project files
- Malicious test inputs
- Unicode edge cases

---

## Reporting Bugs

When a test fails, report with:

1. **Test name**: Full pytest path
2. **Error message**: Complete traceback
3. **Environment**: OS, Python version, NC-KTV version
4. **Steps to reproduce**: Minimal reproduction
5. **Expected vs actual**: What should happen vs what happened

---

## Additional Resources

- [pytest Documentation](https://docs.pytest.org/)
- [pytest-qt](https://pytest-qt.readthedocs.io/)
- [OWASP Testing Guide](https://owasp.org/www-project-web-security-testing-guide/)
- [Python Security](https://python.readthedocs.io/en/stable/library/security_warnings.html)

---

**Last Updated**: 2026-01-07  
**Maintainer**: Development Team
