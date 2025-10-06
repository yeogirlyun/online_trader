#!/usr/bin/env python3
"""
Enhanced Mega Document Creator

Features:
- Create mega documents from directories OR specific file lists
- Interactive file selection with filtering
- Configurable size limits with optional splitting
- Single mega doc or multi-part support
- Enhanced file type detection and processing

Usage Examples:
  # Directory-based with auto-splitting (300KB parts)
  python create_mega_document.py -d src/ include/ -t "My Project" -desc "Source code"

  # File list with custom size limit
  python create_mega_document.py -f file1.cpp file2.h -t "Selected Files" --max-size 500

  # Single mega doc without splitting
  python create_mega_document.py -d src/ -t "Complete Code" --single-doc

  # Interactive file selection
  python create_mega_document.py -d src/ -t "My Project" --interactive
"""

import os
import argparse
import datetime
import re
import glob
import fnmatch
from pathlib import Path
from typing import List, Optional, Tuple

# Default maximum size per mega document file (300KB)
DEFAULT_MAX_SIZE_KB = 300

def get_file_size_kb(file_path):
    """Get file size in KB."""
    return os.path.getsize(file_path) / 1024

def get_supported_extensions():
    """Get list of supported file extensions."""
    return {
        '.hpp', '.cpp', '.h', '.c', '.cc', '.cxx',  # C/C++
        '.py', '.pyx', '.pyi',  # Python
        '.js', '.ts', '.jsx', '.tsx',  # JavaScript/TypeScript
        '.java', '.kt',  # Java/Kotlin
        '.rs',  # Rust
        '.go',  # Go
        '.md', '.txt', '.rst',  # Documentation
        '.cmake', '.yml', '.yaml', '.json', '.xml',  # Config
        '.sql', '.sh', '.bat'  # Scripts and queries
    }

def collect_files_from_directories(directories: List[str], extensions: set = None) -> List[str]:
    """Collect files from directories based on supported extensions."""
    if extensions is None:
        extensions = get_supported_extensions()
    
    all_files = []
    for directory in directories:
        if os.path.exists(directory):
            for root, dirs, files in os.walk(directory):
                # Skip hidden directories and common build/cache directories
                dirs[:] = [d for d in dirs if not d.startswith('.') and d not in {'build', '__pycache__', 'node_modules'}]
                
                for file in files:
                    file_path = os.path.join(root, file)
                    # Check extension or special files like CMakeLists.txt
                    if (Path(file).suffix.lower() in extensions or 
                        file in {'CMakeLists.txt', 'Makefile', 'Dockerfile', 'README'}):
                        all_files.append(file_path)
    
    return sorted(all_files)

