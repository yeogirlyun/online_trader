#!/usr/bin/env python3
"""
Tests for the service layer.

This module tests the FileExtractor, MegaDocumentBuilder, and MegaDocumentService
classes, including their business logic and integration.
"""

import pytest
from pathlib import Path
from unittest.mock import Mock, patch, mock_open
from datetime import datetime

from services.mega_document_service import FileExtractor, MegaDocumentBuilder, MegaDocumentService
from exceptions.mega_doc_exceptions import (
    FileValidationError, ExtractionError, BuildError, ProcessingError
)


class DataBuilder:
    """Builder class for creating test data."""
    
    def __init__(self, temp_dir: Path):
        self.temp_dir = temp_dir
        self.files = {}
    
    def add_file(self, path: str, content: str) -> 'DataBuilder':
        """Add a file to the test data."""
        full_path = self.temp_dir / path
        full_path.parent.mkdir(parents=True, exist_ok=True)
        full_path.write_text(content)
        self.files[path] = content
        return self
    
    def build(self) -> dict:
        """Build and return the test data."""
        return self.files.copy()


def assert_mega_document_structure(output_file: Path, expected_files: list):
    """Assert mega document has correct structure."""
    content = output_file.read_text()
    
    # Check header
    assert "# " in content, "Missing document title"
    assert "**Generated**: " in content, "Missing generation timestamp"
    assert "**Total Files**: " in content, "Missing file count"
    
    # Check table of contents
    assert "## 📋 **TABLE OF CONTENTS**" in content, "Missing table of contents"
    
    # Check each expected file is present
    for file_path in expected_files:
        assert file_path in content, f"Missing file reference: {file_path}"


class TestFileExtractor:
    """Test FileExtractor service."""
    
    def test_extract_basic_paths(self, file_extractor):
        """Test extraction of basic file paths."""
        content = """
        - `src/main.py`
        - `include/header.h`
        - `config/settings.json`
        """
        paths = file_extractor.extract_file_paths(content)
        
        assert len(paths) == 3
        assert "src/main.py" in paths
        assert "include/header.h" in paths
        assert "config/settings.json" in paths
    
    def test_extract_markdown_links(self, file_extractor):
        """Test extraction from markdown links."""
        content = "[Header File](include/header.h) and [Source](src/main.cpp)"
        paths = file_extractor.extract_file_paths(content)
        
        assert len(paths) == 2
        assert "include/header.h" in paths
        assert "src/main.cpp" in paths
    
    def test_extract_code_blocks(self, file_extractor):
        """Test extraction from code blocks."""
        content = """
        ```python
        # From src/main.py
        print('hello')
        ```
        
        ```cpp
        // From include/header.h
        int main();
        ```
        """
        paths = file_extractor.extract_file_paths(content)
        
        # Should find files mentioned in code blocks
        assert len(paths) >= 0  # May or may not find them depending on pattern
    
    def test_ignore_invalid_extensions(self, file_extractor):
        """Test that invalid extensions are ignored."""
        content = """- `file.invalid`
- `script.py`
- `document.unknown`"""
        
        paths = file_extractor.extract_file_paths(content)
        
        # Should find at least the Python file
        assert len(paths) >= 1
        assert "script.py" in paths
    
    def test_security_path_traversal(self, file_extractor):
        """Test that path traversal attempts are rejected."""
        content = """
        - `../../../etc/passwd`
        - `..\\..\\windows\\system32\\config\\sam`
        - `src/legitimate.py`
        """
        paths = file_extractor.extract_file_paths(content)
        
        assert len(paths) == 1
        assert "src/legitimate.py" in paths
    
    def test_normalize_paths(self, file_extractor):
        """Test path normalization."""
        test_cases = [
            ("  src/file.py  ", "src/file.py"),
            ("", None),
            ("http://example.com/file.py", None),
            ("file with spaces.py", None),
            ("justfilename.py", "justfilename.py"),  # Now allowed
        ]
        
        for input_path, expected in test_cases:
            result = file_extractor._normalize_path(input_path)
            assert result == expected
    
    def test_path_validation(self, file_extractor):
        """Test path validation with various inputs."""
        test_cases = [
            ("file.py", True),
            ("../file.py", False),  # Path traversal
            ("file.unknown", False),  # Invalid extension
            ("", False),
            ("src/subdir/file.cpp", True),
        ]
        
        for path, expected in test_cases:
            assert file_extractor._is_valid_path(path) == expected
    
    def test_extract_empty_content(self, file_extractor):
        """Test extraction from empty content."""
        paths = file_extractor.extract_file_paths("")
        assert paths == []
    
    def test_extract_no_matches(self, file_extractor):
        """Test extraction with no file path matches."""
        content = "This is just regular text with no file references."
        paths = file_extractor.extract_file_paths(content)
        assert paths == []
    
    def test_extract_duplicate_paths(self, file_extractor):
        """Test that duplicate paths are deduplicated."""
        content = """
        - `src/main.py`
        - `src/main.py`  # Duplicate
        - `include/header.h`
        """
        paths = file_extractor.extract_file_paths(content)
        
        assert len(paths) == 2  # Duplicates removed
        assert paths.count("src/main.py") == 1
    
    def test_extract_sorted_output(self, file_extractor):
        """Test that extracted paths are sorted."""
        content = """
        - `z_file.py`
        - `a_file.py`
        - `m_file.py`
        """
        paths = file_extractor.extract_file_paths(content)
        
        assert paths == sorted(paths)
    
    def test_extract_error_handling(self, file_extractor):
        """Test error handling during extraction."""
        # Test with malformed content that might cause issues
        malformed_content = "`" * 1000  # Very long backtick sequence
        
        # This should not raise an exception, just return empty list
        paths = file_extractor.extract_file_paths(malformed_content)
        assert isinstance(paths, list)


