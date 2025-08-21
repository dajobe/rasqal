#!/usr/bin/env python3
"""
Test script for crash detection system.

This script simulates different types of crashes and exit codes
to test the enhanced crash reporting in the test runner.
"""

import sys
import signal
import os
import time


def simulate_segfault():
    """Simulate a segmentation fault."""
    print("About to cause a segmentation fault...")
    print("This should trigger crash detection in the test runner.")
    time.sleep(0.1)  # Give time for output to be captured
    os.kill(os.getpid(), signal.SIGSEGV)


def simulate_abort():
    """Simulate an abort."""
    print("About to abort...")
    print("This should trigger crash detection in the test runner.")
    time.sleep(0.1)  # Give time for output to be captured
    os.abort()


def simulate_bus_error():
    """Simulate a bus error."""
    print("About to cause a bus error...")
    print("This should trigger crash detection in the test runner.")
    time.sleep(0.1)  # Give time for output to be captured
    os.kill(os.getpid(), signal.SIGBUS)


def simulate_normal_exit(exit_code):
    """Simulate a normal exit with the given code."""
    print(f"Exiting normally with code {exit_code}")
    print("This should NOT trigger crash detection in the test runner.")
    sys.exit(exit_code)


def main():
    """Main function to handle command line arguments."""
    if len(sys.argv) < 2:
        print("Usage: python test_crash_detection.py <crash_type> [exit_code]")
        print("")
        print("Crash types:")
        print("  segfault  - Simulate segmentation fault")
        print("  abort     - Simulate abort")
        print("  buserror  - Simulate bus error")
        print("  normal    - Normal exit (requires exit_code)")
        print("")
        print("Examples:")
        print("  python test_crash_detection.py segfault")
        print("  python test_crash_detection.py normal 1")
        print("  python test_crash_detection.py normal 0")
        sys.exit(1)
    
    crash_type = sys.argv[1].lower()
    
    if crash_type == "segfault":
        simulate_segfault()
    elif crash_type == "abort":
        simulate_abort()
    elif crash_type == "buserror":
        simulate_bus_error()
    elif crash_type == "normal":
        if len(sys.argv) < 3:
            print("Error: normal exit requires an exit code")
            sys.exit(1)
        try:
            exit_code = int(sys.argv[2])
            simulate_normal_exit(exit_code)
        except ValueError:
            print("Error: exit code must be an integer")
            sys.exit(1)
    else:
        print(f"Unknown crash type: {crash_type}")
        sys.exit(1)


if __name__ == "__main__":
    main()
