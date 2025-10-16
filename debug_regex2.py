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
print('Extracted files:', len(file_list))
for f in file_list[:5]:
    exists = os.path.exists(f)
    print('  ', f, 'exists:', exists)
    if not exists:
        print('    -> This file does not exist!')
