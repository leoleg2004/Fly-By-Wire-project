#!/usr/bin/env python3
"""
patch_fastdds3.py
Patches generated fastddsgen files for FastDDS 3.x .hpp header names.
Safe to run multiple times (idempotent).
"""
import os, sys, re

REPLACEMENTS = [
    # fastrtps/utils/md5.h -> fastdds/utils/md5.hpp
    (r'fastrtps/utils/md5\.h(?!pp)', 'fastdds/utils/md5.hpp'),
    # fastrtps/utils/fixed_size_string.hpp -> fastcdr/cdr/fixed_size_string.hpp
    (r'fastrtps/utils/fixed_size_string\.hpp', 'fastcdr/cdr/fixed_size_string.hpp'),
    # fastdds/rtps/common/InstanceHandle.h -> .hpp
    (r'fastdds/rtps/common/InstanceHandle\.h(?!pp)', 'fastdds/rtps/common/InstanceHandle.hpp'),
    # fastdds/rtps/common/SerializedPayload.h -> .hpp
    (r'fastdds/rtps/common/SerializedPayload\.h(?!pp)', 'fastdds/rtps/common/SerializedPayload.hpp'),
    # fastdds/rtps/common/Types.h -> .hpp
    (r'fastdds/rtps/common/Types\.h(?!pp)', 'fastdds/rtps/common/Types.hpp'),
]

folder = sys.argv[1] if len(sys.argv) > 1 else '.'

for fname in os.listdir(folder):
    fpath = os.path.join(folder, fname)
    if not os.path.isfile(fpath):
        continue
    try:
        with open(fpath, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception:
        continue
    new_content = content
    for pattern, replacement in REPLACEMENTS:
        new_content = re.sub(pattern, replacement, new_content)
    if new_content != content:
        with open(fpath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"  patched: {fname}")
