#!/usr/bin/env python3
"""
Integration tests for the mega document creation system.

This module tests the complete workflow from review document to mega document,
including real file system operations and error scenarios.
"""

import pytest
import tempfile
import shutil
from pathlib import Path
from unittest.mock import patch

from config.mega_doc_config import MegaDocConfig
from services.mega_document_service import MegaDocumentService
from exceptions.mega_doc_exceptions import (
    FileValidationError, SecurityError, ExtractionError, BuildError
)


class TestFullWorkflow:
    """Test complete workflow integration."""
    
    def test_real_file_system_workflow(self, temp_dir):
        """Test complete workflow with real file system."""
        # Create test project structure
        src_dir = temp_dir / "src"
        include_dir = temp_dir / "include"
        config_dir = temp_dir / "config"
        docs_dir = temp_dir / "docs"
        
        src_dir.mkdir()
        include_dir.mkdir()
        config_dir.mkdir()
        docs_dir.mkdir()
        
        # Create source files
        (src_dir / "main.py").write_text("""
#!/usr/bin/env python3
\"\"\"Main application entry point.\"\"\"

def main():
    print("Hello, World!")

if __name__ == "__main__":
    main()
""")
        
        (src_dir / "utils.py").write_text("""
\"\"\"Utility functions.\"\"\"

def helper_function():
    \"\"\"A helpful function.\"\"\"
    return True
""")
        
        (include_dir / "header.h").write_text("""
#pragma once

int main();
void helper_function();
""")
        
        (config_dir / "settings.json").write_text("""
{
    "debug": true,
    "version": "1.0.0",
    "features": ["logging", "caching"]
}
""")
        
        (docs_dir / "README.md").write_text("""
# Test Project

This is a test project for integration testing.

## Features

- Python source files
- C++ header files
- JSON configuration
- Documentation
""")
        
        # Create review document
        review_file = temp_dir / "project_review.md"
        review_file.write_text("""
# Project Code Review

## Overview

This document reviews the test project structure and implementation.

## Source Files

The following source files are included in this review:

- `src/main.py` - Main application entry point
- `src/utils.py` - Utility functions
- `include/header.h` - C++ header file
- `config/settings.json` - Configuration settings

## Documentation

See [README.md](docs/README.md) for project documentation.

## Code Examples

```python
# From src/main.py
def main():
    print("Hello, World!")
```

## Configuration

The project uses JSON configuration:

```json
{
    "debug": true,
    "version": "1.0.0"
}
```

## Analysis

The code structure looks good with proper separation of concerns.
""")
        
        # Create mega document
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        output_file = temp_dir / "project_mega.md"
        
        stats = service.create_mega_document(review_file, output_file, "Project Code Review")
        
        # Verify results
        assert stats["total_files_found"] == 5  # All 5 files should be found
        assert stats["processed_files"] == 5
        assert stats["failed_files"] == []
        assert stats["processing_time_seconds"] > 0
        
        # Verify output file exists and has content
        assert output_file.exists()
        content = output_file.read_text()
        assert len(content) > 1000  # Should be substantial
        
        # Verify all expected files are included
        expected_files = [
            "src/main.py",
            "src/utils.py", 
            "include/header.h",
            "config/settings.json",
            "docs/README.md"
        ]
        
        for file_path in expected_files:
            assert file_path in content
            assert f"## 📄 **FILE" in content  # File sections should be present
        
        # Verify document structure
        assert "# Project Code Review" in content
        assert "**Generated**: " in content
        assert "## 📋 **TABLE OF CONTENTS**" in content
        
        # Verify file contents are included
        assert "Hello, World!" in content
        assert "helper_function" in content
        assert "#pragma once" in content
        assert '"debug": true' in content
        assert "# Test Project" in content
    
    def test_workflow_with_errors(self, temp_dir):
        """Test workflow with various error conditions."""
        # Create some valid files
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('hello')")
        
        # Create review document with mix of valid and invalid references
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Review with Errors

## Valid Files
- `src/main.py`

