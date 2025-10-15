#!/usr/bin/env python3
"""
Mega Document Creator

A powerful tool for creating comprehensive mega documents by extracting and consolidating
source code files referenced in review documents, design specifications, or analysis reports.

ARCHITECTURE:
- Configuration module for centralized settings
- Exception hierarchy for consistent error handling  
- Repository pattern for file operations
- Service layer for business logic
- Comprehensive logging and monitoring

WHAT IT DOES:
- Parses review documents to extract file paths using regex patterns
- Validates file existence and security (prevents path traversal attacks)
- Consolidates multiple source files into a single mega document
- Generates structured markdown with table of contents and file information
- Provides detailed statistics and error reporting

SUPPORTED FILE TYPES:
- Source Code: .py, .cpp, .h, .hpp, .c, .js, .ts, .java, .rs, .go
- Configuration: .json, .yaml, .yml, .xml, .sql
- Documentation: .md, .txt
- Scripts: .sh, .bat
"""

import os
import argparse
import logging
import sys
from pathlib import Path
from typing import List

# Import our new modules
sys.path.append(str(Path(__file__).parent.parent))

from config.mega_doc_config import MegaDocConfig, get_config, set_config
from services.mega_document_service import MegaDocumentService
from exceptions.mega_doc_exceptions import (
    MegaDocumentError, FileValidationError, SecurityError, 
    ConfigurationError, ProcessingError, ExtractionError, BuildError
)


