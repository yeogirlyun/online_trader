#!/usr/bin/env python3
"""
Security tests for the mega document creation system.

This module tests security features including path traversal prevention,
file access controls, and input validation.
"""

import pytest
import tempfile
import shutil
from pathlib import Path
from unittest.mock import patch

from config.mega_doc_config import MegaDocConfig
from services.mega_document_service import MegaDocumentService
from exceptions.mega_doc_exceptions import SecurityError, FileValidationError


class TestPathTraversalSecurity:
    """Test path traversal attack prevention."""
    
    def test_basic_path_traversal_prevention(self, temp_dir):
        """Test basic path traversal prevention."""
        config = MegaDocConfig()
        config.ALLOW_PATH_TRAVERSAL = False
        service = MegaDocumentService(config)
        
        # Create legitimate file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('safe')")
        
        # Create review document with path traversal attempts
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Security Test

## Legitimate Files
- `src/main.py`

## Path Traversal Attempts
- `../../../etc/passwd`
- `..\\..\\windows\\system32\\config\\sam`
- `../config/settings.json`
- `../../../../root/.ssh/id_rsa`
- `..\\..\\..\\windows\\system32\\drivers\\etc\\hosts`
""")
        
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Security Test")
        
        # Should only process legitimate file
        assert stats["total_files_found"] == 1
        assert stats["processed_files"] == 1
        assert stats["failed_files"] == []
        
        content = output_file.read_text()
        assert "src/main.py" in content
        assert "print('safe')" in content
        
        # Should not contain any path traversal files
        assert "../../../etc/passwd" not in content
        assert "..\\..\\windows\\system32" not in content
    
    def test_unicode_path_traversal(self, temp_dir):
        """Test Unicode-based path traversal attempts."""
        config = MegaDocConfig()
        config.ALLOW_PATH_TRAVERSAL = False
        service = MegaDocumentService(config)
        
        # Create legitimate file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('safe')")
        
        # Create review document with Unicode path traversal
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Unicode Security Test

- `src/main.py`
- `..%2F..%2F..%2Fetc%2Fpasswd`  # URL encoded
- `..%5C..%5Cwindows%5Csystem32`  # Windows URL encoded
- `%2E%2E%2F%2E%2E%2Fetc%2Fpasswd`  # Double URL encoded
""")
        
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Unicode Security Test")
        
        # Should only process legitimate file
        assert stats["total_files_found"] == 1
        assert stats["processed_files"] == 1
    
    def test_depth_limit_enforcement(self, temp_dir):
        """Test path depth limit enforcement."""
        config = MegaDocConfig()
        config.MAX_DEPTH_LEVELS = 3
        service = MegaDocumentService(config)
        
        # Create legitimate file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('safe')")
        
        # Create review document with deep paths
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Depth Limit Test

- `src/main.py`  # Valid depth
- `a/b/c/file.py`  # Valid depth (3 levels)
- `a/b/c/d/file.py`  # Too deep (4 levels)
- `a/b/c/d/e/f/g/h/i/j/k/file.py`  # Way too deep
""")
        
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Depth Test")
        
        # Should only process files within depth limit
        assert stats["total_files_found"] == 2  # Only first two files
        assert stats["processed_files"] == 2
    
    def test_allow_path_traversal_configuration(self, temp_dir):
        """Test configuration option to allow path traversal."""
        config = MegaDocConfig()
        config.ALLOW_PATH_TRAVERSAL = True  # Allow traversal
        service = MegaDocumentService(config)
        
        # Create legitimate file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('safe')")
        
        # Create review document with path traversal
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Allow Traversal Test

- `src/main.py`
- `../config/settings.json`
""")
        
        # Create the referenced file
        config_dir = temp_dir / "config"
        config_dir.mkdir()
        (config_dir / "settings.json").write_text('{"key": "value"}')
        
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Allow Traversal Test")
        
        # Should process both files when traversal is allowed
        assert stats["total_files_found"] == 2
        assert stats["processed_files"] == 2


