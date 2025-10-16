#!/usr/bin/env python3
"""
Repository pattern implementation for file operations.

This module provides abstract and concrete implementations of file repositories
with security validation, caching, and comprehensive error handling.
"""

from abc import ABC, abstractmethod
from pathlib import Path
from typing import Iterator, Optional, Dict, Any, List
import os
import stat
import logging
from datetime import datetime

from config.mega_doc_config import MegaDocConfig
from exceptions.mega_doc_exceptions import (
    FileValidationError, SecurityError, ProcessingError, CacheError
)


class FileRepository(ABC):
    """Abstract repository for file operations."""
    
    @abstractmethod
    def read_file(self, path: Path) -> str:
        """Read entire file content."""
        pass
    
    @abstractmethod
    def read_file_stream(self, path: Path) -> Iterator[str]:
        """Stream file content in chunks."""
        pass
    
    @abstractmethod
    def file_exists(self, path: Path) -> bool:
        """Check if file exists."""
        pass
    
    @abstractmethod
    def get_file_info(self, path: Path) -> Dict[str, Any]:
        """Get comprehensive file information."""
        pass
    
    @abstractmethod
    def validate_path(self, path: Path) -> Path:
        """Validate and normalize file path."""
        pass


class SecureFileRepository(FileRepository):
    """Secure file repository with comprehensive validation."""
    
    def __init__(self, config: MegaDocConfig):
        self.config = config
        self.base_path = Path.cwd().resolve()
        self.logger = logging.getLogger(__name__)
        self._file_cache: Dict[Path, Dict[str, Any]] = {}
    
    def validate_path(self, path: Path) -> Path:
        """Prevent path traversal attacks and validate paths."""
        try:
            # Convert to Path if string
            if isinstance(path, str):
                path = Path(path)
            
            # Resolve to absolute path relative to base path
            if path.is_absolute():
                resolved = path
            else:
                resolved = (self.base_path / path).resolve()
            
            # Check for path traversal
            if not self.config.ALLOW_PATH_TRAVERSAL:
                try:
                    resolved.relative_to(self.base_path)
                except ValueError:
                    raise SecurityError(f"Path traversal detected: {path}")
            
            # Check depth levels
            depth = len(resolved.parts) - len(self.base_path.parts)
            if depth > self.config.MAX_DEPTH_LEVELS:
                raise SecurityError(f"Path too deep: {path}")
            
            return resolved
            
        except Exception as e:
            if isinstance(e, SecurityError):
                raise
            raise SecurityError(f"Invalid path: {path}") from e
    
    def file_exists(self, path: Path) -> bool:
        """Check if file exists and is readable."""
        try:
            validated_path = self.validate_path(path)
            return validated_path.exists() and validated_path.is_file()
        except SecurityError:
            return False
    
    def get_file_info(self, path: Path) -> Dict[str, Any]:
        """Get comprehensive file information with caching."""
        validated_path = self.validate_path(path)
        
        # Check cache first
        if self.config.ENABLE_CACHING and validated_path in self._file_cache:
            cached_info = self._file_cache[validated_path]
            # Check if file has been modified since caching
            try:
                current_mtime = validated_path.stat().st_mtime
                if cached_info.get('mtime') == current_mtime:
                    return cached_info
            except OSError:
                pass  # File might have been deleted, will handle below
        
        if not self.file_exists(validated_path):
            raise FileValidationError(str(validated_path), "File does not exist")
        
        try:
            stat_info = validated_path.stat()
            file_info = {
                'path': str(validated_path),
                'size': stat_info.st_size,
                'modified': stat_info.st_mtime,
                'created': stat_info.st_ctime,
                'mtime': stat_info.st_mtime,  # For cache invalidation
                'extension': validated_path.suffix[1:].lower(),
                'is_readable': os.access(validated_path, os.R_OK),
                'is_writable': os.access(validated_path, os.W_OK),
                'permissions': stat.filemode(stat_info.st_mode),
                'owner_uid': stat_info.st_uid,
                'group_gid': stat_info.st_gid,
                'inode': stat_info.st_ino,
                'device': stat_info.st_dev,
                'hard_links': stat_info.st_nlink
            }
            
            # Cache the result
            if self.config.ENABLE_CACHING:
                self._file_cache[validated_path] = file_info
            
            return file_info
            
        except OSError as e:
            raise FileValidationError(str(validated_path), f"OS error: {e}")
    
    def read_file(self, path: Path) -> str:
        """Read file with comprehensive error handling."""
        validated_path = self.validate_path(path)
        file_info = self.get_file_info(validated_path)
        
        # Check file size
        if self.config.is_file_too_large(file_info['size']):
            raise FileValidationError(
                str(validated_path), 
                f"File too large: {file_info['size']} bytes (max: {self.config.get_max_file_size_bytes()})"
            )
        
        # Check readability
        if not file_info['is_readable']:
            raise FileValidationError(str(validated_path), "File not readable")
        
        # Try multiple encodings
        for encoding in self.config.get_encoding_options():
            try:
                content = validated_path.read_text(encoding=encoding)
                self.logger.debug(f"Successfully read {validated_path} with encoding {encoding}")
                return content
            except UnicodeDecodeError:
                self.logger.debug(f"Failed to decode {validated_path} with encoding {encoding}")
                continue
        
        raise FileValidationError(str(validated_path), "Unable to decode file with any supported encoding")
    
    def read_file_stream(self, path: Path) -> Iterator[str]:
        """Stream large files in chunks."""
        validated_path = self.validate_path(path)
        
        try:
            with open(validated_path, 'r', encoding=self.config.DEFAULT_ENCODING) as f:
                while chunk := f.read(self.config.CHUNK_SIZE):
                    yield chunk
        except UnicodeDecodeError:
            # Fallback to binary mode for problematic files
            self.logger.warning(f"Falling back to binary mode for {validated_path}")
            with open(validated_path, 'rb') as f:
                while chunk := f.read(self.config.CHUNK_SIZE):
                    yield chunk.decode('latin-1', errors='replace')
        except Exception as e:
            raise ProcessingError(str(validated_path), "stream", e)
    
    def clear_cache(self):
        """Clear the file info cache."""
        self._file_cache.clear()
        self.logger.info("File cache cleared")
    
    def get_cache_stats(self) -> Dict[str, Any]:
        """Get cache statistics."""
        return {
            'cached_files': len(self._file_cache),
            'cache_enabled': self.config.ENABLE_CACHING,
            'cache_size_limit': self.config.CACHE_SIZE
        }