## Invalid Files
- `nonexistent/file.py`
- `../../../etc/passwd`
- `file with spaces.py`
- `http://example.com/file.py`
- `justfilename.py`
""")
        
        # Create mega document
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        output_file = temp_dir / "output.md"
        
        stats = service.create_mega_document(review_file, output_file, "Error Test")
        
        # Verify results
        assert stats["total_files_found"] == 1  # Only valid file found
        assert stats["processed_files"] == 1
        assert stats["failed_files"] == []  # Invalid files are filtered out, not failed
        
        # Verify output
        assert output_file.exists()
        content = output_file.read_text()
        assert "src/main.py" in content
        assert "print('hello')" in content
    
    def test_workflow_with_large_files(self, temp_dir):
        """Test workflow with larger files."""
        # Create large files
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        
        # Create a large Python file
        large_content = "print('line')\n" * 1000  # 1000 lines
        (src_dir / "large.py").write_text(large_content)
        
        # Create a large JSON file
        large_json = '{"data": "' + "x" * 10000 + '"}'  # Large JSON
        (src_dir / "large.json").write_text(large_json)
        
        # Create review document
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Large Files Review

- `src/large.py`
- `src/large.json`
""")
        
        # Create mega document
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        output_file = temp_dir / "output.md"
        
        stats = service.create_mega_document(review_file, output_file, "Large Files Test")
        
        # Verify results
        assert stats["total_files_found"] == 2
        assert stats["processed_files"] == 2
        assert stats["failed_files"] == []
        
        # Verify output
        assert output_file.exists()
        content = output_file.read_text()
        assert len(content) > 20000  # Should be very large
        
        # Verify file contents are included
        assert "print('line')" in content
        assert '"data": "' in content
    
    def test_workflow_with_different_encodings(self, temp_dir):
        """Test workflow with files of different encodings."""
        # Create files with different encodings
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        
        # UTF-8 file
        (src_dir / "utf8.py").write_text("print('Hello, 世界!')", encoding='utf-8')
        
        # Latin-1 file (simulate)
        (src_dir / "latin1.txt").write_bytes(b'Hello, \xe9\xe0\xe7!')  # Latin-1 encoded
        
        # Create review document
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Encoding Test

- `src/utf8.py`
- `src/latin1.txt`
""")
        
        # Create mega document
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        output_file = temp_dir / "output.md"
        
        stats = service.create_mega_document(review_file, output_file, "Encoding Test")
        
        # Verify results
        assert stats["total_files_found"] == 2
        assert stats["processed_files"] == 2
        assert stats["failed_files"] == []
        
        # Verify output
        assert output_file.exists()
        content = output_file.read_text()
        
        # Should handle different encodings gracefully
        assert "utf8.py" in content
        assert "latin1.txt" in content


class TestErrorScenarios:
    """Test various error scenarios."""
    
    def test_missing_review_file(self, temp_dir):
        """Test handling of missing review file."""
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        output_file = temp_dir / "output.md"
        nonexistent_review = temp_dir / "nonexistent.md"
        
        with pytest.raises(FileValidationError):
            service.create_mega_document(nonexistent_review, output_file, "Test")
    
    def test_read_only_output_directory(self, temp_dir):
        """Test handling of read-only output directory."""
        # Create read-only directory
        readonly_dir = temp_dir / "readonly"
        readonly_dir.mkdir()
        readonly_dir.chmod(0o444)  # Read-only
        
        try:
            config = MegaDocConfig()
            service = MegaDocumentService(config)
            
            # Create test files
            src_dir = temp_dir / "src"
            src_dir.mkdir()
            (src_dir / "main.py").write_text("print('hello')")
            
            review_file = temp_dir / "review.md"
            review_file.write_text("- `src/main.py`")
            
            output_file = readonly_dir / "output.md"
            
            with pytest.raises(BuildError):
                service.create_mega_document(review_file, output_file, "Test")
        finally:
            # Restore permissions for cleanup
            readonly_dir.chmod(0o755)
    
    def test_file_size_limit_exceeded(self, temp_dir):
        """Test handling of files exceeding size limit."""
        # Create large file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        
        large_content = "x" * (2 * 1024 * 1024)  # 2MB
        (src_dir / "large.py").write_text(large_content)
        
        # Create review document
        review_file = temp_dir / "review.md"
        review_file.write_text("- `src/large.py`")
        
        # Create service with small size limit
        config = MegaDocConfig()
        config.MAX_FILE_SIZE_MB = 1  # 1MB limit
        service = MegaDocumentService(config)
        output_file = temp_dir / "output.md"
        
        stats = service.create_mega_document(review_file, output_file, "Size Test")
        
        # Should fail to process the large file
        assert stats["total_files_found"] == 1
        assert stats["processed_files"] == 0
        assert len(stats["failed_files"]) == 1
        assert "large.py" in stats["failed_files"][0][0]
    
    def test_security_path_traversal(self, temp_dir):
        """Test security against path traversal."""
        config = MegaDocConfig()
        config.ALLOW_PATH_TRAVERSAL = False  # Strict security
        service = MegaDocumentService(config)
        
        # Create review document with path traversal attempts
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Security Test

- `../../../etc/passwd`
- `..\\..\\windows\\system32\\config\\sam`
- `src/legitimate.py`
""")
        
        # Create legitimate file
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "legitimate.py").write_text("print('safe')")
        
        output_file = temp_dir / "output.md"
        
        stats = service.create_mega_document(review_file, output_file, "Security Test")
        
        # Should only process legitimate file
        assert stats["total_files_found"] == 1
        assert stats["processed_files"] == 1
        assert stats["failed_files"] == []
        
        content = output_file.read_text()
        assert "legitimate.py" in content
        assert "print('safe')" in content


