#!/usr/bin/env python3
"""
Tests for the configuration module.

This module tests the MegaDocConfig class and its functionality,
including pattern compilation, validation, and serialization.
"""

import pytest
import re
from pathlib import Path

from config.mega_doc_config import MegaDocConfig, get_config, set_config, reset_config


class TestMegaDocConfig:
    """Test cases for MegaDocConfig class."""
    
    def test_default_configuration(self):
        """Test default configuration values."""
        config = MegaDocConfig()
        
        # Test default values
        assert config.MAX_PREVIEW_FILES == 10
        assert config.MAX_ERROR_PREVIEW == 5
        assert config.MAX_FILE_SIZE_MB == 100
        assert config.DEFAULT_ENCODING == 'utf-8'
        assert config.CHUNK_SIZE == 8192
        assert config.ALLOW_PATH_TRAVERSAL == False
        assert config.MAX_DEPTH_LEVELS == 10
        assert config.ENABLE_CACHING == True
        assert config.CACHE_SIZE == 128
        
        # Test supported extensions
        assert 'py' in config.SUPPORTED_EXTENSIONS
        assert 'cpp' in config.SUPPORTED_EXTENSIONS
        assert 'h' in config.SUPPORTED_EXTENSIONS
        assert 'md' in config.SUPPORTED_EXTENSIONS
        assert 'json' in config.SUPPORTED_EXTENSIONS
    
    def test_file_patterns_lazy_loading(self):
        """Test that file patterns are lazy-loaded."""
        config = MegaDocConfig()
        
        # Initially should be None
        assert config._patterns_cache is None
        
        # Access patterns should compile them
        patterns = config.file_patterns
        assert config._patterns_cache is not None
        assert len(patterns) == 7  # We have 7 patterns
        assert all(isinstance(p, re.Pattern) for p in patterns)
        
        # Second access should use cache
        patterns2 = config.file_patterns
        assert patterns is patterns2  # Same object reference
    
    def test_file_patterns_content(self):
        """Test that file patterns match expected content."""
        config = MegaDocConfig()
        patterns = config.file_patterns
        
        # Test content that should match
        test_content = """
        - `src/main.py`
        - `include/header.h`
        `config/settings.json`
        [Link](docs/README.md)
        ```
        tools/script.sh
        ```
        """
        
        matches = set()
        for pattern in patterns:
            pattern_matches = pattern.findall(test_content)
            for match in pattern_matches:
                if isinstance(match, tuple):
                    matches.add(match[1] if len(match) > 1 else match[0])
                else:
                    matches.add(match)
        
        # Should find these files (some patterns may not match all files)
        expected_files = {'src/main.py', 'include/header.h', 'config/settings.json'}
        assert len(matches) >= 3  # At least 3 files should be found
        assert 'src/main.py' in matches
        assert 'include/header.h' in matches
    
    def test_validate_file_extension(self):
        """Test file extension validation."""
        config = MegaDocConfig()
        
        # Valid extensions
        assert config.validate_file_extension("file.py") == True
        assert config.validate_file_extension("script.cpp") == True
        assert config.validate_file_extension("header.h") == True
        assert config.validate_file_extension("config.json") == True
        assert config.validate_file_extension("README.md") == True
        
        # Invalid extensions
        assert config.validate_file_extension("file.unknown") == False
        assert config.validate_file_extension("file") == False
        assert config.validate_file_extension("") == False
        
        # Case insensitive
        assert config.validate_file_extension("file.PY") == True
        assert config.validate_file_extension("file.CPP") == True
    
    def test_get_max_file_size_bytes(self):
        """Test file size limit calculation."""
        config = MegaDocConfig()
        config.MAX_FILE_SIZE_MB = 5
        
        expected_bytes = 5 * 1024 * 1024
        assert config.get_max_file_size_bytes() == expected_bytes
    
    def test_is_file_too_large(self):
        """Test file size checking."""
        config = MegaDocConfig()
        config.MAX_FILE_SIZE_MB = 1
        
        # Small file
        assert config.is_file_too_large(500 * 1024) == False  # 500KB
        
        # Large file
        assert config.is_file_too_large(2 * 1024 * 1024) == True  # 2MB
    
    def test_get_encoding_options(self):
        """Test encoding options."""
        config = MegaDocConfig()
        encodings = config.get_encoding_options()
        
        assert len(encodings) == 4
        assert 'utf-8' in encodings
        assert 'latin-1' in encodings
        assert 'cp1252' in encodings
        assert 'utf-16' in encodings
    
    def test_to_dict(self):
        """Test configuration serialization."""
        config = MegaDocConfig()
        config.MAX_PREVIEW_FILES = 5
        config.MAX_FILE_SIZE_MB = 50
        
        config_dict = config.to_dict()
        
        assert isinstance(config_dict, dict)
        assert config_dict['max_preview_files'] == 5
        assert config_dict['max_file_size_mb'] == 50
        assert 'supported_extensions' in config_dict
        assert isinstance(config_dict['supported_extensions'], list)
    
    def test_from_dict(self):
        """Test configuration deserialization."""
        config_dict = {
            'supported_extensions': ['py', 'cpp', 'h'],
            'max_preview_files': 3,
            'max_file_size_mb': 25,
            'default_encoding': 'ascii'
        }
        
        config = MegaDocConfig.from_dict(config_dict)
        
        assert config.SUPPORTED_EXTENSIONS == frozenset(['py', 'cpp', 'h'])
        assert config.MAX_PREVIEW_FILES == 3
        assert config.MAX_FILE_SIZE_MB == 25
        assert config.DEFAULT_ENCODING == 'ascii'
    
    def test_pattern_compilation_performance(self):
        """Test that pattern compilation is reasonably fast."""
        import time
        
        config = MegaDocConfig()
        
        # First compilation
        start_time = time.time()
        patterns1 = config.file_patterns
        first_time = time.time() - start_time
        
        # Second compilation (should use cache)
        start_time = time.time()
        patterns2 = config.file_patterns
        second_time = time.time() - start_time
        
        # Second access should be much faster
        assert second_time < first_time
        assert patterns1 is patterns2  # Same object reference


