#!/usr/bin/env python3
"""
Tests for the exception hierarchy.

This module tests all custom exceptions and error handling utilities,
including error context management and decorators.
"""

import pytest
import logging
from unittest.mock import Mock, patch

from exceptions.mega_doc_exceptions import (
    MegaDocumentError, FileValidationError, SecurityError, 
    ConfigurationError, ProcessingError, ExtractionError, 
    BuildError, CacheError, PerformanceError, handle_errors, ErrorContext
)


class TestMegaDocumentError:
    """Test base exception class."""
    
    def test_basic_initialization(self):
        """Test basic exception initialization."""
        error = MegaDocumentError("Test error")
        
        assert str(error) == "Test error"
        assert error.error_code is None
        assert error.details == {}
    
    def test_with_error_code(self):
        """Test exception with error code."""
        error = MegaDocumentError("Test error", "TEST_CODE")
        
        assert str(error) == "Test error"
        assert error.error_code == "TEST_CODE"
    
    def test_with_details(self):
        """Test exception with details."""
        details = {"file": "test.py", "line": 42}
        error = MegaDocumentError("Test error", "TEST_CODE", details)
        
        assert error.details == details
    
    def test_to_dict(self):
        """Test exception serialization."""
        details = {"file": "test.py"}
        error = MegaDocumentError("Test error", "TEST_CODE", details)
        
        error_dict = error.to_dict()
        
        assert error_dict["type"] == "MegaDocumentError"
        assert error_dict["message"] == "Test error"
        assert error_dict["error_code"] == "TEST_CODE"
        assert error_dict["details"] == details


class TestFileValidationError:
    """Test file validation exception."""
    
    def test_initialization(self):
        """Test file validation error initialization."""
        error = FileValidationError("test.py", "File not found")
        
        assert str(error) == "Validation failed for test.py: File not found"
        assert error.file_path == "test.py"
        assert error.reason == "File not found"
        assert error.error_code == "FILE_VALIDATION"
    
    def test_details_inheritance(self):
        """Test that details are properly set."""
        error = FileValidationError("test.py", "File not found")
        
        assert error.details["file_path"] == "test.py"
        assert error.details["reason"] == "File not found"


class TestSecurityError:
    """Test security exception."""
    
    def test_initialization(self):
        """Test security error initialization."""
        error = SecurityError("Path traversal detected")
        
        assert str(error) == "Path traversal detected"
        assert error.error_code == "SECURITY"
        assert error.threat_type is None
    
    def test_with_threat_type(self):
        """Test security error with threat type."""
        error = SecurityError("Path traversal detected", "SECURITY", "path_traversal")
        
        assert error.threat_type == "path_traversal"
        assert error.details["threat_type"] == "path_traversal"


class TestProcessingError:
    """Test processing exception."""
    
    def test_initialization(self):
        """Test processing error initialization."""
        original_error = ValueError("Invalid value")
        error = ProcessingError("test.py", "read", original_error)
        
        assert "Failed to read test.py" in str(error)
        assert error.file_path == "test.py"
        assert error.operation == "read"
        assert error.original_error is original_error
        assert error.error_code == "PROCESSING"
    
    def test_details_inheritance(self):
        """Test that details are properly set."""
        original_error = ValueError("Invalid value")
        error = ProcessingError("test.py", "read", original_error)
        
        assert error.details["file_path"] == "test.py"
        assert error.details["operation"] == "read"
        assert error.details["original_error_type"] == "ValueError"
        assert error.details["original_error_message"] == "Invalid value"


class TestErrorHandlerDecorator:
    """Test error handling decorator."""
    
    def test_successful_execution(self):
        """Test decorator with successful execution."""
        @handle_errors()
        def test_func():
            return "success"
        
        result = test_func()
        assert result == "success"
    
    def test_custom_exception_reraising(self):
        """Test that custom exceptions are re-raised."""
        @handle_errors()
        def test_func():
            raise FileValidationError("test.py", "Not found")
        
        with pytest.raises(FileValidationError):
            test_func()
    
    def test_unexpected_exception_handling(self):
        """Test handling of unexpected exceptions."""
        @handle_errors(default_return="error")
        def test_func():
            raise ValueError("Unexpected error")
        
        result = test_func()
        assert result == "error"
    
    def test_logging_disabled(self):
        """Test decorator with logging disabled."""
        @handle_errors(default_return="error", log_errors=False)
        def test_func():
            raise ValueError("Unexpected error")
        
        # Should not log anything
        with patch('logging.getLogger') as mock_logger:
            result = test_func()
            assert result == "error"
            # Logger should not be called
            mock_logger.assert_not_called()
    
    def test_custom_exception_not_reraising(self):
        """Test custom exceptions not re-raised when configured."""
        @handle_errors(default_return="error", reraise_custom=False)
        def test_func():
            raise FileValidationError("test.py", "Not found")
        
        result = test_func()
        assert result == "error"


class TestErrorContext:
    """Test error context manager."""
    
    def test_successful_execution(self):
        """Test context manager with successful execution."""
        with ErrorContext("test_operation") as context:
            assert not context.has_errors()
            assert context.get_errors() == []
    
    def test_custom_exception_handling(self):
        """Test context manager with custom exception."""
        with ErrorContext("test_operation") as context:
            context.add_error(FileValidationError("test.py", "Not found"))
            
            assert context.has_errors()
            errors = context.get_errors()
            assert len(errors) == 1
            assert isinstance(errors[0], FileValidationError)
    
    def test_exception_suppression(self):
        """Test that custom exceptions are suppressed."""
        with ErrorContext("test_operation") as context:
            # Raise exception directly in the context
            raise FileValidationError("test.py", "Not found")
        
        # Should have been suppressed and added to context
        assert context.has_errors()
        assert len(context.get_errors()) == 1
    
    def test_unexpected_exception_reraising(self):
        """Test that unexpected exceptions are re-raised."""
        with pytest.raises(ValueError):
            with ErrorContext("test_operation") as context:
                raise ValueError("Unexpected error")
    
    def test_multiple_errors(self):
        """Test context manager with multiple errors."""
        with ErrorContext("test_operation") as context:
            context.add_error(FileValidationError("test1.py", "Not found"))
            context.add_error(SecurityError("Path traversal"))
            
            assert context.has_errors()
            errors = context.get_errors()
            assert len(errors) == 2
            assert isinstance(errors[0], FileValidationError)
            assert isinstance(errors[1], SecurityError)
    
    def test_logging_integration(self):
        """Test context manager logging integration."""
        with patch('logging.getLogger') as mock_logger:
            mock_logger_instance = Mock()
            mock_logger.return_value = mock_logger_instance
            
            with ErrorContext("test_operation") as context:
                context.add_error(FileValidationError("test.py", "Not found"))
            
            # Should have logged the error
            mock_logger_instance.error.assert_called_once()


class TestSpecificExceptions:
    """Test specific exception types."""
    
    def test_extraction_error(self):
        """Test extraction error."""
        error = ExtractionError("content", "No patterns found")
        
        assert "Failed to extract file paths from content" in str(error)
        assert error.content_source == "content"
        assert error.reason == "No patterns found"
        assert error.error_code == "EXTRACTION"
    
    def test_build_error(self):
        """Test build error."""
        error = BuildError("output.md", "Write failed")
        
        assert "Failed to build document output.md" in str(error)
        assert error.output_file == "output.md"
        assert error.reason == "Write failed"
        assert error.error_code == "BUILD"
    
    def test_cache_error(self):
        """Test cache error."""
        error = CacheError("key123", "get", "Cache miss")
        
        assert "Cache get failed for key key123" in str(error)
        assert error.cache_key == "key123"
        assert error.operation == "get"
        assert error.reason == "Cache miss"
        assert error.error_code == "CACHE"
    
    def test_performance_error(self):
        """Test performance error."""
        error = PerformanceError("build", "memory", "500MB", "100MB")
        
        assert "Performance threshold exceeded for build" in str(error)
        assert error.operation == "build"
        assert error.metric == "memory"
        assert error.value == "500MB"
        assert error.threshold == "100MB"
        assert error.error_code == "PERFORMANCE"
    
    def test_configuration_error(self):
        """Test configuration error."""
        error = ConfigurationError("Invalid setting", "CONFIG", "max_size")
        
        assert str(error) == "Invalid setting"
        assert error.error_code == "CONFIG"
        assert error.config_key == "max_size"
        assert error.details["config_key"] == "max_size"


class TestExceptionInheritance:
    """Test exception inheritance and polymorphism."""
    
    def test_exception_hierarchy(self):
        """Test that all exceptions inherit from MegaDocumentError."""
        exceptions = [
            FileValidationError("test.py", "error"),
            SecurityError("error"),
            ConfigurationError("error"),
            ProcessingError("test.py", "op", ValueError("test")),
            ExtractionError("source", "reason"),
            BuildError("output.md", "reason"),
            CacheError("key", "op", "reason"),
            PerformanceError("op", "metric", "value", "threshold")
        ]
        
        for exc in exceptions:
            assert isinstance(exc, MegaDocumentError)
            assert hasattr(exc, 'to_dict')
            assert hasattr(exc, 'error_code')
            assert hasattr(exc, 'details')
    
    def test_exception_serialization(self):
        """Test that all exceptions can be serialized."""
        exceptions = [
            FileValidationError("test.py", "error"),
            SecurityError("error"),
            ConfigurationError("error"),
            ProcessingError("test.py", "op", ValueError("test")),
            ExtractionError("source", "reason"),
            BuildError("output.md", "reason"),
            CacheError("key", "op", "reason"),
            PerformanceError("op", "metric", "value", "threshold")
        ]
        
        for exc in exceptions:
            error_dict = exc.to_dict()
            assert isinstance(error_dict, dict)
            assert "type" in error_dict
            assert "message" in error_dict
            assert "error_code" in error_dict
            assert "details" in error_dict