class TestFileAccessSecurity:
    """Test file access security controls."""
    
    def test_file_size_limit_security(self, temp_dir):
        """Test file size limit as security measure."""
        config = MegaDocConfig()
        config.MAX_FILE_SIZE_MB = 0.1  # Very small limit (100KB)
        service = MegaDocumentService(config)
        
        # Create large file (potential DoS attack)
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        large_content = "x" * (200 * 1024)  # 200KB
        (src_dir / "large.py").write_text(large_content)
        
        # Create review document
        review_file = temp_dir / "review.md"
        review_file.write_text("- `src/large.py`")
        
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Size Limit Test")
        
        # Should reject large file
        assert stats["total_files_found"] == 1
        assert stats["processed_files"] == 0
        assert len(stats["failed_files"]) == 1
        assert "too large" in stats["failed_files"][0][1].lower()
    
    def test_file_extension_validation(self, temp_dir):
        """Test file extension validation as security measure."""
        config = MegaDocConfig()
        config.SUPPORTED_EXTENSIONS = frozenset(['py', 'txt'])  # Only allow specific extensions
        service = MegaDocumentService(config)
        
        # Create files with different extensions
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('safe')")
        (src_dir / "data.txt").write_text("some data")
        (src_dir / "malicious.exe").write_text("malicious content")
        (src_dir / "script.sh").write_text("#!/bin/bash\necho 'malicious'")
        
        # Create review document
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Extension Validation Test

- `src/main.py`
- `src/data.txt`
- `src/malicious.exe`
- `src/script.sh`
""")
        
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Extension Test")
        
        # Should only process allowed extensions
        assert stats["total_files_found"] == 2  # Only py and txt
        assert stats["processed_files"] == 2
        
        content = output_file.read_text()
        assert "main.py" in content
        assert "data.txt" in content
        assert "malicious.exe" not in content
        assert "script.sh" not in content
    
    def test_symlink_security(self, temp_dir):
        """Test security against symbolic link attacks."""
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        
        # Create legitimate file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('safe')")
        
        # Create symlink (if supported)
        try:
            symlink_file = temp_dir / "symlink.py"
            symlink_file.symlink_to(src_dir / "main.py")
            
            # Create review document
            review_file = temp_dir / "review.md"
            review_file.write_text("""
# Symlink Test

- `src/main.py`
- `symlink.py`
""")
            
            output_file = temp_dir / "output.md"
            stats = service.create_mega_document(review_file, output_file, "Symlink Test")
            
            # Should handle symlinks safely
            assert stats["total_files_found"] == 2
            assert stats["processed_files"] == 2
            
        except OSError:
            # Symlinks not supported on this system
            pytest.skip("Symbolic links not supported on this system")


class TestInputValidationSecurity:
    """Test input validation security measures."""
    
    def test_malicious_review_content(self, temp_dir):
        """Test handling of malicious review content."""
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        
        # Create legitimate file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('safe')")
        
        # Create review document with malicious content
        review_file = temp_dir / "review.md"
        malicious_content = """
# Malicious Review

- `src/main.py`

<script>alert('xss')</script>
<img src="x" onerror="alert('xss')">
<iframe src="javascript:alert('xss')"></iframe>

## SQL Injection Attempt
'; DROP TABLE users; --

## Command Injection Attempt
`; rm -rf /; echo 'hacked'`

## Path Traversal
- `../../../etc/passwd`
- `..\\..\\windows\\system32\\config\\sam`
"""
        review_file.write_text(malicious_content)
        
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Malicious Content Test")
        
        # Should only process legitimate file
        assert stats["total_files_found"] == 1
        assert stats["processed_files"] == 1
        
        content = output_file.read_text()
        assert "src/main.py" in content
        assert "print('safe')" in content
        
        # Should not contain malicious content
        assert "<script>" not in content
        assert "DROP TABLE" not in content
        assert "rm -rf" not in content
    
    def test_very_long_file_paths(self, temp_dir):
        """Test handling of very long file paths."""
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        
        # Create legitimate file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('safe')")
        
        # Create review document with very long paths
        review_file = temp_dir / "review.md"
        long_path = "a" + "/b" * 100 + "/file.py"  # Very long path
        review_file.write_text(f"""
# Long Path Test

