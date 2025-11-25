#!/bin/bash

# Exit on error
# set -e  # Don't exit on error so we can run all tests

echo "=========================================="
echo "Running all DomoticsCore Unit Tests"
echo "=========================================="

FAILED=0
PASSED=0

# Find all directories in tests/unit that contain platformio.ini
for test_dir in tests/unit/*; do
    if [ -d "$test_dir" ] && [ -f "$test_dir/platformio.ini" ]; then
        echo ""
        echo "=========================================="
        echo "Test Suite: $(basename $test_dir)"
        echo "=========================================="
        
        # Clean build artifacts
        if [ -d "$test_dir/.pio" ]; then
            rm -rf "$test_dir/.pio"
        fi

        # Run pio run
        # Note: This usually requires a connected device for embedded tests
        if (cd "$test_dir" && pio run); then
            echo "  ✓ PASSED"
            PASSED=$((PASSED + 1))
        else
            echo "  ✗ FAILED"
            FAILED=$((FAILED + 1))
        fi
    fi
done

echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ $FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi
