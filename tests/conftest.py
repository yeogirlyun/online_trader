#!/usr/bin/env python3
"""
Test configuration and fixtures for mega document creation system.

This module provides shared test fixtures, utilities, and configuration
for comprehensive testing of all components.
"""

import pytest
import tempfile
import shutil
from pathlib import Path
from typing import Dict, Any, List
from unittest.mock import Mock, patch

# Import our modules
import sys
sys.path.append(str(Path(__file__).parent.parent))

from config.mega_doc_config import MegaDocConfig
from exceptions.mega_doc_exceptions import (
    MegaDocumentError, FileValidationError, SecurityError, 
    ConfigurationError, ProcessingError, ExtractionError, BuildError
)
from repositories.file_repository import MockFileRepository
from services.mega_document_service import FileExtractor, MegaDocumentBuilder, MegaDocumentService


@pytest.fixture
def test_config():
    """Create a test configuration with minimal settings."""
    config = MegaDocConfig()
    # Override settings for testing
    config.MAX_FILE_SIZE_MB = 1  # Small limit for testing
    config.MAX_PREVIEW_FILES = 3
    config.MAX_ERROR_PREVIEW = 2
    config.ENABLE_CACHING = False  # Disable caching for predictable tests
    config.ALLOW_PATH_TRAVERSAL = True  # Allow traversal for tests (temporary dirs)
    return config


@pytest.fixture
def temp_dir():
    """Create a temporary directory for test files."""
    temp_path = Path(tempfile.mkdtemp())
    yield temp_path
    shutil.rmtree(temp_path)


@pytest.fixture
def mock_repository(test_config):
    """Create a mock file repository for testing."""
    return MockFileRepository(test_config)


@pytest.fixture
def sample_files(temp_dir):
    """Create sample files for testing."""
    files = {}
    
    # Create test directory structure
    src_dir = temp_dir / "src"
    include_dir = temp_dir / "include"
    config_dir = temp_dir / "config"
    
    src_dir.mkdir()
    include_dir.mkdir()
    config_dir.mkdir()
    
    # Create test files
    test_files = {
        "src/main.py": "print('Hello, World!')\n",
        "src/utils.py": "def helper():\n    return True\n",
        "include/header.h": "#pragma once\nint main();\n",
        "config/settings.json": '{"debug": true, "version": "1.0"}\n',
        "README.md": "# Test Project\nThis is a test.\n",
        "invalid.txt": "This file has no extension\n"
    }
    
    for file_path, content in test_files.items():
        full_path = temp_dir / file_path
        full_path.write_text(content)
        files[file_path] = content
    
    return files, temp_dir


@pytest.fixture
def sample_review_content():
    """Sample review document content for testing."""
    return """
# Test Review Document

This document contains references to various source files.

## Source Files

- `src/main.py`
- `src/utils.py`
- `include/header.h`
- `config/settings.json`

## Configuration

See [settings.json](config/settings.json) for configuration details.

## Code Examples

```python
# From src/main.py
print('Hello, World!')
```

## Invalid References

- `nonexistent/file.py` (should be skipped)
- `http://example.com/file.py` (URL, should be skipped)
- `file with spaces.py` (spaces, should be skipped)
- `justfilename.py` (no path, should be skipped)
"""


@pytest.fixture
def file_extractor(test_config):
    """Create a file extractor for testing."""
    return FileExtractor(test_config)


@pytest.fixture
def mega_document_service(test_config):
    """Create a mega document service for testing."""
    return MegaDocumentService(test_config)


class TestDataBuilder:
    """Builder class for creating test data."""
    
    def __init__(self, temp_dir: Path):
        self.temp_dir = temp_dir
        self.files = {}
    
    def add_file(self, path: str, content: str) -> 'TestDataBuilder':
        """Add a file to the test data."""
        full_path = self.temp_dir / path
        full_path.parent.mkdir(parents=True, exist_ok=True)
        full_path.write_text(content)
        self.files[path] = content
        return self
    
    def add_large_file(self, path: str, size_mb: float = 2.0) -> 'TestDataBuilder':
        """Add a large file for size limit testing."""
        content = "x" * int(size_mb * 1024 * 1024)
        return self.add_file(path, content)
    
    def add_binary_file(self, path: str) -> 'TestDataBuilder':
        """Add a binary file for encoding testing."""
        full_path = self.temp_dir / path
        full_path.parent.mkdir(parents=True, exist_ok=True)
        full_path.write_bytes(b'\x00\x01\x02\x03\xff\xfe\xfd')
        return self
    
    def add_symlink(self, link_path: str, target_path: str) -> 'TestDataBuilder':
        """Add a symbolic link."""
        link_full = self.temp_dir / link_path
        target_full = self.temp_dir / target_path
        link_full.parent.mkdir(parents=True, exist_ok=True)
        link_full.symlink_to(target_full)
        return self
    
    def build(self) -> Dict[str, str]:
        """Build and return the test data."""
        return self.files.copy()


def assert_file_exists(path: Path, should_exist: bool = True):
    """Assert file existence."""
    if should_exist:
        assert path.exists(), f"Expected file to exist: {path}"
        assert path.is_file(), f"Expected file to be a file: {path}"
    else:
        assert not path.exists(), f"Expected file to not exist: {path}"


def assert_file_content(path: Path, expected_content: str):
    """Assert file content matches expected."""
    actual_content = path.read_text()
    assert actual_content == expected_content, f"File content mismatch in {path}"


def assert_mega_document_structure(output_file: Path, expected_files: List[str]):
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
    
    # Check file sections
    for i, file_path in enumerate(expected_files, 1):
        section_header = f"## 📄 **FILE {i} of {len(expected_files)}**: {file_path}"
        assert section_header in content, f"Missing file section: {file_path}"


def create_test_review_document(temp_dir: Path, file_paths: List[str]) -> Path:
    """Create a test review document with file references."""
    content = "# Test Review\n\n"
    content += "## Source Files\n\n"
    
    for file_path in file_paths:
        content += f"- `{file_path}`\n"
    
    review_file = temp_dir / "review.md"
    review_file.write_text(content)
    return review_file