- `src/main.py`
- `{long_path}`
""")
        
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Long Path Test")
        
        # Should handle long paths gracefully
        assert stats["total_files_found"] == 1  # Only legitimate file
        assert stats["processed_files"] == 1
    
    def test_null_byte_injection(self, temp_dir):
        """Test null byte injection prevention."""
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        
        # Create legitimate file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('safe')")
        
        # Create review document with null bytes
        review_file = temp_dir / "review.md"
        review_file.write_bytes(b"""
# Null Byte Test

- `src/main.py`
- `src/main.py\x00.txt`
- `src/main.py\x00.exe`
""")
        
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Null Byte Test")
        
        # Should only process legitimate file
        assert stats["total_files_found"] == 1
        assert stats["processed_files"] == 1


class TestConfigurationSecurity:
    """Test configuration-related security."""
    
    def test_secure_default_configuration(self):
        """Test that default configuration is secure."""
        config = MegaDocConfig()
        
        # Verify secure defaults
        assert config.ALLOW_PATH_TRAVERSAL == False
        assert config.MAX_DEPTH_LEVELS > 0
        assert config.MAX_FILE_SIZE_MB > 0
        assert len(config.SUPPORTED_EXTENSIONS) > 0
        
        # Verify no dangerous extensions by default
        dangerous_extensions = {'exe', 'bat', 'cmd', 'scr', 'pif', 'com'}
        assert not config.SUPPORTED_EXTENSIONS.intersection(dangerous_extensions)
    
    def test_configuration_validation(self):
        """Test configuration validation."""
        # Test with invalid configuration values
        config = MegaDocConfig()
        
        # Test negative values
        config.MAX_FILE_SIZE_MB = -1
        assert config.get_max_file_size_bytes() == -1024 * 1024  # Should handle gracefully
        
        config.MAX_DEPTH_LEVELS = 0
        # Should still work but be restrictive
        
        # Test empty extensions
        config.SUPPORTED_EXTENSIONS = frozenset()
        assert config.validate_file_extension("file.py") == False
    
    def test_configuration_serialization_security(self):
        """Test configuration serialization doesn't expose sensitive data."""
        config = MegaDocConfig()
        config_dict = config.to_dict()
        
        # Should not contain sensitive information
        sensitive_keys = {'password', 'secret', 'key', 'token', 'auth'}
        for key in config_dict.keys():
            assert not any(sensitive in key.lower() for sensitive in sensitive_keys)
        
        # Should be serializable to JSON
        import json
        json_str = json.dumps(config_dict)
        assert isinstance(json_str, str)


class TestErrorHandlingSecurity:
    """Test security aspects of error handling."""
    
    def test_error_message_information_disclosure(self, temp_dir):
        """Test that error messages don't disclose sensitive information."""
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        
        # Create review document with path traversal
        review_file = temp_dir / "review.md"
        review_file.write_text("- `../../../etc/passwd`")
        
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Error Test")
        
        # Should not process the file
        assert stats["total_files_found"] == 0
        assert stats["processed_files"] == 0
        
        # Error messages should not contain sensitive paths
        if stats["failed_files"]:
            for _, error_msg in stats["failed_files"]:
                assert "/etc/passwd" not in error_msg
                assert "passwd" not in error_msg.lower()
    
    def test_logging_security(self, temp_dir):
        """Test that logging doesn't expose sensitive information."""
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        
        # Create review document with sensitive content
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Sensitive Content Test

- `src/main.py`

## Sensitive Information
Password: secret123
API Key: sk-1234567890abcdef
Database URL: postgresql://user:pass@localhost/db
""")
        
        # Create legitimate file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('safe')")
        
        # Mock logger to capture log messages
        with patch('logging.getLogger') as mock_logger:
            mock_logger_instance = Mock()
            mock_logger.return_value = mock_logger_instance
            
            output_file = temp_dir / "output.md"
            stats = service.create_mega_document(review_file, output_file, "Sensitive Test")
            
            # Check that sensitive information is not logged
            log_calls = mock_logger_instance.info.call_args_list + mock_logger_instance.error.call_args_list
            for call in log_calls:
                log_message = str(call)
                assert "secret123" not in log_message
                assert "sk-1234567890abcdef" not in log_message
                assert "postgresql://user:pass" not in log_message
