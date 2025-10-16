#!/usr/bin/env python3
import re
import os

with open('MULTI_SYMBOL_ROTATION_DETAILED_DESIGN.md', 'r') as f:
    content = f.read()

# Test pattern 1
pattern1 = r'-\s+`([^`]+\.(h|cpp|hpp|c|py|js|ts|java|rs|go|md|txt|sh|bat|yml|yaml|json|xml|sql))`'
matches1 = re.findall(pattern1, content, re.MULTILINE | re.IGNORECASE)

extracted_files = set()
for match in matches1:
    file_path = match[0]
    extracted_files.add(file_path)

file_list = sorted(list(extracted_files))
valid_files = [f for f in file_list if os.path.exists(f)]
invalid_files = [f for f in file_list if not os.path.exists(f)]

print('Total extracted files:', len(file_list))
print('Valid files:', len(valid_files))
print('Invalid files:', len(invalid_files))
print()
print('Valid files:')
for f in valid_files[:10]:
    print('  ', f)
if len(valid_files) > 10:
    print('  ... and', len(valid_files) - 10, 'more')
