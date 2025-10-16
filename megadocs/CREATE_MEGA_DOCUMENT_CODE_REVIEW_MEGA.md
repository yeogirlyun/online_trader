# CREATE_MEGA_DOCUMENT_CODE_REVIEW - Complete Analysis

**Generated**: 2025-10-15 04:00:44
**Working Directory**: /Volumes/ExternalSSD/Dev/C++/online_trader
**Source**: Review document: /Volumes/ExternalSSD/Dev/C++/online_trader/CREATE_MEGA_DOCUMENT_CODE_REVIEW.md (3 valid files)

**Total Files**: 3

---

## 📋 **TABLE OF CONTENTS**

1. [tools/README.md](#file-1)
2. [tools/auto_mega_doc.sh](#file-2)
3. [tools/create_mega_document.py](#file-3)

---

## 📄 **FILE 1 of 3**: tools/README.md

**File Information**:
- **Path**: `tools/README.md`

- **Size**: 82 lines
- **Modified**: 2025-10-15 03:43:04

- **Type**: .md

```text
# Tools Directory

This directory contains various utility scripts and tools for the online trader project.

## Auto Mega Document Creator

**Script:** `auto_mega_doc.sh`

Automatically detects the most recent bug report, review, or analysis document and creates a comprehensive mega document using the `create_mega_document.py` script.

### Features

- 🔍 **Auto-Detection**: Finds the most recent document matching common patterns
- ✅ **Validation**: Checks that documents contain source module references
- 📚 **Comprehensive**: Creates mega documents with all referenced source code
- 🚨 **Error Handling**: Provides helpful messages if no source modules are found

### Usage

```bash
./tools/auto_mega_doc.sh
```

### Document Patterns Searched

The script automatically finds documents matching these patterns:
- `*BUG*.md` - Bug reports and bug analysis
- `*REVIEW*.md` - Code reviews and analysis
- `*ANALYSIS*.md` - Detailed analysis documents
- `*REQUIREMENTS*.md` - Requirements and specifications
- `*DESIGN*.md` - Design documents
- `*ARCHITECTURE*.md` - Architecture documentation
- `*DETAILED*.md` - Detailed technical documents
- `*CRITICAL*.md` - Critical issue documentation
- `*FIX*.md` - Fix implementation documents
- `*REPORT*.md` - Various reports

### Source Module Detection

The script validates that documents contain source module references by looking for:

**File Path Patterns:**
- `include/path/file.h` - Header files
- `src/path/file.cpp` - Source files
- `tools/script.py` - Python scripts
- `scripts/script.sh` - Shell scripts
- `config/file.json` - Configuration files
- `config/file.yaml` - YAML configuration

**Reference Sections:**
- Headers like "Source Modules" or "Reference"
- Sections containing file paths with backticks

### Output

- **Location**: `megadocs/` directory
- **Format**: `{DOCUMENT_NAME}_MEGA.md`
- **Content**: Original document + all referenced source code
- **Features**: Table of contents, file metadata, clickable links

### Error Handling

If no source modules are found, the script will:
1. Display a clear error message
2. Explain what patterns to look for
3. Suggest manual usage of `create_mega_document.py`
4. Exit with appropriate error code

### Examples

```bash
# Run the auto-detection script
./tools/auto_mega_doc.sh

# Manual usage (if auto-detection fails)
python3 tools/create_mega_document.py --review path/to/document.md -t "Document Title" -o megadocs/output.md
```

## Other Tools

- `create_mega_document.py` - Core mega document creation script
- Various analysis and utility scripts

```

## 📄 **FILE 2 of 3**: tools/auto_mega_doc.sh

**File Information**:
- **Path**: `tools/auto_mega_doc.sh`

- **Size**: 220 lines
- **Modified**: 2025-10-15 03:43:04

- **Type**: .sh

```text
#!/bin/bash

# Auto Mega Document Creator
# Automatically detects the most recent bug report/review document and creates a mega document
# 
# Features:
# - Automatically finds the most recent document matching patterns like *BUG*.md, *REVIEW*.md, etc.
# - Validates that the document contains source module references
# - Uses create_mega_document.py to generate comprehensive mega documents
# - Provides helpful error messages if no source modules are found
#
# Usage: ./tools/auto_mega_doc.sh
#
# The script will:
# 1. Search for recent documents in the project root and subdirectories
# 2. Check if the document contains source module references
# 3. Generate a mega document in the megadocs/ folder
# 4. Show a preview of the created document
#
# Document patterns searched:
# - *BUG*.md, *REVIEW*.md, *ANALYSIS*.md, *REQUIREMENTS*.md
# - *DESIGN*.md, *ARCHITECTURE*.md, *DETAILED*.md, *CRITICAL*.md
# - *FIX*.md, *REPORT*.md
#
# Source module patterns detected:
# - include/path/file.h, src/path/file.cpp, tools/script.py
# - config/file.json, config/file.yaml, scripts/script.sh
# - Reference sections with headers like "Source Modules" or "Reference"

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

# Function to check if a document has source module references
check_has_source_modules() {
    local doc_path="$1"
    
    # Patterns that indicate source module references (more specific)
    local patterns=(
        "include/[^[:space:]]*\.h"
        "src/[^[:space:]]*\.cpp"
        "tools/[^[:space:]]*\.py"
        "scripts/[^[:space:]]*\.sh"
        "config/[^[:space:]]*\.json"
        "config/[^[:space:]]*\.yaml"
        "config/[^[:space:]]*\.yml"
    )
    
    local has_references=false
    
    for pattern in "${patterns[@]}"; do
        if grep -qE "$pattern" "$doc_path"; then
            has_references=true
            break
        fi
    done
    
    # Also check for common reference section headers (more specific patterns)
    if grep -qiE "(^#.*source modules|^#.*reference.*modules|^#.*modules.*reference|^##.*source modules|^##.*reference.*modules|^##.*modules.*reference)" "$doc_path"; then
        has_references=true
    fi
    
    # Additional check: look for file paths with backticks or specific formatting
    if grep -qE "\`[^\`]*\.(h|cpp|py|sh|json|yaml|yml)\`" "$doc_path"; then
        has_references=true
    fi
    
    echo "$has_references"
}

# Function to find the most recent document
find_most_recent_doc() {
    local project_root="$1"
    
    # Document patterns to look for
    local patterns=(
        "*BUG*.md"
        "*REVIEW*.md"
        "*ANALYSIS*.md"
        "*REQUIREMENTS*.md"
        "*DESIGN*.md"
        "*ARCHITECTURE*.md"
        "*DETAILED*.md"
        "*CRITICAL*.md"
        "*FIX*.md"
        "*REPORT*.md"
    )
    
    local all_docs=()
    
    # Find all matching documents
    for pattern in "${patterns[@]}"; do
        while IFS= read -r -d '' file; do
            all_docs+=("$file")
        done < <(find "$project_root" -name "$pattern" -type f -print0)
    done
    
    if [ ${#all_docs[@]} -eq 0 ]; then
        return 1
    fi
    
    # Sort by modification time (most recent first)
    local most_recent=""
    local most_recent_time=0
    
    for doc in "${all_docs[@]}"; do
        local mod_time=$(stat -f "%m" "$doc" 2>/dev/null || stat -c "%Y" "$doc" 2>/dev/null)
        if [ "$mod_time" -gt "$most_recent_time" ]; then
            most_recent_time="$mod_time"
            most_recent="$doc"
        fi
    done
    
    echo "$most_recent"
}

# Main function
main() {
    print_info "Auto Mega Document Creator"
    print_info "=========================="
    
    # Get project root (parent of tools directory)
    local project_root="$(cd "$(dirname "$0")/.." && pwd)"
    print_info "Project root: $project_root"
    
    # Find the most recent document
    print_info "Searching for recent bug reports, reviews, and analysis documents..."
    
    local recent_doc
    if ! recent_doc=$(find_most_recent_doc "$project_root"); then
        print_error "No recent bug report, review, or analysis documents found!"
        print_info "Looking for patterns: *BUG*.md, *REVIEW*.md, *ANALYSIS*.md, *REQUIREMENTS*.md, etc."
        exit 1
    fi
    
    print_success "Found most recent document: $recent_doc"
    
    # Check if the document has source module references
    print_info "Checking if document contains source module references..."
    
    local has_sources
    has_sources=$(check_has_source_modules "$recent_doc")
    
    if [ "$has_sources" != "true" ]; then
        print_error "Document does not appear to contain source module references!"
        print_warning "The document should have a reference section listing source modules/paths."
        print_info "Looking for patterns like:"
        print_info "  - include/path/file.h"
        print_info "  - src/path/file.cpp"
        print_info "  - tools/script.py"
        print_info "  - Or a 'Reference' or 'Source Modules' section"
        echo
        print_warning "Please add source module references to the document or choose a different document."
        print_info "You can manually specify a document with:"
        print_info "  python3 tools/create_mega_document.py --review <document_path> -t \"<title>\" -o megadocs/output.md"
        exit 1
    fi
    
    print_success "Document contains source module references!"
    
    # Generate title from filename
    local basename_doc=$(basename "$recent_doc" .md)
    local title="${basename_doc} - Complete Analysis"
    
    # Generate output filename
    local output_file="megadocs/${basename_doc}_MEGA.md"
    
    print_info "Creating mega document..."
    print_info "  Source: $recent_doc"
    print_info "  Title: $title"
    print_info "  Output: $output_file"
    
    # Create megadocs directory if it doesn't exist
    mkdir -p "$project_root/megadocs"
    
    # Run the create_mega_document script
    if python3 "$project_root/tools/create_mega_document.py" --review "$recent_doc" -t "$title" -o "$output_file"; then
        print_success "Mega document created successfully!"
        print_info "Output: $output_file"
        
        # Show file size
        local file_size=$(du -h "$output_file" | cut -f1)
        print_info "File size: $file_size"
        
        # Show first few lines
        print_info "Preview:"
        echo "---"
        head -10 "$output_file"
        echo "---"
        
    else
        print_error "Failed to create mega document!"
        exit 1
    fi
}

# Run main function
main "$@"

```

## 📄 **FILE 3 of 3**: tools/create_mega_document.py

**File Information**:
- **Path**: `tools/create_mega_document.py`

- **Size**: 272 lines
- **Modified**: 2025-10-15 03:43:04

- **Type**: .py

```text
#!/usr/bin/env python3
"""
Enhanced Mega Document Creator

Features:
- Extract source modules from review documents automatically
- Create mega documents with extracted source code

Usage Examples:
  # Extract source modules from review document
  python create_mega_document.py --review DESIGN_DOCUMENT.md -t "Design Review" -o design_mega.md

  # Extract source modules from review document with custom title
  python create_mega_document.py --review ANALYSIS_REPORT.md -t "Analysis Review" -o analysis_mega.md
"""

import os
import argparse
import datetime
import re
import glob
from pathlib import Path
from typing import List, Tuple

def get_file_size_kb(file_path):
    """Get file size in KB."""
    return os.path.getsize(file_path) / 1024


def extract_source_modules_from_review(review_file_path):
    """Extract source module paths from a review document."""
    if not os.path.exists(review_file_path):
        print(f"❌ Review file not found: {review_file_path}")
        return []
    
    print(f"📖 Reading review document: {review_file_path}")
    
    try:
        with open(review_file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"❌ Error reading review file: {e}")
        return []
    
    # Patterns to match source file paths in markdown documents
    patterns = [
        # Pattern 1: `- include/path/file.h` or `- src/path/file.cpp` (with dashes)
        r'-\s+`([^`]+\.(?:h|cpp|hpp|c|py|js|ts|java|rs|go|md|txt|sh|bat|yml|yaml|json|xml|sql))`',
        
        # Pattern 2: `include/path/file.h` or `src/path/file.cpp` (without dashes)
        r'`([^`]*\.(?:h|cpp|hpp|c|py|js|ts|java|rs|go|md|txt|sh|bat|yml|yaml|json|xml|sql))`',
        
        # Pattern 3: Lines starting with file paths (no backticks)
        r'^([^`\s][^`]*\.(?:h|cpp|hpp|c|py|js|ts|java|rs|go|md|txt|sh|bat|yml|yaml|json|xml|sql))',
        
        # Pattern 4: File paths in code blocks
        r'```[^`]*\n([^`]*\.(?:h|cpp|hpp|c|py|js|ts|java|rs|go|md|txt|sh|bat|yml|yaml|json|xml|sql))',
        
        # Pattern 5: Paths in markdown links
        r'\[([^\]]+)\]\(([^)]*\.(?:h|cpp|hpp|c|py|js|ts|java|rs|go|md|txt|sh|bat|yml|yaml|json|xml|sql))\)',
        
        # Pattern 6: Direct file paths without backticks (for lists)
        r'^-\s+([^`\s][^`]*\.(?:h|cpp|hpp|c|py|js|ts|java|rs|go|md|txt|sh|bat|yml|yaml|json|xml|sql))',
        
        # Pattern 7: File paths in parentheses or brackets
        r'[\[\(]([^\]\)]*\.(?:h|cpp|hpp|c|py|js|ts|java|rs|go|md|txt|sh|bat|yml|yaml|json|xml|sql))[\]\)]'
    ]
    
    extracted_files = set()
    
    for pattern in patterns:
        matches = re.findall(pattern, content, re.MULTILINE | re.IGNORECASE)
        for match in matches:
            if isinstance(match, tuple):
                # Handle tuple matches (like from pattern 5)
                file_path = match[1] if len(match) > 1 else match[0]
            else:
                file_path = match
            
            # Clean up the file path
            file_path = file_path.strip()
            
            # Skip if it's just a filename without path
            if '/' not in file_path and '\\' not in file_path:
                continue
                
            # Skip if it contains spaces (likely not a real file path)
            if ' ' in file_path:
                continue
            
            # Skip if it's a URL or web link
            if file_path.startswith(('http://', 'https://', 'www.')):
                continue
            
            extracted_files.add(file_path)
    
    # Convert to list and sort
    file_list = sorted(list(extracted_files))
    
    print(f"📋 Extracted {len(file_list)} source module paths from review document")
    
    if file_list:
        print("📄 Found source modules:")
        for i, file_path in enumerate(file_list[:10], 1):  # Show first 10
            print(f"   {i:2d}. {file_path}")
        if len(file_list) > 10:
            print(f"   ... and {len(file_list) - 10} more")
    
    return file_list


def validate_file_list(file_list: List[str]) -> Tuple[List[str], List[str]]:
    """Validate file list and return (valid_files, invalid_files)."""
    valid_files = []
    invalid_files = []
    
    for file_path in file_list:
        if os.path.exists(file_path) and os.path.isfile(file_path):
            valid_files.append(file_path)
        else:
            invalid_files.append(file_path)
    
    return valid_files, invalid_files


def create_mega_document(review_file, title="", output=""):
    """Create mega document from review document."""
    
    print(f"🚀 Enhanced Mega Document Creator")
    print(f"================================")
    print(f"📁 Working Directory: {os.getcwd()}")
    print(f"📁 Output: {output}")
    
    # Process review document
    print(f"📖 Processing review document: {review_file}")
    
    # Extract source modules from review
    extracted_files = extract_source_modules_from_review(review_file)
    
    if not extracted_files:
        print("❌ No source modules found in review document")
        return []
    
    # Validate extracted files
    valid_files, invalid_files = validate_file_list(extracted_files)
    
    if invalid_files:
        print(f"⚠️  Invalid files (will be skipped): {len(invalid_files)}")
        for invalid_file in invalid_files[:5]:  # Show first 5
            print(f"   ❌ {invalid_file}")
        if len(invalid_files) > 5:
            print(f"   ... and {len(invalid_files) - 5} more")
    
    all_files = valid_files
    source_info = f"Review document: {review_file} ({len(valid_files)} valid files)"
    
    print(f"📁 Initial file count: {len(all_files)}")
    
    if not all_files:
        print("❌ No valid files found to process")
        return []
    
    # Sort files for consistent ordering
    all_files.sort()
    
    # Calculate total estimated size
    total_size = sum(os.path.getsize(f) for f in all_files if os.path.exists(f))
    
    print(f"\n📊 Processing Statistics:")
    print(f"   Files to process: {len(all_files)}")
    print(f"   Total size: {total_size / 1024:.1f} KB")
    
    # Create single document
    print(f"\n📝 Creating mega document...")
    
    with open(output, 'w', encoding='utf-8') as f:
        # Write header
        f.write(f"# {title}\n\n")
        f.write(f"**Generated**: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"**Working Directory**: {os.getcwd()}\n")
        f.write(f"**Source**: {source_info}\n\n")
        f.write(f"**Total Files**: {len(all_files)}\n\n")
        f.write("---\n\n")
        
        # Table of contents
        f.write("## 📋 **TABLE OF CONTENTS**\n\n")
        for i, file_path in enumerate(all_files, 1):
            f.write(f"{i}. [{file_path}](#file-{i})\n")
        f.write("\n---\n\n")
        
        # File contents
        files_processed = 0
        for i, file_path in enumerate(all_files, 1):
            try:
                with open(file_path, 'r', encoding='utf-8') as file_f:
                    content = file_f.read()
                
                f.write(f"## 📄 **FILE {i} of {len(all_files)}**: {file_path}\n\n")
                f.write("**File Information**:\n")
                f.write(f"- **Path**: `{file_path}`\n\n")
                f.write(f"- **Size**: {len(content.splitlines())} lines\n")
                f.write(f"- **Modified**: {datetime.datetime.fromtimestamp(os.path.getmtime(file_path)).strftime('%Y-%m-%d %H:%M:%S')}\n\n")
                f.write(f"- **Type**: {Path(file_path).suffix}\n\n")
                f.write("```text\n")
                f.write(content)
                f.write("\n```\n\n")
                
                print(f"📄 Processing file {i}/{len(all_files)}: {file_path}")
                files_processed += 1
                
            except Exception as e:
                print(f"❌ Error processing {file_path}: {e}")
                f.write(f"## 📄 **FILE {i} of {len(all_files)}**: {file_path}\n\n")
                f.write(f"**Error**: Could not read file - {e}\n\n")
    
    actual_size_kb = get_file_size_kb(output)
    print(f"📊 Document size: {actual_size_kb:.1f} KB ({files_processed} files)")
    
    print(f"\n✅ Mega Document Creation Complete!")
    print(f"📊 Summary:")
    print(f"   Files processed: {files_processed}")
    print(f"   Source: {source_info}")
    print(f"📄 Document: {os.path.abspath(output)} ({actual_size_kb:.1f} KB)")
    
    return [output]

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Enhanced Mega Document Creator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Extract source modules from review document
  python create_mega_document.py --review DESIGN_DOCUMENT.md -t "Design Review" -o design_mega.md

  # Extract source modules from review document with custom title
  python create_mega_document.py --review ANALYSIS_REPORT.md -t "Analysis Review" -o analysis_mega.md
        """
    )
    
    # Required arguments
    parser.add_argument("--review", "-r", required=True,
                       help="Review document path to extract source modules from")
    parser.add_argument("--title", "-t", required=True, 
                       help="Document title")
    parser.add_argument("--output", "-o", required=True, 
                       help="Output file path")
    
    args = parser.parse_args()
    
    # Create mega document
    try:
        created_files = create_mega_document(
            review_file=args.review,
            title=args.title,
            output=args.output
        )
        
        if not created_files:
            print("❌ No documents created")
            exit(1)
        else:
            print(f"\n🎉 Success! Created {len(created_files)} document(s)")
            
    except KeyboardInterrupt:
        print("\n❌ Operation cancelled by user")
        exit(1)
    except Exception as e:
        print(f"❌ Error creating mega document: {e}")
        import traceback
        traceback.print_exc()
        exit(1)

```

