#!/usr/bin/env python3
"""
Configuration module for mega document creation.

This module provides centralized configuration management for the mega document
creation system, eliminating magic numbers and enabling easy customization.
"""

from dataclasses import dataclass, field
from typing import Set, List, Optional, Dict, Any
import re
from pathlib import Path
import os


@dataclass
class MegaDocConfig:
    """Centralized configuration for mega document creation."""
    
    # File extensions - immutable set for performance
    SUPPORTED_EXTENSIONS: Set[str] = field(default_factory=lambda: frozenset({
        'h', 'cpp', 'hpp', 'c', 'py', 'js', 'ts', 
        'java', 'rs', 'go', 'md', 'txt', 'sh', 'bat',
        'yml', 'yaml', 'json', 'xml', 'sql'
    }))
    
    # Display limits
    MAX_PREVIEW_FILES: int = 10
    MAX_ERROR_PREVIEW: int = 5
    
    # File processing limits
    MAX_FILE_SIZE_MB: int = 100
    DEFAULT_ENCODING: str = 'utf-8'
    CHUNK_SIZE: int = 8192  # For streaming large files
    
    # Security settings
    ALLOW_PATH_TRAVERSAL: bool = False
    MAX_DEPTH_LEVELS: int = 10
    
    # Performance settings
    ENABLE_CACHING: bool = True
    CACHE_SIZE: int = 128
    
    # Compiled regex patterns (lazy-loaded)
    _patterns_cache: Optional[List[re.Pattern]] = field(default=None, init=False)
    
    @property
    def file_patterns(self) -> List[re.Pattern]:
        """Lazily compile and cache regex patterns for performance."""
        if self._patterns_cache is None:
            ext_pattern = '|'.join(sorted(self.SUPPORTED_EXTENSIONS))
            self._patterns_cache = [
                # List items with backticks: - `file.py`
                re.compile(rf'-\s+`([^`]+\.(?:{ext_pattern}))`', re.MULTILINE | re.IGNORECASE),
                # Standalone backticks: `file.py` or `path/file.py`
                re.compile(rf'`([^`]*\.(?:{ext_pattern}))`', re.MULTILINE | re.IGNORECASE),
                # Lines starting with file paths: file.py
                re.compile(rf'^([^`\s][^`]*\.(?:{ext_pattern}))', re.MULTILINE | re.IGNORECASE),
                # Code blocks with file references
                re.compile(rf'```[^`]*\n([^`]*\.(?:{ext_pattern}))', re.MULTILINE | re.IGNORECASE),
                # Markdown links: [text](file.py)
                re.compile(rf'\[([^\]]+)\]\(([^)]*\.(?:{ext_pattern}))\)', re.MULTILINE | re.IGNORECASE),
                # List items without backticks: - file.py
                re.compile(rf'^-\s+([^`\s][^`]*\.(?:{ext_pattern}))', re.MULTILINE | re.IGNORECASE),
                # Brackets and parentheses: (file.py) or [file.py]
                re.compile(rf'[\[\(]([^\]\)]*\.(?:{ext_pattern}))[\]\)]', re.MULTILINE | re.IGNORECASE),
                # File references in text: src/file.cpp
                re.compile(rf'\b([a-zA-Z0-9_/\\-]+\.(?:{ext_pattern}))\b', re.MULTILINE | re.IGNORECASE)
            ]
        return self._patterns_cache
    
    def validate_file_extension(self, file_path: str) -> bool:
        """Validate file extension against supported types."""
        try:
            extension = Path(file_path).suffix[1:].lower()
            return extension in self.SUPPORTED_EXTENSIONS
        except (AttributeError, IndexError):
            return False
    
    def get_max_file_size_bytes(self) -> int:
        """Get maximum file size in bytes."""
        return self.MAX_FILE_SIZE_MB * 1024 * 1024
    
    def get_supported_extensions_pattern(self) -> str:
        """Get regex pattern for supported extensions."""
        return '|'.join(sorted(self.SUPPORTED_EXTENSIONS))
    
    def is_file_too_large(self, file_size_bytes: int) -> bool:
        """Check if file exceeds size limit."""
        return file_size_bytes > self.get_max_file_size_bytes()
    
    def get_encoding_options(self) -> List[str]:
        """Get list of encoding options to try."""
        return [self.DEFAULT_ENCODING, 'latin-1', 'cp1252', 'utf-16']
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert configuration to dictionary for serialization."""
        return {
            'supported_extensions': list(self.SUPPORTED_EXTENSIONS),
            'max_preview_files': self.MAX_PREVIEW_FILES,
            'max_error_preview': self.MAX_ERROR_PREVIEW,
            'max_file_size_mb': self.MAX_FILE_SIZE_MB,
            'default_encoding': self.DEFAULT_ENCODING,
            'chunk_size': self.CHUNK_SIZE,
            'allow_path_traversal': self.ALLOW_PATH_TRAVERSAL,
            'max_depth_levels': self.MAX_DEPTH_LEVELS,
            'enable_caching': self.ENABLE_CACHING,
            'cache_size': self.CACHE_SIZE
        }
    
    @classmethod
    def from_dict(cls, config_dict: Dict[str, Any]) -> 'MegaDocConfig':
        """Create configuration from dictionary."""
        config = cls()
        
        if 'supported_extensions' in config_dict:
            config.SUPPORTED_EXTENSIONS = frozenset(config_dict['supported_extensions'])
        
        # Map dictionary keys to config attributes
        key_mapping = {
            'max_preview_files': 'MAX_PREVIEW_FILES',
            'max_error_preview': 'MAX_ERROR_PREVIEW', 
            'max_file_size_mb': 'MAX_FILE_SIZE_MB',
            'default_encoding': 'DEFAULT_ENCODING',
            'chunk_size': 'CHUNK_SIZE',
            'allow_path_traversal': 'ALLOW_PATH_TRAVERSAL',
            'max_depth_levels': 'MAX_DEPTH_LEVELS',
            'enable_caching': 'ENABLE_CACHING',
            'cache_size': 'CACHE_SIZE'
        }
        
        for key, value in config_dict.items():
            if key in key_mapping:
                setattr(config, key_mapping[key], value)
        
        return config


# Global configuration instance
_config: Optional[MegaDocConfig] = None


def get_config() -> MegaDocConfig:
    """Get global configuration instance."""
    global _config
    if _config is None:
        _config = MegaDocConfig()
    return _config


def set_config(config: MegaDocConfig) -> None:
    """Set global configuration instance."""
    global _config
    _config = config


def reset_config() -> None:
    """Reset global configuration to default."""
    global _config
    _config = None
