#!/usr/bin/env python3
"""
Exception hierarchy for mega document operations.

This module provides a comprehensive exception hierarchy for consistent error
handling throughout the mega document creation system.
"""

from typing import Optional, Dict, Any, List
import logging


class MegaDocumentError(Exception):
    """Base exception for all mega document operations."""
    
    def __init__(self, message: str, error_code: Optional[str] = None, details: Optional[Dict[str, Any]] = None):
        self.error_code = error_code
        self.details = details or {}
        super().__init__(message)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert exception to dictionary for logging/serialization."""
        return {
            'type': self.__class__.__name__,
            'message': str(self),
            'error_code': self.error_code,
            'details': self.details
        }


class FileValidationError(MegaDocumentError):
    """File validation failures."""
    
    def __init__(self, file_path: str, reason: str, error_code: str = "FILE_VALIDATION"):
        self.file_path = file_path
        self.reason = reason
        super().__init__(
            f"Validation failed for {file_path}: {reason}", 
            error_code,
            {'file_path': file_path, 'reason': reason}
        )


class SecurityError(MegaDocumentError):
    """Security-related errors."""
    
    def __init__(self, message: str, error_code: str = "SECURITY", threat_type: Optional[str] = None):
        self.threat_type = threat_type
        super().__init__(message, error_code, {'threat_type': threat_type})


class ConfigurationError(MegaDocumentError):
    """Configuration-related errors."""
    
    def __init__(self, message: str, error_code: str = "CONFIG", config_key: Optional[str] = None):
        self.config_key = config_key
        super().__init__(message, error_code, {'config_key': config_key})


class ProcessingError(MegaDocumentError):
    """File processing errors."""
    
    def __init__(self, file_path: str, operation: str, original_error: Exception):
        self.file_path = file_path
        self.operation = operation
        self.original_error = original_error
        super().__init__(
            f"Failed to {operation} {file_path}: {str(original_error)}",
            "PROCESSING",
            {
                'file_path': file_path,
                'operation': operation,
                'original_error_type': type(original_error).__name__,
                'original_error_message': str(original_error)
            }
        )


class ExtractionError(MegaDocumentError):
    """File path extraction errors."""
    
    def __init__(self, content_source: str, reason: str, error_code: str = "EXTRACTION"):
        self.content_source = content_source
        self.reason = reason
        super().__init__(
            f"Failed to extract file paths from {content_source}: {reason}",
            error_code,
            {'content_source': content_source, 'reason': reason}
        )


class BuildError(MegaDocumentError):
    """Document building errors."""
    
    def __init__(self, output_file: str, reason: str, error_code: str = "BUILD"):
        self.output_file = output_file
        self.reason = reason
        super().__init__(
            f"Failed to build document {output_file}: {reason}",
            error_code,
            {'output_file': output_file, 'reason': reason}
        )


class CacheError(MegaDocumentError):
    """Caching-related errors."""
    
    def __init__(self, cache_key: str, operation: str, reason: str, error_code: str = "CACHE"):
        self.cache_key = cache_key
        self.operation = operation
        self.reason = reason
        super().__init__(
            f"Cache {operation} failed for key {cache_key}: {reason}",
            error_code,
            {'cache_key': cache_key, 'operation': operation, 'reason': reason}
        )


class PerformanceError(MegaDocumentError):
    """Performance-related errors."""
    
    def __init__(self, operation: str, metric: str, value: Any, threshold: Any, error_code: str = "PERFORMANCE"):
        self.operation = operation
        self.metric = metric
        self.value = value
        self.threshold = threshold
        super().__init__(
            f"Performance threshold exceeded for {operation}: {metric} = {value} (threshold: {threshold})",
            error_code,
            {
                'operation': operation,
                'metric': metric,
                'value': value,
                'threshold': threshold
            }
        )


# Error handler decorator
def handle_errors(default_return=None, log_errors: bool = True, reraise_custom: bool = True):
    """Decorator for consistent error handling."""
    def decorator(func):
        def wrapper(*args, **kwargs):
            try:
                return func(*args, **kwargs)
            except MegaDocumentError:
                if reraise_custom:
                    raise  # Re-raise our custom exceptions
                return default_return
            except Exception as e:
                if log_errors:
                    logger = logging.getLogger(func.__module__)
                    logger.error(f"Unexpected error in {func.__name__}: {e}", exc_info=True)
                return default_return
        return wrapper
    return decorator


# Context manager for error handling
class ErrorContext:
    """Context manager for handling errors with logging and recovery."""
    
    def __init__(self, operation: str, logger: Optional[logging.Logger] = None):
        self.operation = operation
        self.logger = logger or logging.getLogger(__name__)
        self.errors: List[MegaDocumentError] = []
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        if exc_type is not None:
            if issubclass(exc_type, MegaDocumentError):
                self.errors.append(exc_val)
                self.logger.error(f"Error in {self.operation}: {exc_val}")
                return True  # Suppress the exception
            else:
                self.logger.error(f"Unexpected error in {self.operation}: {exc_val}", exc_info=True)
                return False  # Re-raise the exception
        
        return False
    
    def add_error(self, error: MegaDocumentError):
        """Add an error to the context."""
        self.errors.append(error)
        self.logger.error(f"Error in {self.operation}: {error}")
    
    def has_errors(self) -> bool:
        """Check if any errors occurred."""
        return len(self.errors) > 0
    
    def get_errors(self) -> List[MegaDocumentError]:
        """Get all errors that occurred."""
        return self.errors.copy()