class TestGlobalConfig:
    """Test global configuration management."""
    
    def test_get_config_default(self):
        """Test getting default global config."""
        reset_config()  # Ensure clean state
        config = get_config()
        
        assert isinstance(config, MegaDocConfig)
        assert config.MAX_PREVIEW_FILES == 10  # Default value
    
    def test_set_config(self):
        """Test setting global config."""
        custom_config = MegaDocConfig()
        custom_config.MAX_PREVIEW_FILES = 5
        
        set_config(custom_config)
        retrieved_config = get_config()
        
        assert retrieved_config is custom_config
        assert retrieved_config.MAX_PREVIEW_FILES == 5
    
    def test_reset_config(self):
        """Test resetting global config."""
        custom_config = MegaDocConfig()
        custom_config.MAX_PREVIEW_FILES = 5
        set_config(custom_config)
        
        reset_config()
        config = get_config()
        
        assert config.MAX_PREVIEW_FILES == 10  # Back to default
        assert config is not custom_config  # Different instance


class TestConfigEdgeCases:
    """Test edge cases and error conditions."""
    
    def test_empty_extensions(self):
        """Test configuration with empty extensions."""
        config = MegaDocConfig()
        config.SUPPORTED_EXTENSIONS = frozenset()
        
        # Should not crash
        patterns = config.file_patterns
        assert len(patterns) == 7  # Still has patterns, just empty extension part
        
        # Should not match any files
        assert config.validate_file_extension("file.py") == False
    
    def test_very_large_file_size_limit(self):
        """Test with very large file size limit."""
        config = MegaDocConfig()
        config.MAX_FILE_SIZE_MB = 1000  # 1GB
        
        # Should handle large limits gracefully
        assert config.get_max_file_size_bytes() == 1000 * 1024 * 1024
        assert config.is_file_too_large(500 * 1024 * 1024) == False  # 500MB
        assert config.is_file_too_large(2000 * 1024 * 1024) == True  # 2GB
    
    def test_negative_file_size_limit(self):
        """Test with negative file size limit."""
        config = MegaDocConfig()
        config.MAX_FILE_SIZE_MB = -1
        
        # Should handle negative limits gracefully
        assert config.get_max_file_size_bytes() == -1024 * 1024
        assert config.is_file_too_large(100) == True  # Any positive size is too large
    
    def test_invalid_serialization_data(self):
        """Test deserialization with invalid data."""
        # Test with missing keys
        config = MegaDocConfig.from_dict({})
        assert isinstance(config, MegaDocConfig)  # Should not crash
        
        # Test with invalid types
        invalid_dict = {
            'max_preview_files': 'not_a_number',
            'supported_extensions': 'not_a_list'
        }
        config = MegaDocConfig.from_dict(invalid_dict)
        assert isinstance(config, MegaDocConfig)  # Should not crash
