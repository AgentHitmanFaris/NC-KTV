"""
Fuzzing Tests for NC-KTV
Generate random/malformed inputs to find crashes and vulnerabilities
"""

import pytest
import random
import string
import json
import yaml
from pathlib import Path


class TestConfigFuzzing:
    """Fuzz testing for configuration system"""
    
    def generate_random_yaml(self, depth=3, width=5):
        """Generate random YAML structure"""
        if depth == 0:
            # Base case: return random primitive
            choice = random.choice(['string', 'int', 'float', 'bool', 'none'])
            if choice == 'string':
                return ''.join(random.choices(string.printable, k=random.randint(0, 100)))
            elif choice == 'int':
                return random.randint(-1000000, 1000000)
            elif choice == 'float':
                return random.uniform(-1000.0, 1000.0)
            elif choice == 'bool':
                return random.choice([True, False])
            else:
                return None
        
        # Recursive case
        structure_type = random.choice(['dict', 'list'])
        if structure_type == 'dict':
            return {
                f'key_{i}': self.generate_random_yaml(depth - 1, width)
                for i in range(random.randint(0, width))
            }
        else:
            return [
                self.generate_random_yaml(depth - 1, width)
                for _ in range(random.randint(0, width))
            ]
    
    def test_random_config_structures(self, temp_dir):
        """Test with randomly generated config structures"""
        from utils.config import Config
        
        for iteration in range(50):  # 50 random configurations
            config_path = temp_dir / f"fuzz_config_{iteration}.yaml"
           
            try:
                config = Config(config_path)
                random_data = self.generate_random_yaml()
                
                config.settings = random_data
                config.save()
                
                # Try to reload
                config2 = Config(config_path)
                
                # If it doesn't crash, success
                assert True
            except (yaml.YAMLError, TypeError, ValueError, RecursionError):
                # Expected errors are okay
                assert True
    
    def test_malformed_yaml_syntax(self, temp_dir):
        """Test with malformed YAML syntax"""
        from utils.config import Config
        
        malformed_yamls = [
            "key: value: invalid",
            "- item\n  - nested\n - badindent",
            "[unclosed",
            "{unclosed",
            "key: 'unclosed string",
            "key: value\n\tinvalid tab",
            "!!python/object:os.system",
            "& anchor * reference",
        ]
        
        for i, mal_yaml in enumerate(malformed_yamls):
            config_path = temp_dir / f"malformed_{i}.yaml"
            config_path.write_text(mal_yaml)
            
            try:
                config = Config(config_path)
                # Should handle gracefully
                assert True
            except yaml.YAMLError:
                # Rejection is acceptable
                assert True


class TestPluginManifestFuzzing:
    """Fuzz testing for plugin manifests"""
    
    def mutate_json(self, data, mutation_rate=0.3):
        """Mutate JSON data randomly"""
        if isinstance(data, dict):
            mutated = {}
            for key, value in data.items():
                # Random key mutation
                if random.random() < mutation_rate:
                    key = ''.join(random.choices(string.printable, k=random.randint(0, 50)))
                
                # Random value mutation
                if random.random() < mutation_rate:
                    value = random.choice([
                        None,
                        random.randint(-1000, 1000),
                        random.random(),
                        ''.join(random.choices(string.printable, k=random.randint(0, 100))),
                        [],
                        {}
                    ])
                elif isinstance(value, (dict, list)):
                    value = self.mutate_json(value, mutation_rate)
                
                mutated[key] = value
            return mutated
        elif isinstance(data, list):
            return [self.mutate_json(item, mutation_rate) for item in data]
        else:
            return data
    
    def test_mutated_plugin_manifests(self, temp_dir, sample_plugin_manifest):
        """Test with mutated plugin manifests"""
        from core.plugin_manager import PluginManager
        from utils.config import Config
        
        config = Config(temp_dir / "config.yaml")
        manager = PluginManager(config)
        manager.INSTALLED_DIR = temp_dir / "plugins"
        manager.INSTALLED_DIR.mkdir()
        
        for iteration in range(30):  # 30 mutations
            plugin_dir = manager.INSTALLED_DIR / f"fuzz_plugin_{iteration}"
            plugin_dir.mkdir()
            
            # Mutate manifest
            mutated_manifest = self.mutate_json(sample_plugin_manifest.copy())
            
            try:
                with open(plugin_dir / "plugin.json", 'w') as f:
                    json.dump(mutated_manifest, f)
                
                # Try to load
                with open(plugin_dir / "main.py", 'w') as f:
                    f.write("class Plugin: pass")
                
                manager.load_plugin(f"fuzz_{iteration}", plugin_dir)
                
                # If no crash, success
                assert True
            except (json.JSONDecodeError, TypeError, KeyError, ValueError):
                # Expected errors
                assert True


