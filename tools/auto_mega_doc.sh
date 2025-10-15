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