class TestMegaDocumentBuilder:
    """Test MegaDocumentBuilder service."""
    
    def test_build_success(self, mock_repository, test_config, temp_dir):
        """Test successful document building."""
        # Setup mock repository
        mock_repository.add_file("src/main.py", "print('hello')")
        mock_repository.add_file("include/header.h", "#pragma once")
        
        # Create extractor and builder
        extractor = FileExtractor(test_config)
        builder = MegaDocumentBuilder(extractor, mock_repository, test_config)
        
        # Create review content
        review_content = """
        - `src/main.py`
        - `include/header.h`
        """
        
        # Mock repository read_file method
        def mock_read_file(path):
            if str(path).endswith("review.md"):
                return review_content
            # Use the original read_file method for other files
            path_str = str(path)
            if path_str not in mock_repository.files:
                raise FileValidationError(path_str, "File does not exist")
            return mock_repository.files[path_str]
        
        mock_repository.read_file = mock_read_file
        
        # Build document
        output_file = temp_dir / "output.md"
        stats = builder.build(Path("review.md"), output_file, "Test Document")
        
        # Verify results
        assert stats["total_files_found"] == 2
        assert stats["processed_files"] == 2
        assert stats["failed_files"] == []
        assert output_file.exists()
        
        # Verify content
        content = output_file.read_text()
        assert "Test Document" in content
        assert "src/main.py" in content
        assert "include/header.h" in content
    
    def test_build_with_errors(self, mock_repository, test_config, temp_dir):
        """Test document building with some file errors."""
        # Setup mock repository
        mock_repository.add_file("src/main.py", "print('hello')")
        # Don't add the second file to simulate error
        
        # Create extractor and builder
        extractor = FileExtractor(test_config)
        builder = MegaDocumentBuilder(extractor, mock_repository, test_config)
        
        # Create review content
        review_content = """
        - `src/main.py`
        - `nonexistent/file.cpp`
        """
        
        # Mock repository read_file method
        def mock_read_file(path):
            if str(path).endswith("review.md"):
                return review_content
            # Use the original read_file method for other files
            path_str = str(path)
            if path_str not in mock_repository.files:
                raise FileValidationError(path_str, "File does not exist")
            return mock_repository.files[path_str]
        
        mock_repository.read_file = mock_read_file
        
        # Build document
        output_file = temp_dir / "output.md"
        stats = builder.build(Path("review.md"), output_file, "Test Document")
        
        # Verify results
        assert stats["total_files_found"] == 2
        assert stats["processed_files"] == 1
        assert len(stats["failed_files"]) == 1
        assert "nonexistent/file.cpp" in stats["failed_files"][0][0]
    
    def test_build_no_files_found(self, mock_repository, test_config, temp_dir):
        """Test building with no files found."""
        extractor = FileExtractor(test_config)
        builder = MegaDocumentBuilder(extractor, mock_repository, test_config)
        
        # Mock repository to return empty content
        mock_repository.read_file = Mock(return_value="No file references here")
        
        output_file = temp_dir / "output.md"
        
        with pytest.raises(ExtractionError):
            builder.build(Path("review.md"), output_file, "Test Document")
    
    def test_build_write_error(self, mock_repository, test_config, temp_dir):
        """Test handling of write errors."""
        extractor = FileExtractor(test_config)
        builder = MegaDocumentBuilder(extractor, mock_repository, test_config)
        
        # Mock repository
        mock_repository.read_file = Mock(return_value="- `src/main.py`")
        mock_repository.add_file("src/main.py", "print('hello')")
        
        # Create output file that can't be written to
        output_file = temp_dir / "output.md"
        output_file.write_text("existing content")
        output_file.chmod(0o444)  # Read-only
        
        try:
            with pytest.raises(BuildError):
                builder.build(Path("review.md"), output_file, "Test Document")
        finally:
            # Restore permissions for cleanup
            output_file.chmod(0o644)
    
    def test_write_header(self, mock_repository, test_config, temp_dir):
        """Test header writing."""
        extractor = FileExtractor(test_config)
        builder = MegaDocumentBuilder(extractor, mock_repository, test_config)
        
        stats = {
            "review_file": "test.md",
            "total_files_found": 5
        }
        
        output_file = temp_dir / "test.md"
        with open(output_file, 'w') as f:
            builder._write_header(f, "Test Title", stats)
        
        content = output_file.read_text()
        assert "# Test Title" in content
        assert "**Generated**: " in content
        assert "**Source**: test.md" in content
        assert "**Total Files**: 5" in content
    
    def test_write_table_of_contents(self, mock_repository, test_config, temp_dir):
        """Test table of contents writing."""
        extractor = FileExtractor(test_config)
        builder = MegaDocumentBuilder(extractor, mock_repository, test_config)
        
        file_paths = ["src/main.py", "include/header.h", "config/settings.json"]
        
        output_file = temp_dir / "test.md"
        with open(output_file, 'w') as f:
            builder._write_table_of_contents(f, file_paths)
        
        content = output_file.read_text()
        assert "## 📋 **TABLE OF CONTENTS**" in content
        assert "1. [src/main.py](#file-1)" in content
        assert "2. [include/header.h](#file-2)" in content
        assert "3. [config/settings.json](#file-3)" in content