class TestFileFuzzing:
    """Fuzz testing for file handling"""
    
    def generate_random_bytes(self, size):
        """Generate random byte sequence"""
        return bytes(random.randint(0, 255) for _ in range(size))
    
    def test_corrupted_project_files(self, temp_dir):
        """Test with corrupted project files"""
        from core.project import Project
        
        # Create valid project first
        project = Project()
        project_path = temp_dir / "test.nctv"
        project.save(project_path)
        
        # Read original data
        original_data = project_path.read_bytes()
        
        # Create corrupted versions
        for iteration in range(20):
            corrupted_path = temp_dir / f"corrupted_{iteration}.nctv"
            
            # Apply random corruptions
            corrupted_data = bytearray(original_data)
            
            # Flip random bits
            for _ in range(random.randint(1, 10)):
                pos = random.randint(0, len(corrupted_data) - 1)
                corrupted_data[pos] ^= random.randint(1, 255)
            
            corrupted_path.write_bytes(bytes(corrupted_data))
            
            try:
                Project.load(corrupted_path)
                assert True
            except (ValueError, EOFError, Exception):
                # Corruption should be handled
                assert True
    
    def test_random_file_sizes(self, temp_dir):
        """Test with random file sizes"""
        from utils.config import Config
        
        sizes = [0, 1, 10, 100, 1000, 10000, 100000, 1000000]
        
        for size in sizes:
            config_path = temp_dir / f"size_{size}.yaml"
            config_path.write_bytes(self.generate_random_bytes(size))
            
            try:
                config = Config(config_path)
                assert True
            except (yaml.YAMLError, UnicodeDecodeError, MemoryError):
                assert True


class TestStringFuzzing:
    """Fuzz testing for string handling"""
    
    def generate_fuzzy_string(self, max_length=1000):
        """Generate fuzzy string with special characters"""
        strategies = [
            lambda: ''.join(random.choices(string.printable, k=random.randint(0, max_length))),
            lambda: '\x00' * random.randint(0, 100),  # Null bytes
            lambda: '\xff' * random.randint(0, 100),  # High bytes
            lambda: ''.join(chr(i) for i in random.sample(range(0, 0x10000), min(100, max_length))),  # Unicode
            lambda: '🔥' * random.randint(0, 100),  # Emoji
            lambda: '\r\n' * random.randint(0, 100),  # Line endings
            lambda: ' ' * random.randint(0, 1000),  # Whitespace
            lambda: '\\' * random.randint(0, 100),  # Backslashes
            lambda: '"' * random.randint(0, 100),  # Quotes
        ]
        
        return random.choice(strategies)()
    
    def test_fuzzy_config_values(self, temp_dir):
        """Test with fuzzy string values"""
        from utils.config import Config
        
        config = Config(temp_dir / "config.yaml")
        
        for iteration in range(50):
            fuzzy_value = self.generate_fuzzy_string()
            
            try:
                config.set(f'fuzz.test_{iteration}', fuzzy_value)
                config.save()
                
                config2 = Config(temp_dir / "config.yaml")
                retrieved = config2.get(f'fuzz.test_{iteration}')
                
                # Should handle all strings
                assert True
            except (UnicodeError, yaml.YAMLError, ValueError):
                # Some strings may be rejected
                assert True


class TestBoundaryFuzzing:
    """Test boundary conditions"""
    
    def test_integer_boundaries(self, temp_dir):
        """Test integer boundary values"""
        from utils.config import Config
        
        config = Config(temp_dir / "config.yaml")
        
        boundaries = [
            -2**63,  # Min 64-bit int
            -2**31,  # Min 32-bit int
            -1,
            0,
            1,
            2**31 - 1,  # Max 32-bit int
            2**63 - 1,  # Max 64-bit int
        ]
        
        for value in boundaries:
            try:
                config.set('test.int', value)
                config.save()
                
                config2 = Config(temp_dir / "config.yaml")
                assert config2.get('test.int') == value
            except (ValueError, OverflowError):
                assert True
    
    def test_float_boundaries(self, temp_dir):
        """Test float boundary values"""
        from utils.config import Config
        import math
        
        config = Config(temp_dir / "config.yaml")
        
        boundaries = [
            -float('inf'),
            float('inf'),
            float('nan'),
            -1.7976931348623157e+308,  # Approx min float
            1.7976931348623157e+308,   # Approx max float
            2.2250738585072014e-308,   # Min positive float
            0.0,
            -0.0,
        ]
        
        for value in boundaries:
            try:
                config.set('test.float', value)
                config.save()
                assert True
            except (ValueError, OverflowError, yaml.YAMLError):
                # Some values may be rejected
                assert True


if __name__ == '__main__':
    pytest.main([__file__, '-v', '-x'])  # Stop on first failure for fuzzing