def filter_files_interactive(files: List[str]) -> List[str]:
    """Interactive file filtering with pattern matching."""
    if not files:
        print("❌ No files found to filter.")
        return []
    
    print(f"\n📁 Found {len(files)} files. Interactive filtering options:")
    print("1. Include all files")
    print("2. Filter by pattern (regex or glob)")
    print("3. Filter by file type")
    print("4. Manual selection (up to 50 files)")
    
    while True:
        try:
            choice = input("\nChoose filtering option (1-4): ").strip()
            
            if choice == '1':
                return files
            
            elif choice == '2':
                pattern = input("Enter pattern (regex or glob, e.g., '*.cpp' or '.*test.*'): ").strip()
                if not pattern:
                    continue
                
                # Try as glob pattern first
                if '*' in pattern or '?' in pattern:
                    filtered = []
                    for file in files:
                        if fnmatch.fnmatch(os.path.basename(file), pattern) or \
                           fnmatch.fnmatch(file, pattern):
                            filtered.append(file)
                else:
                    # Try as regex
                    try:
                        regex = re.compile(pattern, re.IGNORECASE)
                        filtered = [f for f in files if regex.search(f)]
                    except re.error:
                        print(f"❌ Invalid regex pattern: {pattern}")
                        continue
                
                if filtered:
                    print(f"✅ Filtered to {len(filtered)} files")
                    return filtered
                else:
                    print("❌ No files match the pattern")
                    continue
            
            elif choice == '3':
                extensions = set()
                for file in files:
                    ext = Path(file).suffix.lower()
                    if ext:
                        extensions.add(ext)
                
                print(f"Available extensions: {', '.join(sorted(extensions))}")
                ext_input = input("Enter extensions to include (space-separated, e.g., '.cpp .h'): ").strip()
                
                if ext_input:
                    target_extensions = {ext.lower() for ext in ext_input.split()}
                    filtered = [f for f in files if Path(f).suffix.lower() in target_extensions]
                    
                    if filtered:
                        print(f"✅ Filtered to {len(filtered)} files")
                        return filtered
                    else:
                        print("❌ No files match the extensions")
                        continue
            
            elif choice == '4':
                if len(files) > 50:
                    print("⚠️ Too many files for manual selection. Showing first 50:")
                    display_files = files[:50]
                else:
                    display_files = files
                
                print("\nFiles:")
                for i, file in enumerate(display_files, 1):
                    size_kb = get_file_size_kb(file) if os.path.exists(file) else 0
                    print(f"{i:3d}. {file} ({size_kb:.1f} KB)")
                
                selection = input("\nEnter file numbers (space-separated, e.g., '1 5 10-15'): ").strip()
                if not selection:
                    continue
                
                selected_indices = set()
                for part in selection.split():
                    if '-' in part:
                        try:
                            start, end = map(int, part.split('-'))
                            selected_indices.update(range(start, end + 1))
                        except ValueError:
                            print(f"❌ Invalid range: {part}")
                    else:
                        try:
                            selected_indices.add(int(part))
                        except ValueError:
                            print(f"❌ Invalid number: {part}")
                
                # Convert to file list
                filtered = []
                for idx in sorted(selected_indices):
                    if 1 <= idx <= len(display_files):
                        filtered.append(display_files[idx - 1])
                
                if filtered:
                    print(f"✅ Selected {len(filtered)} files")
                    return filtered
                else:
                    print("❌ No valid files selected")
                    continue
            
            else:
                print("❌ Invalid choice. Please enter 1-4.")
                continue
                
        except KeyboardInterrupt:
            print("\n❌ Selection cancelled.")
            return []
        except EOFError:
            print("\n❌ Selection cancelled.")
            return []

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

def create_document_header(title, description, part_num=None, total_parts=None, source_info=""):
    """Create the header for a mega document."""
    header = f"# {title}\n\n"
    if part_num and total_parts:
        header += f"**Part {part_num} of {total_parts}**\n\n"
    header += f"**Generated**: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
    header += f"**Working Directory**: {os.getcwd()}\n"
    if source_info:
        header += f"**Source**: {source_info}\n"
    header += f"**Description**: {description}\n\n"
    if part_num and total_parts:
        header += f"**Part {part_num} Files**: See file count below\n\n"
    else:
        header += f"**Total Files**: See file count below\n\n"
    header += "---\n\n"
    return header

def create_table_of_contents(all_files, start_idx=0, end_idx=None):
    """Create table of contents for a range of files."""
    if end_idx is None:
        end_idx = len(all_files)
    
    toc = "## 📋 **TABLE OF CONTENTS**\n\n"
    for i in range(start_idx, end_idx):
        file_path = all_files[i]
        toc += f"{i+1}. [{file_path}](#file-{i+1})\n"
    toc += "\n---\n\n"
    return toc

