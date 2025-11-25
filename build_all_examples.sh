#!/bin/bash

# Script to test compilation of all examples in DomoticsCore modules

set -e

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

FAILED_EXAMPLES=()
PASSED_EXAMPLES=()

echo "=========================================="
echo "Testing all DomoticsCore examples"
echo "=========================================="
echo ""

# Find all examples directories
EXAMPLE_DIRS=$(find . -type d -name "examples" | grep -v ".pio" | sort)

for EXAMPLE_DIR in $EXAMPLE_DIRS; do
    MODULE=$(dirname "$EXAMPLE_DIR")
    echo ""
    echo "=========================================="
    echo "Module: $MODULE"
    echo "=========================================="
    
    # Find all example subdirectories
    EXAMPLES=$(find "$EXAMPLE_DIR" -mindepth 1 -maxdepth 1 -type d | sort)
    
    for EXAMPLE in $EXAMPLES; do
        EXAMPLE_NAME=$(basename "$EXAMPLE")
        echo ""
        echo -e "${YELLOW}Testing: $EXAMPLE_NAME${NC}"
        
        # Check if platformio.ini exists
        if [ ! -f "$EXAMPLE/platformio.ini" ]; then
            echo -e "${YELLOW}  ⊘ No platformio.ini - Skipping${NC}"
            continue
        fi
        
        # Compile the example
        if pio run -d "$EXAMPLE" -t clean > /dev/null 2>&1 && pio run -d "$EXAMPLE" > /dev/null 2>&1; then
            echo -e "${GREEN}  ✓ PASSED${NC}"
            PASSED_EXAMPLES+=("$MODULE/$EXAMPLE_NAME")
        else
            echo -e "${RED}  ✗ FAILED${NC}"
            FAILED_EXAMPLES+=("$MODULE/$EXAMPLE_NAME")
            
            # Show compilation error
            echo ""
            echo "  --- Error details ---"
            pio run -d "$EXAMPLE" 2>&1 | tail -n 20
            echo "  ---------------------"
        fi
    done
done

echo ""
echo "=========================================="
echo "SUMMARY"
echo "=========================================="
echo -e "${GREEN}Passed: ${#PASSED_EXAMPLES[@]}${NC}"
echo -e "${RED}Failed: ${#FAILED_EXAMPLES[@]}${NC}"

if [ ${#FAILED_EXAMPLES[@]} -gt 0 ]; then
    echo ""
    echo "Failed examples:"
    for EXAMPLE in "${FAILED_EXAMPLES[@]}"; do
        echo -e "  ${RED}✗${NC} $EXAMPLE"
    done
    exit 1
else
    echo ""
    echo -e "${GREEN}All examples compiled successfully!${NC}"
    exit 0
fi
