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