def write_file_content(f, file_path, file_num, total_files):
    """Write a single file's content to the mega document."""
    try:
        with open(file_path, 'r', encoding='utf-8') as file_f:
            content = file_f.read()
        
        f.write(f"## 📄 **FILE {file_num} of {total_files}**: {file_path}\n\n")
        f.write("**File Information**:\n")
        f.write(f"- **Path**: `{file_path}`\n\n")
        f.write(f"- **Size**: {len(content.splitlines())} lines\n")
        f.write(f"- **Modified**: {datetime.datetime.fromtimestamp(os.path.getmtime(file_path)).strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write(f"- **Type**: {Path(file_path).suffix}\n\n")
        f.write("```text\n")
        f.write(content)
        f.write("\n```\n\n")
        
        print(f"📄 Processing file {file_num}/{total_files}: {file_path}")
        return True
        
    except Exception as e:
        print(f"❌ Error processing {file_path}: {e}")
        f.write(f"## 📄 **FILE {file_num} of {total_files}**: {file_path}\n\n")
        f.write(f"**Error**: Could not read file - {e}\n\n")
        return False

def create_mega_document_part(output_path, all_files, start_idx, end_idx, title, description, 
                            part_num=None, total_parts=None, include_bug_report=False, bug_report_file=None,
                            source_info=""):
    """Create a single part of the mega document."""
    
    with open(output_path, 'w', encoding='utf-8') as f:
        # Write header
        f.write(create_document_header(title, description, part_num, total_parts, source_info))
        
        # Include bug report if requested (only in first part)
        if include_bug_report and bug_report_file and os.path.exists(bug_report_file) and (part_num is None or part_num == 1):
            f.write("## 🐛 **BUG REPORT**\n\n")
            with open(bug_report_file, 'r', encoding='utf-8') as bug_f:
                f.write(bug_f.read())
            f.write("\n\n---\n\n")
        
        # Table of contents for this part
        f.write(create_table_of_contents(all_files, start_idx, end_idx))
        
        # File contents
        files_in_part = 0
        for i in range(start_idx, end_idx):
            file_path = all_files[i]
            if write_file_content(f, file_path, i+1, len(all_files)):
                files_in_part += 1
    
    return files_in_part

def create_mega_document(directories=None, file_list=None, title="", description="", output="", 
                        include_bug_report=False, bug_report_file=None, interactive=False,
                        single_doc=False, max_size_kb=DEFAULT_MAX_SIZE_KB):
    """Enhanced mega document creation with flexible input options."""
    
    print(f"🚀 Enhanced Mega Document Creator")
    print(f"================================")
    print(f"📁 Working Directory: {os.getcwd()}")
    print(f"📁 Output: {output}")
    
    # Collect files from different sources
    all_files = []
    source_info = ""
    
    if file_list:
        # Direct file list provided
        print(f"📂 Processing file list: {len(file_list)} files specified")
        valid_files, invalid_files = validate_file_list(file_list)
        
        if invalid_files:
            print(f"⚠️  Invalid files (will be skipped): {len(invalid_files)}")
            for invalid_file in invalid_files:
                print(f"   ❌ {invalid_file}")
        
        all_files = valid_files
        source_info = f"File list ({len(valid_files)} files)"
        
    elif directories:
        # Directory-based collection
        print(f"📂 Scanning directories: {directories}")
        all_files = collect_files_from_directories(directories)
        source_info = f"Directories: {', '.join(directories)}"
    
    else:
        print("❌ No input source specified (directories or file list)")
        return []
    
    print(f"📁 Initial file count: {len(all_files)}")
    
    if not all_files:
        print("❌ No valid files found to process")
        return []
    
    # Interactive filtering if requested
    if interactive:
        print(f"\n🔄 Interactive file selection enabled...")
        all_files = filter_files_interactive(all_files)
        if not all_files:
            print("❌ No files selected")
            return []
    
    # Sort files for consistent ordering
    all_files.sort()
    
    # Calculate total estimated size
    total_size = sum(os.path.getsize(f) for f in all_files if os.path.exists(f))
    max_size_bytes = max_size_kb * 1024
    
    print(f"\n📊 Processing Statistics:")
    print(f"   Files to process: {len(all_files)}")
    print(f"   Total size: {total_size / 1024:.1f} KB")
    print(f"   Max size per part: {max_size_kb} KB")
    
    # Determine splitting strategy
    if single_doc:
        print(f"   Mode: Single document (no size limit)")
        estimated_parts = 1
    else:
        estimated_parts = max(1, int(total_size / max_size_bytes) + 1)
        print(f"   Estimated parts: {estimated_parts}")
    
    # Create document(s)
    created_files = []
    
    if single_doc or estimated_parts == 1:
        # Create single document
        print(f"\n📝 Creating single mega document...")
        
        files_in_part = create_mega_document_part(
            output, all_files, 0, len(all_files), title, description,
            None, None, include_bug_report, bug_report_file, source_info
        )
        
        actual_size_kb = get_file_size_kb(output)
        print(f"📊 Document size: {actual_size_kb:.1f} KB ({files_in_part} files)")
        created_files.append(output)
        
        if not single_doc and actual_size_kb > max_size_kb:
            print(f"⚠️  Document exceeds {max_size_kb}KB. Consider using --max-size or removing --single-doc")
    
    else:
        # Create multiple parts
        print(f"\n📝 Creating multi-part document ({estimated_parts} parts)...")
        
        current_part = 1
        start_idx = 0
        files_per_part = len(all_files) // estimated_parts + 1
        
        while start_idx < len(all_files):
            # Calculate end index for this part
            end_idx = min(start_idx + files_per_part, len(all_files))
            
            # Create output filename for this part
            base_name = Path(output).stem
            extension = Path(output).suffix
            output_path = f"{base_name}_part{current_part}{extension}"
            
            print(f"  📄 Creating part {current_part}/{estimated_parts}: {output_path}")
            
            # Create this part
            files_in_part = create_mega_document_part(
                output_path, all_files, start_idx, end_idx, title, description,
                current_part, estimated_parts, include_bug_report, bug_report_file, source_info
            )
            
            # Check actual file size
            actual_size_kb = get_file_size_kb(output_path)
            print(f"     Size: {actual_size_kb:.1f} KB ({files_in_part} files)")
            
            created_files.append(output_path)
            
            # Adaptive splitting if part is too large
            if actual_size_kb > max_size_kb * 1.5:  # 50% tolerance
                print(f"     ⚠️  Part exceeds size limit, will split more aggressively next time")
            
            start_idx = end_idx
            current_part += 1
    
    # Summary
    print(f"\n✅ Mega Document Creation Complete!")
    print(f"📊 Summary:")
    print(f"   Parts created: {len(created_files)}")
    print(f"   Total files processed: {len(all_files)}")
    print(f"   Source: {source_info}")
    
    for i, file_path in enumerate(created_files, 1):
        size_kb = get_file_size_kb(file_path)
        abs_path = os.path.abspath(file_path)
        if len(created_files) > 1:
            print(f"📄 Part {i}: {abs_path} ({size_kb:.1f} KB)")
        else:
            print(f"📄 Document: {abs_path} ({size_kb:.1f} KB)")
    
    return created_files

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Enhanced Mega Document Creator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Directory-based with auto-splitting (300KB parts)
  python create_mega_document.py -d src/ include/ -t "My Project" -desc "Source code" -o project.md

  # File list with custom size limit  
  python create_mega_document.py -f src/main.cpp include/header.h -t "Selected Files" -desc "Core files" -o core.md --max-size 500

  # Single mega doc without splitting
  python create_mega_document.py -d src/ -t "Complete Code" -desc "All source" -o complete.md --single-doc

  # Interactive file selection
  python create_mega_document.py -d src/ -t "My Project" -desc "Interactive selection" -o selected.md --interactive

  # File list with pattern-based filtering
  python create_mega_document.py -d src/ include/ -t "C++ Files" -desc "C++ source" -o cpp.md --interactive
        """
    )
    
    # Input source options (mutually exclusive)
    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument("--directories", "-d", nargs="+", 
                           help="Directories to scan for files")
    input_group.add_argument("--files", "-f", nargs="+",
                           help="Specific files to include")
    
    # Required arguments
    parser.add_argument("--title", "-t", required=True, 
                       help="Document title")
    parser.add_argument("--description", "-desc", required=True, 
                       help="Document description")
    parser.add_argument("--output", "-o", required=True, 
                       help="Output file path")
    
    # Optional features
    parser.add_argument("--interactive", "-i", action="store_true",
                       help="Interactive file selection and filtering")
    parser.add_argument("--single-doc", "-s", action="store_true",
                       help="Create single document without size-based splitting")
    parser.add_argument("--max-size", "-m", type=int, default=DEFAULT_MAX_SIZE_KB,
                       help=f"Maximum size per part in KB (default: {DEFAULT_MAX_SIZE_KB})")
    
    # Bug report integration
    parser.add_argument("--include-bug-report", action="store_true", 
                       help="Include bug report in document")
    parser.add_argument("--bug-report-file", 
                       help="Bug report file path")
    
    # Additional options
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Verbose output")
    
    args = parser.parse_args()
    
    # Validate arguments
    if args.include_bug_report and not args.bug_report_file:
        parser.error("--include-bug-report requires --bug-report-file")
    
    if args.max_size < 10:
        parser.error("--max-size must be at least 10 KB")
    
    if args.single_doc and args.max_size != DEFAULT_MAX_SIZE_KB:
        print("⚠️  --max-size ignored when --single-doc is used")
    
    # Create mega document with new enhanced function
    try:
        created_files = create_mega_document(
            directories=args.directories,
            file_list=args.files,
            title=args.title,
            description=args.description,
            output=args.output,
            include_bug_report=args.include_bug_report,
            bug_report_file=args.bug_report_file,
            interactive=args.interactive,
            single_doc=args.single_doc,
            max_size_kb=args.max_size
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
        if args.verbose:
            import traceback
            traceback.print_exc()
        exit(1)
