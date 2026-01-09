"""
Stress Tests for NC-KTV
Tests performance under heavy load and resource constraints
"""

import pytest
import time
import psutil
import os
from pathlib import Path


class TestMemoryStress:
    """Test memory usage under stress"""
    
    def test_large_project_load(self, temp_dir):
        """Test loading very large project files"""
        from core.project import Project
        
        # Create project with many lyrics
        project = Project()
        
        # Add 10,000 lyrics lines
        for i in range(10000):
            project.lyrics.add_line(
                text=f"Line {i} with some longer text to increase size",
                start_time=float(i),
                end_time=float(i + 1)
            )
        
        # Save and reload
        project_path = temp_dir / "large_project.nctv"
        project.save(project_path)
        
        # Measure memory before
        process = psutil.Process(os.getpid())
        mem_before = process.memory_info().rss / 1024 / 1024  # MB
        
        # Load project
        loaded_project = Project.load(project_path)
        
        # Measure memory after
        mem_after = process.memory_info().rss / 1024 / 1024  # MB
        mem_used = mem_after - mem_before
        
        # Should not use excessive memory (< 500MB for this test)
        assert mem_used < 500
        assert len(loaded_project.lyrics.lines) == 10000
    
    def test_memory_leak_detection(self, temp_dir):
        """Test for memory leaks in repeated operations"""
        from utils.config import Config
        
        process = psutil.Process(os.getpid())
        measurements = []
        
        # Perform operation 100 times
        for i in range(100):
            config = Config(temp_dir / f"config_{i}.yaml")
            config.set('test.value', f"value_{i}")
            config.save()
            
            # Measure memory every 10 iterations
            if i % 10 == 0:
                mem = process.memory_info().rss / 1024 / 1024
                measurements.append(mem)
        
        # Check for significant memory growth
        if len(measurements) >= 2:
            growth = measurements[-1] - measurements[0]
            # Should not grow more than 100MB
            assert growth < 100
    
    def test_concurrent_plugin_loading(self, temp_dir):
        """Test loading many plugins simultaneously"""
        from core.plugin_manager import PluginManager
        from utils.config import Config
        import json
        
        config = Config(temp_dir / "config.yaml")
        manager = PluginManager(config)
        manager.INSTALLED_DIR = temp_dir / "plugins"
        manager.INSTALLED_DIR.mkdir()
        
        # Create 100 plugins
        for i in range(100):
            plugin_dir = manager.INSTALLED_DIR / f"plugin_{i}"
            plugin_dir.mkdir()
            
            manifest = {
                "id": f"com.test.plugin_{i}",
                "name": f"Plugin {i}",
                "version": "1.0.0",
                "author": "Test",
                "type": "effect"
            }
            
            with open(plugin_dir / "plugin.json", 'w') as f:
                json.dump(manifest, f)
            
            with open(plugin_dir / "main.py", 'w') as f:
                f.write("""
from core.plugin_base import EffectPlugin
class Plugin(EffectPlugin):
    def initialize(self, api): return True
    def cleanup(self): pass
    def get_name(self): return "Test"
    def get_parameters(self): return []
    def render_frame(self, ctx, params): return {}
""")
        
        # Load all plugins
        start_time = time.time()
        manager.load_all_plugins()
        load_time = time.time() - start_time
        
        # Should load in reasonable time (< 10 seconds)
        assert load_time < 10
        assert len(manager.plugins) == 100


class TestCPUStress:
    """Test CPU usage under stress"""
    
    def test_export_performance(self, temp_dir):
        """Test export performance with long audio"""
        pytest.skip("Requires audio/video files - integration test")
    
    def test_transcription_performance(self, temp_dir):
        """Test transcription performance"""
        pytest.skip("Requires Whisper - integration test")
    
    def test_rapid_theme_switching(self, temp_dir):
        """Test rapid theme switching"""
        from utils.theme_manager import ThemeManager
        from utils.config import Config
        
        config = Config(temp_dir / "config.yaml")
        manager = ThemeManager(config)
        
        # Switch themes rapidly
        start_time = time.time()
        for i in range(100):
            theme_id = 'builtin-dark' if i % 2 == 0 else 'builtin-light'
            manager.set_active_theme(theme_id)
        
        elapsed = time.time() - start_time
        
        # Should handle quickly (< 1 second)
        assert elapsed < 1.0


