"""
PocketDoctor Mark 4 — Master Test Runner [Mark 4 UI Beta]

Discovers and executes all unit test suites in tests/ directory.
Outputs clean test execution metrics and status exit codes.
"""

import unittest
import sys
import os

if __name__ == "__main__":
    print("=" * 65)
    print("PocketDoctor Mark 4 - Test Suite [v4.0.0-beta]")
    print("=" * 65)
    test_dir = os.path.dirname(os.path.abspath(__file__))
    loader = unittest.TestLoader()
    suite = loader.discover(start_dir=test_dir, pattern="test_*.py")
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    print("=" * 65)
    if result.wasSuccessful():
        print("SUCCESS: ALL BETA TESTS PASSED CLEANLY")
        sys.exit(0)
    else:
        print("FAILURE: TEST SUITE ERRORS DETECTED")
        sys.exit(1)