def setup_logging(verbose: bool = False) -> logging.Logger:
    """Setup logging configuration."""
    level = logging.DEBUG if verbose else logging.INFO
    
    # Configure root logger
    logger = logging.getLogger()
    logger.setLevel(level)
    
    # Clear existing handlers
    logger.handlers.clear()
    
    # Console handler
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(
        logging.Formatter(
            '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )
    )
    logger.addHandler(console_handler)
    
    return logger


def get_file_size_kb(file_path: Path) -> float:
    """Get file size in KB."""
    return file_path.stat().st_size / 1024


def create_mega_document(review_file: Path, title: str, output: Path, verbose: bool = False) -> List[str]:
    """Create mega document using the new architecture."""
    
    logger = setup_logging(verbose)
    logger.info("🚀 Refactored Mega Document Creator")
    logger.info("=" * 50)
    logger.info(f"📁 Working Directory: {os.getcwd()}")
    logger.info(f"📁 Output: {output}")
    
    try:
        # Initialize configuration
        config = MegaDocConfig()
        set_config(config)
        
        # Initialize service
        service = MegaDocumentService(config)
        
        # Process review document
        logger.info(f"📖 Processing review document: {review_file}")
        
        # Create mega document
        stats = service.create_mega_document(review_file, output, title)
        
        # Log results
        actual_size_kb = get_file_size_kb(output)
        logger.info(f"📊 Document size: {actual_size_kb:.1f} KB ({stats['processed_files']} files)")
        
        logger.info("✅ Mega Document Creation Complete!")
        logger.info("📊 Summary:")
        logger.info(f"   Total files found: {stats['total_files_found']}")
        logger.info(f"   Files processed: {stats['processed_files']}")
        logger.info(f"   Files failed: {len(stats['failed_files'])}")
        logger.info(f"   Total content size: {stats['total_size_bytes'] / 1024:.1f} KB")
        logger.info(f"   Processing time: {stats['processing_time_seconds']:.2f} seconds")
        logger.info(f"📄 Document: {output.absolute()} ({actual_size_kb:.1f} KB)")
        
        # Log all files found
        if stats.get('all_files_found'):
            logger.info(f"📋 All files found ({len(stats['all_files_found'])}):")
            for i, file_path in enumerate(stats['all_files_found'], 1):
                status = "✅" if file_path not in [f[0] for f in stats['failed_files']] else "❌"
                logger.info(f"   {i:2d}. {status} {file_path}")
        
        # Log failed files if any
        if stats['failed_files']:
            logger.warning(f"⚠️  Failed files ({len(stats['failed_files'])}):")
            for file_path, error in stats['failed_files']:
                logger.warning(f"   ❌ {file_path}: {error}")
        
        return [str(output)]
        
    except MegaDocumentError as e:
        logger.error(f"❌ Mega Document Error: {e}")
        if verbose:
            logger.error(f"Error details: {e.to_dict()}")
        return []
    except Exception as e:
        logger.error(f"❌ Unexpected error: {e}")
        if verbose:
            import traceback
            logger.error(traceback.format_exc())
        return []
    

def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Mega Document Creator - Consolidate source files from review documents",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
USAGE SCENARIOS:

📋 CODE REVIEW CONSOLIDATION:
  # Create mega doc from code review with all referenced files
  python create_mega_document.py --review CODE_REVIEW.md -t "Code Review Analysis" -o code_review_mega.md

📊 DESIGN DOCUMENT ANALYSIS:
  # Extract all source modules mentioned in design document
  python create_mega_document.py --review DESIGN_SPEC.md -t "Design Implementation" -o design_mega.md

🔍 BUG REPORT INVESTIGATION:
  # Consolidate all files mentioned in bug report for analysis
  python create_mega_document.py --review BUG_REPORT.md -t "Bug Investigation" -o bug_analysis.md

📈 PERFORMANCE ANALYSIS:
  # Gather all performance-critical files for analysis
  python create_mega_document.py --review PERF_ANALYSIS.md -t "Performance Review" -o perf_mega.md

🛠️ REFACTORING PLANNING:
  # Collect all files to be refactored for planning
  python create_mega_document.py --review REFACTOR_PLAN.md -t "Refactoring Scope" -o refactor_mega.md

📚 DOCUMENTATION GENERATION:
  # Create comprehensive documentation from technical specs
  python create_mega_document.py --review TECH_SPEC.md -t "Technical Documentation" -o tech_docs.md

🔧 DEBUGGING AND TROUBLESHOOTING:
  # Verbose output for debugging issues
  python create_mega_document.py --review ISSUE_REPORT.md -t "Issue Analysis" -o issue_mega.md --verbose

📁 BATCH PROCESSING:
  # Process multiple documents (use in scripts)
  for doc in reviews/*.md; do
    python create_mega_document.py --review "$doc" -t "Review: $(basename "$doc")" -o "megadocs/$(basename "$doc" .md)_mega.md"
  done

REVIEW DOCUMENT FORMAT:
  Your review document should contain file references in these formats:
  
  ## Source Files
  - `src/main.py`                    # List item with backticks
  - `include/header.h`               # Another file reference
  - `config/settings.json`           # Configuration files
  
  ## Implementation Details
  The main logic is in `core/engine.cpp` and `core/engine.h`.
  
  ```cpp
  // Code blocks can also contain file references
  #include "utils/helper.h"
  ```
  
  [Link to file](docs/README.md)     # Markdown links
  
  ## Analysis
  Files like `tests/unit_test.py` and `tests/integration_test.py` need review.

SECURITY FEATURES:
  ✅ Path traversal protection (prevents ../../../etc/passwd attacks)
  ✅ File size limits (configurable, default 100MB)
  ✅ Extension validation (only processes supported file types)
  ✅ Permission checking (validates file accessibility)
  ✅ Error handling (graceful failure with detailed reporting)

OUTPUT FORMAT:
  The generated mega document includes:
  📄 Document header with metadata
  📋 Table of contents with file links
  📊 File information (size, modification date, type)
  📝 Complete file content for each referenced file
  📈 Processing statistics and error summary

CONFIGURATION:
  The tool uses intelligent defaults but can be customized:
  - Supported file extensions (configurable)
  - Maximum file size limits
  - Path traversal security settings
  - Caching and performance options
  - Logging verbosity levels

For more information, see the source code or run with --verbose for detailed logs.
        """
    )
    
    # Required arguments
    parser.add_argument("--review", "-r", required=True, type=Path,
                       help="""Review document path containing file references to extract.
                       The document should contain file paths in formats like:
                       - `src/main.py` (backtick-wrapped paths)
                       - [Link](docs/README.md) (markdown links)
                       - #include "header.h" (in code blocks)
                       Supports .md, .txt, and other text formats.""")
    
    parser.add_argument("--title", "-t", required=True, 
                       help="""Title for the generated mega document.
                       This will appear as the main heading in the output.
                       Examples: "Code Review Analysis", "Design Implementation", "Bug Investigation" """)
    
    parser.add_argument("--output", "-o", required=True, type=Path, 
                       help="""Output file path for the generated mega document.
                       Should have .md extension for markdown format.
                       Directory will be created if it doesn't exist.
                       Examples: megadocs/review_mega.md, analysis.md, output.md""")
    
    # Optional arguments
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="""Enable verbose logging for debugging and detailed output.
                       Shows file processing details, validation steps, and error traces.
                       Useful for troubleshooting issues or understanding the process.""")
    
    args = parser.parse_args()
    
    # Validate inputs
    if not args.review.exists():
        print(f"❌ Review file not found: {args.review}")
        sys.exit(1)
    
    if not args.review.is_file():
        print(f"❌ Review path is not a file: {args.review}")
        sys.exit(1)
    
    # Create mega document
    try:
        created_files = create_mega_document(
            review_file=args.review,
            title=args.title,
            output=args.output,
            verbose=args.verbose
        )
        
        if not created_files:
            print("❌ No documents created")
            sys.exit(1)
        else:
            print(f"\n🎉 Success! Created {len(created_files)} document(s)")
            
    except KeyboardInterrupt:
        print("\n❌ Operation cancelled by user")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Error creating mega document: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
