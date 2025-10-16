#!/usr/bin/env python3
"""
Service layer for mega document creation.

This module provides business logic services for file extraction and document building,
separated from infrastructure concerns for better testability and maintainability.
"""

from typing import List, Set, Tuple, Dict, Any, Optional
import logging
from pathlib import Path
from datetime import datetime

from config.mega_doc_config import MegaDocConfig
from repositories.file_repository import FileRepository, SecureFileRepository
from exceptions.mega_doc_exceptions import (
    FileValidationError, ExtractionError, BuildError, ProcessingError
)


class FileExtractor:
    """Service for extracting file paths from review documents."""
    
    def __init__(self, config: MegaDocConfig):
        self.config = config
        self.logger = logging.getLogger(__name__)
    
    def extract_file_paths(self, content: str) -> List[str]:
        """Extract unique file paths from content using compiled patterns."""
        potential_paths: Set[str] = set()
        
        try:
            # Step 1: Extract potential file references with broader patterns
            for pattern in self.config.file_patterns:
                matches = pattern.findall(content)
                for match in matches:
                    # Handle tuple matches (from markdown links)
                    if isinstance(match, tuple):
                        file_path = match[1] if len(match) > 1 else match[0]
                    else:
                        file_path = match
                    
                    potential_paths.add(file_path.strip())
            
            # Step 2: Search directories to find exact file matches
            validated_paths = self._search_and_validate_paths(list(potential_paths))
            
            return self._normalize_and_validate_paths(validated_paths)
            
        except Exception as e:
            raise ExtractionError("content", f"Pattern matching failed: {e}") from e
    
    def _search_and_validate_paths(self, potential_paths: List[str]) -> List[str]:
        """Search directories to find exact file matches for potential paths."""
        validated_paths = []
        
        for path in potential_paths:
            try:
                # Clean up the path
                clean_path = self._clean_path_reference(path)
                if not clean_path:
                    continue
                
                # Try to find the exact file
                exact_path = self._find_exact_file(clean_path)
                if exact_path:
                    validated_paths.append(exact_path)
                    self.logger.debug(f"Found exact match: {path} -> {exact_path}")
                else:
                    self.logger.debug(f"No exact match found for: {path}")
                    
            except Exception as e:
                self.logger.debug(f"Error processing path {path}: {e}")
                continue
        
        return validated_paths
    
    def _clean_path_reference(self, path: str) -> Optional[str]:
        """Clean up a path reference to extract the core filename."""
        if not path or not isinstance(path, str):
            return None
        
        path = path.strip()
        
        # Skip URLs and web links
        if path.startswith(('http://', 'https://', 'www.', 'ftp://')):
            return None
        
        # Skip paths with spaces (likely not real file paths)
        if ' ' in path:
            return None
        
        # Extract filename from various formats
        # Remove common prefixes and suffixes
        path = path.replace('`', '').replace('"', '').replace("'", '')
        
        # Handle different path formats
        if path.startswith('./'):
            path = path[2:]  # Remove ./
        elif path.startswith('/'):
            path = path[1:]  # Remove leading /
        
        # Skip if it doesn't look like a file
        if '.' not in path or len(path) < 3:
            return None
        
        # Return the cleaned path (keep directory structure)
        return path
    
    def _find_exact_file(self, filename: str) -> Optional[str]:
        """Search directories to find the exact file matching the filename."""
        try:
            # Get the base path (project root)
            base_path = Path.cwd()
            
            # First try direct path match
            direct_path = base_path / filename
            if direct_path.exists() and direct_path.is_file():
                return filename
            
            # Search in common directories
            search_dirs = [
                base_path,
                base_path / 'src',
                base_path / 'include', 
                base_path / 'scripts',
                base_path / 'config',
                base_path / 'tools',
                base_path / 'tests',
                base_path / 'data',
                base_path / 'logs'
            ]
            
            # Search for exact filename matches
            for search_dir in search_dirs:
                if not search_dir.exists():
                    continue
                    
                # Search recursively
                for file_path in search_dir.rglob(filename):
                    if file_path.is_file():
                        # Return relative path from project root
                        try:
                            relative_path = file_path.relative_to(base_path)
                            return str(relative_path)
                        except ValueError:
                            # File is outside project root, skip
                            continue
            
            return None
            
        except Exception as e:
            self.logger.debug(f"Error searching for file {filename}: {e}")
            return None
    
    def _normalize_and_validate_paths(self, paths: List[str]) -> List[str]:
        """Normalize and validate extracted paths."""
        normalized = []
        seen = set()  # Track seen paths to avoid duplicates
        
        for path in paths:
            try:
                normalized_path = self._normalize_path(path)
                if normalized_path and self._is_valid_path(normalized_path) and normalized_path not in seen:
                    normalized.append(normalized_path)
                    seen.add(normalized_path)
                else:
                    self.logger.debug(f"Skipped invalid path: {path}")
            except Exception as e:
                self.logger.warning(f"Error processing path {path}: {e}")
        
        return sorted(normalized)  # Consistent ordering
    
    def _normalize_path(self, path: str) -> Optional[str]:
        """Normalize file path."""
        if not path or not isinstance(path, str):
            return None
        
        path = path.strip()
        
        # Skip URLs and web links
        if path.startswith(('http://', 'https://', 'www.', 'ftp://')):
            return None
        
        # Skip paths with spaces (likely not real file paths)
        if ' ' in path:
            return None
        
        # Allow filenames without path separators (they might be in the same directory)
        # if '/' not in path and '\\' not in path:
        #     return None
        
        return path
    
    def _is_valid_path(self, path: str) -> bool:
        """Validate path format and extension."""
        try:
            path_obj = Path(path)
            
            # Check for path traversal
            if '..' in path_obj.parts:
                return False
            
            # Check extension
            if not self.config.validate_file_extension(path):
                return False
            
            return True
            
        except Exception:
            return False