class TestPerformanceIntegration:
    """Test performance characteristics."""
    
    def test_processing_time_measurement(self, temp_dir):
        """Test that processing time is measured correctly."""
        # Create multiple files
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        
        for i in range(10):
            (src_dir / f"file{i}.py").write_text(f"print('file {i}')")
        
        # Create review document
        review_file = temp_dir / "review.md"
        file_list = "\n".join(f"- `src/file{i}.py`" for i in range(10))
        review_file.write_text(f"# Performance Test\n\n{file_list}")
        
        # Create mega document
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        output_file = temp_dir / "output.md"
        
        stats = service.create_mega_document(review_file, output_file, "Performance Test")
        
        # Verify timing
        assert stats["processing_time_seconds"] > 0
        assert stats["processing_time_seconds"] < 10  # Should be reasonably fast
        
        # Verify all files processed
        assert stats["total_files_found"] == 10
        assert stats["processed_files"] == 10
    
    def test_memory_usage_with_large_documents(self, temp_dir):
        """Test memory usage with large documents."""
        # Create large files
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        
        for i in range(5):
            content = "x" * 10000  # 10KB per file
            (src_dir / f"large{i}.txt").write_text(content)
        
        # Create review document
        review_file = temp_dir / "review.md"
        file_list = "\n".join(f"- `src/large{i}.txt`" for i in range(5))
        review_file.write_text(f"# Memory Test\n\n{file_list}")
        
        # Create mega document
        config = MegaDocConfig()
        service = MegaDocumentService(config)
        output_file = temp_dir / "output.md"
        
        stats = service.create_mega_document(review_file, output_file, "Memory Test")
        
        # Verify processing completed
        assert stats["total_files_found"] == 5
        assert stats["processed_files"] == 5
        
        # Verify output size
        assert output_file.exists()
        output_size = output_file.stat().st_size
        assert output_size > 50000  # Should be substantial


class TestConfigurationIntegration:
    """Test configuration integration."""
    
    def test_custom_configuration(self, temp_dir):
        """Test with custom configuration."""
        # Create custom config
        config = MegaDocConfig()
        config.MAX_PREVIEW_FILES = 3
        config.MAX_ERROR_PREVIEW = 2
        config.SUPPORTED_EXTENSIONS = frozenset(['py', 'txt'])  # Only Python and text files
        
        # Create test files
        src_dir = temp_dir / "src"
        src_dir.mkdir()
        (src_dir / "main.py").write_text("print('hello')")
        (src_dir / "data.txt").write_text("some data")
        (src_dir / "config.json").write_text('{"key": "value"}')  # Should be ignored
        
        # Create review document
        review_file = temp_dir / "review.md"
        review_file.write_text("""
# Custom Config Test

- `src/main.py`
- `src/data.txt`
- `src/config.json`
""")
        
        # Create service with custom config
        service = MegaDocumentService(config)
        output_file = temp_dir / "output.md"
        
        stats = service.create_mega_document(review_file, output_file, "Custom Config Test")
        
        # Should only process Python and text files
        assert stats["total_files_found"] == 2  # Only py and txt files
        assert stats["processed_files"] == 2
        
        content = output_file.read_text()
        assert "main.py" in content
        assert "data.txt" in content
        assert "config.json" not in content  # Should be filtered out
    
    def test_configuration_serialization(self, temp_dir):
        """Test configuration serialization and deserialization."""
        # Create original config
        original_config = MegaDocConfig()
        original_config.MAX_PREVIEW_FILES = 5
        original_config.MAX_FILE_SIZE_MB = 50
        
        # Serialize to dict
        config_dict = original_config.to_dict()
        
        # Deserialize to new config
        new_config = MegaDocConfig.from_dict(config_dict)
        
        # Verify they're equivalent
        assert new_config.MAX_PREVIEW_FILES == original_config.MAX_PREVIEW_FILES
        assert new_config.MAX_FILE_SIZE_MB == original_config.MAX_FILE_SIZE_MB
        assert new_config.SUPPORTED_EXTENSIONS == original_config.SUPPORTED_EXTENSIONS
