#!/usr/bin/env python3
"""
Tests for the repository module.

This module tests both the abstract FileRepository interface and
the concrete SecureFileRepository and MockFileRepository implementations.
"""

import pytest
import tempfile
import shutil
from pathlib import Path
from unittest.mock import Mock, patch, mock_open

from repositories.file_repository import FileRepository, SecureFileRepository, MockFileRepository
from exceptions.mega_doc_exceptions import FileValidationError, SecurityError, ProcessingError


class TestMockFileRepository:
    """Test MockFileRepository for testing purposes."""
    
    def test_initialization(self, test_config):
        """Test mock repository initialization."""
        repo = MockFileRepository(test_config)
        
        assert repo.config is test_config
        assert repo.files == {}
        assert repo.file_info == {}
    
    def test_add_file(self, test_config):
        """Test adding files to mock repository."""
        repo = MockFileRepository(test_config)
        
        repo.add_file("test.py", "print('hello')", size=100)
        
        assert "test.py" in repo.files
        assert repo.files["test.py"] == "print('hello')"
        assert "test.py" in repo.file_info
        assert repo.file_info["test.py"]["size"] == 100
    
    def test_file_exists(self, test_config):
        """Test file existence checking."""
        repo = MockFileRepository(test_config)
        
        assert not repo.file_exists(Path("nonexistent.py"))
        
        repo.add_file("test.py", "content")
        assert repo.file_exists(Path("test.py"))
    
    def test_get_file_info(self, test_config):
        """Test getting file information."""
        repo = MockFileRepository(test_config)
        
        repo.add_file("test.py", "content", size=200, modified=1234567890)
        
        info = repo.get_file_info(Path("test.py"))
        assert info["path"] == "test.py"
        assert info["size"] == 200
        assert info["modified"] == 1234567890
        assert info["extension"] == "py"
    
    def test_get_file_info_nonexistent(self, test_config):
        """Test getting info for nonexistent file."""
        repo = MockFileRepository(test_config)
        
        with pytest.raises(FileValidationError):
            repo.get_file_info(Path("nonexistent.py"))
    
    def test_read_file(self, test_config):
        """Test reading file content."""
        repo = MockFileRepository(test_config)
        
        repo.add_file("test.py", "print('hello')")
        
        content = repo.read_file(Path("test.py"))
        assert content == "print('hello')"
    
    def test_read_file_nonexistent(self, test_config):
        """Test reading nonexistent file."""
        repo = MockFileRepository(test_config)
        
        with pytest.raises(FileValidationError):
            repo.read_file(Path("nonexistent.py"))
    
    def test_read_file_stream(self, test_config):
        """Test streaming file content."""
        repo = MockFileRepository(test_config)
        
        content = "x" * 1000  # 1000 characters
        repo.add_file("test.py", content)
        
        chunks = list(repo.read_file_stream(Path("test.py")))
        assert len(chunks) > 0
        assert "".join(chunks) == content


