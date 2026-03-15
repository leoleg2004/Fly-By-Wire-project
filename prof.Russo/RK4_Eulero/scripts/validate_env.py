#!/usr/bin/env python3
"""
Validate F-16 RK4_Eulero project environment for Eclipse/PyDev.

Checks:
  - Python version (3.10+)
  - Required dependencies (vpython, scipy, numpy)
  - File paths match project layout
  - venv active (if applicable)
"""

import sys
import os
import importlib.util

def check_python_version():
    """Check Python version >= 3.10."""
    version = sys.version_info
    if version.major < 3 or (version.major == 3 and version.minor < 10):
        print(f"❌ Python {version.major}.{version.minor} detected. Require >= 3.10")
        return False
    print(f"✅ Python {version.major}.{version.minor}.{version.micro}")
    return True

def check_dependencies():
    """Check required packages are importable."""
    deps = ['numpy', 'scipy', 'vpython']
    all_ok = True
    for dep in deps:
        try:
            __import__(dep)
            print(f"✅ {dep}")
        except ImportError as e:
            print(f"❌ {dep}: {e}")
            all_ok = False
    return all_ok

def check_file_paths():
    """Check project file structure."""
    files_to_check = [
        'simulazione/py/Simulazione.py',
        '.project',
        '.pydevproject',
    ]
    all_ok = True
    for fpath in files_to_check:
        full_path = os.path.join(os.path.dirname(__file__), '..', fpath)
        if os.path.exists(full_path):
            print(f"✅ {fpath}")
        else:
            print(f"❌ {fpath} not found at {full_path}")
            all_ok = False
    return all_ok

def check_interpreter():
    """Check which Python interpreter is active."""
    print(f"\nInterpreter: {sys.executable}")
    if 'venv' in sys.prefix or 'VIRTUAL_ENV' in os.environ:
        print(f"✅ venv active: {sys.prefix}")
    else:
        print(f"ℹ️  System Python (no venv)")
    return True

def main():
    print("=" * 60)
    print("F-16 RK4_Eulero Project Environment Validation")
    print("=" * 60)

    checks = [
        ("Python Version", check_python_version),
        ("Dependencies", check_dependencies),
        ("File Paths", check_file_paths),
        ("Interpreter", check_interpreter),
    ]

    results = []
    for name, check_fn in checks:
        print(f"\n{name}:")
        try:
            result = check_fn()
            results.append(result)
        except Exception as e:
            print(f"❌ Error: {e}")
            results.append(False)

    print("\n" + "=" * 60)
    if all(results):
        print("✅ All checks passed. Ready to run in Eclipse/PyDev.")
        return 0
    else:
        print("❌ Some checks failed. Fix issues above.")
        return 1

if __name__ == '__main__':
    sys.exit(main())