class MegaDocumentBuilder:
    """Service for building mega documents with comprehensive statistics."""
    
    def __init__(
        self, 
        extractor: FileExtractor,
        repository: FileRepository,
        config: MegaDocConfig
    ):
        self.extractor = extractor
        self.repository = repository
        self.config = config
        self.logger = logging.getLogger(__name__)
    
    def build(self, review_file: Path, output_file: Path, title: str = "") -> Dict[str, Any]:
        """Build mega document and return comprehensive statistics."""
        stats = {
            'start_time': datetime.now(),
            'review_file': str(review_file),
            'output_file': str(output_file),
            'title': title,
            'total_files_found': 0,
            'valid_files': 0,
            'processed_files': 0,
            'failed_files': [],
            'total_size_bytes': 0,
            'processing_time_seconds': 0,
            'errors': []
        }
        
        try:
            # Extract file paths
            self.logger.info(f"Processing review file: {review_file}")
            review_content = self.repository.read_file(review_file)
            file_paths = self.extractor.extract_file_paths(review_content)
            stats['total_files_found'] = len(file_paths)
            stats['all_files_found'] = file_paths.copy()  # Store all files found
            
            if not file_paths:
                raise ExtractionError(str(review_file), "No source modules found")
            
            # Process files and build document
            self._build_document(file_paths, output_file, title, stats)
            
        except Exception as e:
            stats['errors'].append(str(e))
            self.logger.error(f"Failed to build mega document: {e}")
            raise
        
        finally:
            stats['processing_time_seconds'] = (
                datetime.now() - stats['start_time']
            ).total_seconds()
        
        return stats
    
    def _build_document(
        self, 
        file_paths: List[str], 
        output_file: Path, 
        title: str, 
        stats: Dict[str, Any]
    ):
        """Build the actual mega document."""
        try:
            with open(output_file, 'w', encoding=self.config.DEFAULT_ENCODING) as f:
                # Write header
                self._write_header(f, title, stats)
                
                # Write table of contents
                self._write_table_of_contents(f, file_paths)
                
                # Process files
                for i, file_path in enumerate(file_paths, 1):
                    try:
                        self._process_single_file(f, file_path, i, len(file_paths), stats)
                    except Exception as e:
                        error_msg = f"Failed to process {file_path}: {e}"
                        stats['failed_files'].append((file_path, str(e)))
                        self.logger.warning(error_msg)
                        f.write(f"\n## Error processing {file_path}\n{error_msg}\n\n")
        except Exception as e:
            raise BuildError(str(output_file), f"Failed to write document: {e}") from e
    
    def _write_header(self, f, title: str, stats: Dict[str, Any]):
        """Write document header."""
        f.write(f"# {title}\n\n")
        f.write(f"**Generated**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"**Working Directory**: {self.repository.base_path}\n")
        f.write(f"**Source**: {stats['review_file']}\n")
        f.write(f"**Total Files**: {stats['total_files_found']}\n\n")
        f.write("---\n\n")
    
    def _write_table_of_contents(self, f, file_paths: List[str]):
        """Write table of contents."""
        f.write("## 📋 **TABLE OF CONTENTS**\n\n")
        for i, file_path in enumerate(file_paths, 1):
            f.write(f"{i}. [{file_path}](#file-{i})\n")
        f.write("\n---\n\n")
    
    def _process_single_file(
        self, 
        f, 
        file_path: str, 
        file_number: int, 
        total_files: int, 
        stats: Dict[str, Any]
    ):
        """Process a single file and add to document."""
        try:
            path_obj = Path(file_path)
            
            # Validate file exists
            if not self.repository.file_exists(path_obj):
                raise FileValidationError(file_path, "File does not exist")
            
            # Get file info
            file_info = self.repository.get_file_info(path_obj)
            stats['total_size_bytes'] += file_info['size']
            
            # Read file content
            content = self.repository.read_file(path_obj)
            
            # Write file section
            f.write(f"## 📄 **FILE {file_number} of {total_files}**: {file_path}\n\n")
            f.write("**File Information**:\n")
            f.write(f"- **Path**: `{file_path}`\n")
            f.write(f"- **Size**: {len(content.splitlines())} lines\n")
            f.write(f"- **Modified**: {datetime.fromtimestamp(file_info['modified']).strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"- **Type**: {file_info['extension']}\n")
            f.write(f"- **Permissions**: {file_info['permissions']}\n\n")
            f.write("```text\n")
            f.write(content)
            f.write("\n```\n\n")
            
            stats['processed_files'] += 1
            self.logger.debug(f"Processed file {file_number}/{total_files}: {file_path}")
            
        except Exception as e:
            raise ProcessingError(file_path, "process", e)


class MegaDocumentService:
    """High-level service that orchestrates the entire mega document creation process."""
    
    def __init__(self, config: Optional[MegaDocConfig] = None):
        self.config = config or MegaDocConfig()
        self.logger = logging.getLogger(__name__)
        
        # Initialize components
        self.repository = SecureFileRepository(self.config)
        self.extractor = FileExtractor(self.config)
        self.builder = MegaDocumentBuilder(self.extractor, self.repository, self.config)
    
    def create_mega_document(
        self, 
        review_file: Path, 
        output_file: Path, 
        title: str = ""
    ) -> Dict[str, Any]:
        """Create a mega document from a review file."""
        self.logger.info(f"Starting mega document creation: {review_file} -> {output_file}")
        
        try:
            # Validate inputs
            if not self.repository.file_exists(review_file):
                raise FileValidationError(str(review_file), "Review file does not exist")
            
            # Ensure output directory exists
            output_file.parent.mkdir(parents=True, exist_ok=True)
            
            # Build the document
            stats = self.builder.build(review_file, output_file, title)
            
            self.logger.info(f"Mega document created successfully: {output_file}")
            return stats
            
        except Exception as e:
            self.logger.error(f"Failed to create mega document: {e}")
            raise
    
    def get_system_info(self) -> Dict[str, Any]:
        """Get system information and statistics."""
        return {
            'config': self.config.to_dict(),
            'cache_stats': self.repository.get_cache_stats(),
            'repository_type': type(self.repository).__name__,
            'base_path': str(self.repository.base_path)
        }