class TestSecureFileRepository:
    """Test SecureFileRepository with real file system."""
    
    def test_initialization(self, test_config):
        """Test secure repository initialization."""
        repo = SecureFileRepository(test_config)
        
        assert repo.config is test_config
        assert repo.base_path == Path.cwd().resolve()
        assert repo._file_cache == {}
    
    def test_validate_path_success(self, test_config, temp_dir):
        """Test successful path validation."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "test.py"
        test_file.write_text("print('hello')")
        
        validated = repo.validate_path(test_file)
        # Handle macOS symlink resolution differences
        expected = test_file.resolve()
        assert str(validated) == str(expected) or str(validated) == str(expected).replace('/private', '')
    
    def test_validate_path_string_input(self, test_config, temp_dir):
        """Test path validation with string input."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "test.py"
        test_file.write_text("print('hello')")
        
        validated = repo.validate_path(str(test_file))
        # Handle macOS symlink resolution differences
        expected = test_file.resolve()
        assert str(validated) == str(expected) or str(validated) == str(expected).replace('/private', '')
    
    def test_validate_path_traversal_detection(self):
        """Test path traversal detection."""
        from config.mega_doc_config import MegaDocConfig
        strict_config = MegaDocConfig()
        strict_config.ALLOW_PATH_TRAVERSAL = False
        
        repo = SecureFileRepository(strict_config)
        
        with pytest.raises(SecurityError, match="Path traversal detected"):
            repo.validate_path(Path("/etc/passwd"))  # Absolute path outside project
        
        with pytest.raises(SecurityError, match="Path traversal detected"):
            repo.validate_path(Path("../../../etc/passwd"))  # Relative path traversal
    
    def test_validate_path_depth_limit(self, test_config):
        """Test path depth limit enforcement."""
        repo = SecureFileRepository(test_config)
        repo.config.MAX_DEPTH_LEVELS = 2
        
        # Create a deep path
        deep_path = "a" + "/b" * 10 + "/file.py"
        
        with pytest.raises(SecurityError, match="Path too deep"):
            repo.validate_path(deep_path)
    
    def test_file_exists(self, test_config, temp_dir):
        """Test file existence checking."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "test.py"
        test_file.write_text("print('hello')")
        
        assert repo.file_exists(test_file)
        assert not repo.file_exists(temp_dir / "nonexistent.py")
    
    def test_file_exists_security_error(self, test_config):
        """Test file existence with security error."""
        repo = SecureFileRepository(test_config)
        
        # Should return False for security violations
        assert not repo.file_exists(Path("../../../etc/passwd"))
    
    def test_get_file_info(self, test_config, temp_dir):
        """Test getting file information."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "test.py"
        test_file.write_text("print('hello')")
        
        info = repo.get_file_info(test_file)
        
        assert info["path"] == str(test_file.resolve()) or info["path"] == str(test_file.resolve()).replace('/private', '')
        assert info["size"] > 0
        assert info["extension"] == "py"
        assert info["is_readable"] == True
        assert "permissions" in info
        assert "modified" in info
        assert "created" in info
    
    def test_get_file_info_caching(self, test_config, temp_dir):
        """Test file info caching."""
        repo = SecureFileRepository(test_config)
        repo.config.ENABLE_CACHING = True
        
        test_file = temp_dir / "test.py"
        test_file.write_text("print('hello')")
        
        # First call
        info1 = repo.get_file_info(test_file)
        
        # Second call should use cache
        info2 = repo.get_file_info(test_file)
        
        assert info1 == info2
        # Check cache with path normalization
        resolved_path = test_file.resolve()
        cache_key = resolved_path
        if cache_key not in repo._file_cache:
            # Try with /private prefix removed
            cache_key = Path(str(resolved_path).replace('/private', ''))
        
        assert cache_key in repo._file_cache
    
    def test_get_file_info_cache_invalidation(self, test_config, temp_dir):
        """Test cache invalidation when file is modified."""
        repo = SecureFileRepository(test_config)
        repo.config.ENABLE_CACHING = True
        
        test_file = temp_dir / "test.py"
        test_file.write_text("print('hello')")
        
        # First call
        info1 = repo.get_file_info(test_file)
        
        # Modify file
        test_file.write_text("print('modified')")
        
        # Second call should not use cache
        info2 = repo.get_file_info(test_file)
        
        assert info1["size"] != info2["size"]  # Size should be different
    
    def test_read_file_success(self, test_config, temp_dir):
        """Test successful file reading."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "test.py"
        content = "print('hello world')"
        test_file.write_text(content)
        
        read_content = repo.read_file(test_file)
        assert read_content == content
    
    def test_read_file_size_limit(self, test_config, temp_dir):
        """Test file size limit enforcement."""
        repo = SecureFileRepository(test_config)
        repo.config.MAX_FILE_SIZE_MB = 0.001  # 1KB limit
        
        test_file = temp_dir / "large.txt"
        content = "x" * 2000  # 2KB content
        test_file.write_text(content)
        
        with pytest.raises(FileValidationError, match="File too large"):
            repo.read_file(test_file)
    
    def test_read_file_encoding_fallback(self, test_config, temp_dir):
        """Test encoding fallback for problematic files."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "test.txt"
        # Create file with non-UTF8 content
        test_file.write_bytes(b'\xff\xfe\x00\x00')  # UTF-16 BOM
        
        # Should handle encoding issues gracefully
        content = repo.read_file(test_file)
        assert isinstance(content, str)
    
    def test_read_file_not_readable(self, test_config, temp_dir):
        """Test reading non-readable file."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "test.py"
        test_file.write_text("print('hello')")
        
        # Make file non-readable (Unix only)
        import os
        if hasattr(os, 'chmod'):
            test_file.chmod(0o000)  # No permissions
            
            with pytest.raises(FileValidationError, match="File not readable"):
                repo.read_file(test_file)
            
            # Restore permissions for cleanup
            test_file.chmod(0o644)
    
    def test_read_file_stream(self, test_config, temp_dir):
        """Test file streaming."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "test.py"
        content = "x" * 1000  # 1000 characters
        test_file.write_text(content)
        
        chunks = list(repo.read_file_stream(test_file))
        assert len(chunks) > 0
        assert "".join(chunks) == content
    
    def test_read_file_stream_binary_fallback(self, test_config, temp_dir):
        """Test streaming with binary fallback."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "test.bin"
        test_file.write_bytes(b'\xff\xfe\x00\x01')
        
        chunks = list(repo.read_file_stream(test_file))
        assert len(chunks) > 0
        # Should decode as latin-1 with error replacement
        assert all(isinstance(chunk, str) for chunk in chunks)
    
    def test_clear_cache(self, test_config, temp_dir):
        """Test cache clearing."""
        repo = SecureFileRepository(test_config)
        repo.config.ENABLE_CACHING = True
        
        test_file = temp_dir / "test.py"
        test_file.write_text("print('hello')")
        
        # Populate cache
        repo.get_file_info(test_file)
        assert len(repo._file_cache) > 0
        
        # Clear cache
        repo.clear_cache()
        assert len(repo._file_cache) == 0
    
    def test_get_cache_stats(self, test_config, temp_dir):
        """Test cache statistics."""
        repo = SecureFileRepository(test_config)
        repo.config.ENABLE_CACHING = True
        
        test_file = temp_dir / "test.py"
        test_file.write_text("print('hello')")
        
        # Populate cache
        repo.get_file_info(test_file)
        
        stats = repo.get_cache_stats()
        assert stats["cached_files"] == 1
        assert stats["cache_enabled"] == True
        assert stats["cache_size_limit"] == test_config.CACHE_SIZE


class TestRepositoryEdgeCases:
    """Test edge cases and error conditions."""
    
    def test_empty_file(self, test_config, temp_dir):
        """Test handling of empty files."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "empty.txt"
        test_file.write_text("")
        
        info = repo.get_file_info(test_file)
        assert info["size"] == 0
        
        content = repo.read_file(test_file)
        assert content == ""
    
    def test_very_long_filename(self, test_config, temp_dir):
        """Test handling of very long filenames."""
        repo = SecureFileRepository(test_config)
        
        long_name = "a" * 255 + ".txt"
        test_file = temp_dir / long_name
        test_file.write_text("content")
        
        # Should handle long filenames gracefully
        assert repo.file_exists(test_file)
        info = repo.get_file_info(test_file)
        assert info["path"].endswith(long_name)
    
    def test_special_characters_in_path(self, test_config, temp_dir):
        """Test handling of special characters in paths."""
        repo = SecureFileRepository(test_config)
        
        special_name = "test with spaces & symbols!.txt"
        test_file = temp_dir / special_name
        test_file.write_text("content")
        
        # Should handle special characters
        assert repo.file_exists(test_file)
        content = repo.read_file(test_file)
        assert content == "content"
    
    def test_concurrent_file_modification(self, test_config, temp_dir):
        """Test behavior when file is modified during processing."""
        repo = SecureFileRepository(test_config)
        
        test_file = temp_dir / "test.py"
        test_file.write_text("original")
        
        # Get file info
        info1 = repo.get_file_info(test_file)
        
        # Modify file
        test_file.write_text("modified")
        
        # Get file info again
        info2 = repo.get_file_info(test_file)
        
        # Should reflect the change
        assert info1["size"] != info2["size"]
    
    def test_nonexistent_directory(self, test_config):
        """Test handling of nonexistent directories."""
        repo = SecureFileRepository(test_config)
        
        nonexistent_file = Path("nonexistent/dir/file.py")
        
        assert not repo.file_exists(nonexistent_file)
        
        with pytest.raises(FileValidationError):
            repo.get_file_info(nonexistent_file)
    
    def test_permission_denied_directory(self, test_config, temp_dir):
        """Test handling of permission denied directories."""
        repo = SecureFileRepository(test_config)
        
        # Create a directory and make it non-readable
        restricted_dir = temp_dir / "restricted"
        restricted_dir.mkdir()
        
        import os
        if hasattr(os, 'chmod'):
            restricted_dir.chmod(0o000)  # No permissions
            
            test_file = restricted_dir / "test.py"
            
            # Should handle permission errors gracefully
            assert not repo.file_exists(test_file)
            
            # Restore permissions for cleanup
            restricted_dir.chmod(0o755)