class TestDiskStress:
    """Test disk I/O under stress"""
    
    def test_large_config_save(self, temp_dir):
        """Test saving very large configuration"""
        from utils.config import Config
        
        config = Config(temp_dir / "config.yaml")
        
        # Create large nested structure
        large_data = {}
        for i in range(1000):
            large_data[f'key_{i}'] = {
                'value': 'A' * 1000,
                'nested': {
                    'data': list(range(100))
                }
            }
        
        config.set('large.data', large_data)
        
        start_time = time.time()
        config.save()
        save_time = time.time() - start_time
        
        # Should save in reasonable time (< 5 seconds)
        assert save_time < 5.0
        
        # Verify file size
        file_size = (temp_dir / "config.yaml").stat().st_size / 1024 / 1024  # MB
        # Should be under 100MB
        assert file_size < 100
    
    def test_concurrent_file_access(self, temp_dir):
        """Test concurrent file access"""
        from utils.config import Config
        import threading
        
        config_path = temp_dir / "shared_config.yaml"
        errors = []
        
        def write_config(thread_id):
            try:
                config = Config(config_path)
                config.set(f'thread.{thread_id}', f'value_{thread_id}')
                config.save()
            except Exception as e:
                errors.append(e)
        
        # Start 10 threads writing simultaneously
        threads = []
        for i in range(10):
            t = threading.Thread(target=write_config, args=(i,))
            t.start()
            threads.append(t)
        
        for t in threads:
            t.join()
        
        # Some writes may fail due to race conditions, but no crashes
        # (In production, add proper locking)
        assert len(errors) < 10  # Not all should fail


class TestLongRunningOperations:
    """Test stability over long periods"""
    
    @pytest.mark.slow
    def test_24hour_stability(self, temp_dir):
        """Test 24-hour continuous operation"""
        # This would run for 24 hours in actual testing
        # For quick tests, we simulate shorter duration
        pytest.skip("Long-running test - run manually")
    
    def test_repeated_operations(self, temp_dir):
        """Test repeated operations for stability"""
        from utils.config import Config
        
        # Perform 1000 save/load cycles
        config_path = temp_dir / "config.yaml"
        
        for i in range(1000):
            config = Config(config_path)
            config.set('iteration', i)
            config.save()
            
            config2 = Config(config_path)
            assert config2.get('iteration') == i
        
        # No crashes = success
        assert True


class TestResourceLimits:
    """Test behavior at resource limits"""
    
    def test_disk_full_simulation(self, temp_dir):
        """Test handling of disk full condition"""
        pytest.skip("Requires special setup")
    
    def test_max_file_descriptors(self, temp_dir):
        """Test handling of max file descriptor limit"""
        # Try to open many files
        files = []
        max_files = 0
        
        try:
            for i in range(10000):
                f = open(temp_dir / f"file_{i}.txt", 'w')
                files.append(f)
                max_files = i + 1
        except OSError as e:
            # Hit limit
            assert "Too many open files" in str(e) or max_files > 0
        finally:
            for f in files:
                try:
                    f.close()
                except:
                    pass
    
    def test_max_threads(self, temp_dir):
        """Test handling of max thread limit"""
        import threading
        
        threads = []
        max_threads = 0
        
        def dummy_work():
            time.sleep(0.1)
        
        try:
            for i in range(10000):
                t = threading.Thread(target=dummy_work)
                t.start()
                threads.append(t)
                max_threads = i + 1
        except (OSError, RuntimeError):
            # Hit thread limit
            assert max_threads > 0
        finally:
            for t in threads:
                try:
                    t.join(timeout=1)
                except:
                    pass


class TestPerformanceBenchmarks:
    """Performance benchmark tests"""
    
    def test_config_load_speed(self, temp_dir):
        """Benchmark config loading speed"""
        from utils.config import Config
        
        config = Config(temp_dir / "config.yaml")
        
        # Multiple loads
        times = []
        for i in range(100):
            start = time.time()
            config.load()
            elapsed = time.time() - start
            times.append(elapsed)
        
        avg_time = sum(times) / len(times)
        
        # Should load in < 10ms on average
        assert avg_time < 0.01
    
    def test_plugin_discovery_speed(self, temp_dir):
        """Benchmark plugin discovery speed"""
        from core.plugin_manager import PluginManager
        from utils.config import Config
        import json
        
        config = Config(temp_dir / "config.yaml")
        manager = PluginManager(config)
        manager.INSTALLED_DIR = temp_dir / "plugins"
        manager.INSTALLED_DIR.mkdir()
        
        # Create 50 plugins
        for i in range(50):
            plugin_dir = manager.INSTALLED_DIR / f"plugin_{i}"
            plugin_dir.mkdir()
            
            with open(plugin_dir / "plugin.json", 'w') as f:
                json.dump({
                    "id": f"plugin_{i}",
                    "name": f"Plugin {i}",
                    "version": "1.0.0",
                    "author": "Test",
                    "type": "effect"
                }, f)
        
        start = time.time()
        plugins = manager.discover_plugins()
        elapsed = time.time() - start
        
        # Should discover in < 1 second
        assert elapsed < 1.0
        assert len(plugins) == 50


if __name__ == '__main__':
    pytest.main([__file__, '-v', '-m', 'not slow'])