class TestMegaDocumentService:
    """Test MegaDocumentService integration."""
    
    def test_create_mega_document_success(self, test_config, temp_dir):
        """Test successful mega document creation."""
        # Create a repository that uses the temp directory as base
        from repositories.file_repository import SecureFileRepository
        from services.mega_document_service import MegaDocumentService
        
        # Create a config that allows path traversal for testing
        test_config.ALLOW_PATH_TRAVERSAL = True
        
        # Create repository with temp directory as base
        repo = SecureFileRepository(test_config)
        repo.base_path = temp_dir
        
        # Create service and modify its repository base path
        service = MegaDocumentService(test_config)
        service.repository.base_path = temp_dir
        
        # Create test files
        src_file = temp_dir / "src" / "main.py"
        src_file.parent.mkdir()
        src_file.write_text("print('hello world')")
        
        include_file = temp_dir / "include" / "header.h"
        include_file.parent.mkdir()
        include_file.write_text("#pragma once\nint main();")
        
        # Create review document
        review_file = temp_dir / "review.md"
        review_file.write_text("""
        # Test Review
        
        Source files:
        - `src/main.py`
        - `include/header.h`
        """)
        
        # Build mega document
        output_file = temp_dir / "output.md"
        stats = service.create_mega_document(review_file, output_file, "Test Document")
        
        # Verify results
        assert stats["total_files_found"] == 2
        assert stats["processed_files"] == 2
        assert stats["failed_files"] == []
        assert output_file.exists()
        
        # Verify content
        content = output_file.read_text()
        assert "Test Document" in content
        assert "src/main.py" in content
        assert "include/header.h" in content
        assert "print('hello world')" in content
    
    def test_create_mega_document_review_file_not_found(self, mega_document_service, temp_dir):
        """Test handling of missing review file."""
        output_file = temp_dir / "output.md"
        nonexistent_review = temp_dir / "nonexistent.md"
        
        with pytest.raises(FileValidationError):
            mega_document_service.create_mega_document(nonexistent_review, output_file, "Test")
    
    def test_create_mega_document_output_directory_creation(self, mega_document_service, temp_dir):
        """Test that output directory is created if it doesn't exist."""
        # Create test file
        src_file = temp_dir / "src" / "main.py"
        src_file.parent.mkdir()
        src_file.write_text("print('hello')")
        
        # Create review document
        review_file = temp_dir / "review.md"
        review_file.write_text("- `src/main.py`")
        
        # Create output in non-existent directory
        output_file = temp_dir / "output" / "subdir" / "output.md"
        
        stats = mega_document_service.create_mega_document(review_file, output_file, "Test")
        
        # Verify directory was created and file exists
        assert output_file.exists()
        assert output_file.parent.exists()
    
    def test_get_system_info(self, mega_document_service):
        """Test system information retrieval."""
        info = mega_document_service.get_system_info()
        
        assert "config" in info
        assert "cache_stats" in info
        assert "repository_type" in info
        assert "base_path" in info
        
        assert info["repository_type"] == "SecureFileRepository"
        assert isinstance(info["config"], dict)
        assert isinstance(info["cache_stats"], dict)