class MockFileRepository(FileRepository):
    """Mock file repository for testing purposes."""
    
    def __init__(self, config: MegaDocConfig):
        self.config = config
        self.base_path = Path.cwd().resolve()  # Add base_path for compatibility
        self.files: Dict[str, str] = {}
        self.file_info: Dict[str, Dict[str, Any]] = {}
        self.logger = logging.getLogger(__name__)
    
    def add_file(self, path: str, content: str, **file_info):
        """Add a mock file to the repository."""
        self.files[path] = content
        self.file_info[path] = {
            'path': path,
            'size': len(content.encode('utf-8')),
            'modified': datetime.now().timestamp(),
            'created': datetime.now().timestamp(),
            'extension': Path(path).suffix[1:].lower(),
            'is_readable': True,
            'is_writable': True,
            'permissions': '-rw-r--r--',
            **file_info
        }
    
    def validate_path(self, path: Path) -> Path:
        """Mock path validation."""
        return Path(path)
    
    def file_exists(self, path: Path) -> bool:
        """Check if mock file exists."""
        return str(path) in self.files
    
    def get_file_info(self, path: Path) -> Dict[str, Any]:
        """Get mock file information."""
        path_str = str(path)
        if path_str not in self.file_info:
            raise FileValidationError(path_str, "File does not exist")
        return self.file_info[path_str]
    
    def read_file(self, path: Path) -> str:
        """Read mock file content."""
        path_str = str(path)
        if path_str not in self.files:
            raise FileValidationError(path_str, "File does not exist")
        return self.files[path_str]
    
    def read_file_stream(self, path: Path) -> Iterator[str]:
        """Stream mock file content."""
        content = self.read_file(path)
        chunk_size = self.config.CHUNK_SIZE
        for i in range(0, len(content), chunk_size):
            yield content[i:i + chunk_size]
