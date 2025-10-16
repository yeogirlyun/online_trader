#!/usr/bin/env python3
"""
Test runner and configuration for the mega document creation system.

This module provides test discovery, execution, and reporting functionality.
"""

import pytest
import sys
import os
from pathlib import Path

# Add project root to Python path
project_root = Path(__file__).parent.parent
sys.path.insert(0, str(project_root))


def run_tests():
    """Run all tests with appropriate configuration."""
    
    # Test discovery patterns
    test_patterns = [
        "tests/unit/test_*.py",
        "tests/integration/test_*.py", 
        "tests/security/test_*.py"
    ]
    
    # Test configuration
    pytest_args = [
        "-v",  # Verbose output
        "--tb=short",  # Short traceback format
        "--strict-markers",  # Strict marker checking
        "--disable-warnings",  # Disable warnings for cleaner output
        "--color=yes",  # Colored output
        "-x",  # Stop on first failure
    ]
    
    # Add test patterns
    pytest_args.extend(test_patterns)
    
    print("🧪 Running Mega Document Creation System Tests")
    print("=" * 60)
    print(f"📁 Project Root: {project_root}")
    print(f"🔍 Test Patterns: {test_patterns}")
    print()
    
    # Run tests
    exit_code = pytest.main(pytest_args)
    
    if exit_code == 0:
        print("\n✅ All tests passed!")
    else:
        print(f"\n❌ Tests failed with exit code: {exit_code}")
    
    return exit_code


def run_unit_tests_only():
    """Run only unit tests."""
    pytest_args = [
        "-v",
        "--tb=short",
        "--color=yes",
        "tests/unit/"
    ]
    
    print("🧪 Running Unit Tests Only")
    print("=" * 30)
    
    exit_code = pytest.main(pytest_args)
    return exit_code


def run_integration_tests_only():
    """Run only integration tests."""
    pytest_args = [
        "-v",
        "--tb=short", 
        "--color=yes",
        "tests/integration/"
    ]
    
    print("🧪 Running Integration Tests Only")
    print("=" * 35)
    
    exit_code = pytest.main(pytest_args)
    return exit_code


def run_security_tests_only():
    """Run only security tests."""
    pytest_args = [
        "-v",
        "--tb=short",
        "--color=yes", 
        "tests/security/"
    ]
    
    print("🧪 Running Security Tests Only")
    print("=" * 32)
    
    exit_code = pytest.main(pytest_args)
    return exit_code


def run_coverage_tests():
    """Run tests with coverage reporting."""
    try:
        import coverage
    except ImportError:
        print("❌ Coverage module not installed. Install with: pip install coverage")
        return 1
    
    pytest_args = [
        "-v",
        "--tb=short",
        "--color=yes",
        "--cov=config",
        "--cov=exceptions", 
        "--cov=repositories",
        "--cov=services",
        "--cov-report=html",
        "--cov-report=term-missing",
        "tests/"
    ]
    
    print("🧪 Running Tests with Coverage")
    print("=" * 35)
    
    exit_code = pytest.main(pytest_args)
    return exit_code


def run_performance_tests():
    """Run performance-focused tests."""
    pytest_args = [
        "-v",
        "--tb=short",
        "--color=yes",
        "-m", "performance",
        "tests/"
    ]
    
    print("🧪 Running Performance Tests")
    print("=" * 30)
    
    exit_code = pytest.main(pytest_args)
    return exit_code


if __name__ == "__main__":
    if len(sys.argv) > 1:
        test_type = sys.argv[1].lower()
        
        if test_type == "unit":
            exit_code = run_unit_tests_only()
        elif test_type == "integration":
            exit_code = run_integration_tests_only()
        elif test_type == "security":
            exit_code = run_security_tests_only()
        elif test_type == "coverage":
            exit_code = run_coverage_tests()
        elif test_type == "performance":
            exit_code = run_performance_tests()
        else:
            print(f"❌ Unknown test type: {test_type}")
            print("Available types: unit, integration, security, coverage, performance")
            exit_code = 1
    else:
        exit_code = run_tests()
    
    sys.exit(exit_code)