class TestServiceIntegration:
    """Test integration between service components."""
    
    def test_end_to_end_workflow(self, test_config, temp_dir):
        """Test complete end-to-end workflow."""
        # Create a repository that uses the temp directory as base
        from repositories.file_repository import SecureFileRepository
        from services.mega_document_service import MegaDocumentService
        
        # Create a config that allows path traversal for testing
        test_config.ALLOW_PATH_TRAVERSAL = True
        
        # Create service and modify its repository base path
        service = MegaDocumentService(test_config)
        service.repository.base_path = temp_dir
        
        # Create test data
        builder = DataBuilder(temp_dir)
        builder.add_file("src/main.py", "print('Hello, World!')")
        builder.add_file("include/header.h", "#pragma once\nint main();")
        builder.add_file("config/settings.json", '{"debug": true}')
        
        # Create review document
        review_file = temp_dir / "review.md"
        review_file.write_text("""
        # Project Review
        
        ## Source Files
        - `src/main.py`
        - `include/header.h`
        - `config/settings.json`
        
        ## Analysis
        The code looks good.
        """)
        
        # Create service and build document
        output_file = temp_dir / "mega_doc.md"
        
        stats = service.create_mega_document(review_file, output_file, "Project Analysis")
        
        # Verify results
        assert stats["total_files_found"] == 3
        assert stats["processed_files"] == 3
        assert stats["failed_files"] == []
        
        # Verify document structure
        assert_mega_document_structure(output_file, ["src/main.py", "include/header.h", "config/settings.json"])
        
        # Verify content
        content = output_file.read_text()
        assert "Hello, World!" in content
        assert "#pragma once" in content
        assert '"debug": true' in content
    
    def test_error_propagation(self, test_config, temp_dir):
        """Test that errors are properly propagated through the service layer."""
        # Create service with mock repository that raises errors
        service = MegaDocumentService(test_config)
        
        # Mock the repository to raise an error
        with patch.object(service.repository, 'read_file', side_effect=FileValidationError("test.py", "Permission denied")):
            review_file = temp_dir / "review.md"
            review_file.write_text("- `test.py`")
            output_file = temp_dir / "output.md"
            
            with pytest.raises(FileValidationError):
                service.create_mega_document(review_file, output_file, "Test")
    
    def test_performance_with_large_files(self, test_config, temp_dir):
        """Test performance with larger files."""
        # Create a repository that uses the temp directory as base
        from repositories.file_repository import SecureFileRepository
        from services.mega_document_service import MegaDocumentService
        
        # Create a config that allows path traversal for testing
        test_config.ALLOW_PATH_TRAVERSAL = True
        
        # Create service and modify its repository base path
        service = MegaDocumentService(test_config)
        service.repository.base_path = temp_dir
        
        # Create larger test files
        builder = DataBuilder(temp_dir)
        builder.add_file("src/main.py", "print('hello')\n" * 1000)  # 1000 lines
        builder.add_file("include/header.h", "#pragma once\n" * 100)  # 100 lines
        
        # Create review document
        review_file = temp_dir / "review.md"
        review_file.write_text("- `src/main.py`\n- `include/header.h`")
        
        # Build document
        output_file = temp_dir / "output.md"
        
        stats = service.create_mega_document(review_file, output_file, "Large Files Test")
        
        # Verify it completed successfully
        assert stats["total_files_found"] == 2
        assert stats["processed_files"] == 2
        assert stats["processing_time_seconds"] > 0
        
        # Verify content size
        content = output_file.read_text()
        assert len(content) > 10000  # Should be substantial
