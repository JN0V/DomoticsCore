#!/bin/bash
set -e

echo "=== DomoticsCore Local CI ==="
echo "Date: $(date)"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

ERRORS=0

echo "1. Checking version consistency..."
if python3 tools/check_versions.py --verbose; then
    echo -e "${GREEN}✓ Version check passed${NC}"
else
    echo -e "${RED}✗ Version check failed${NC}"
    ERRORS=$((ERRORS + 1))
fi
echo ""

echo "2. Running isolated unit tests on native platform..."
ISOLATED_TESTS_PASSED=0
ISOLATED_TESTS_TOTAL=0
for testdir in tests/unit/test_*_isolated; do
    if [ -d "$testdir" ]; then
        ISOLATED_TESTS_TOTAL=$((ISOLATED_TESTS_TOTAL + 1))
        testname=$(basename "$testdir")
        if (cd "$testdir" && pio test -e native 2>/dev/null | grep -q "PASSED"); then
            ISOLATED_TESTS_PASSED=$((ISOLATED_TESTS_PASSED + 1))
            echo -e "  ${GREEN}✓${NC} $testname"
        else
            echo -e "  ${RED}✗${NC} $testname"
        fi
    fi
done
if [ "$ISOLATED_TESTS_TOTAL" -gt 0 ]; then
    if [ "$ISOLATED_TESTS_PASSED" -eq "$ISOLATED_TESTS_TOTAL" ]; then
        echo -e "${GREEN}✓ All isolated tests passed ($ISOLATED_TESTS_PASSED/$ISOLATED_TESTS_TOTAL)${NC}"
    else
        echo -e "${RED}✗ Some isolated tests failed ($ISOLATED_TESTS_PASSED/$ISOLATED_TESTS_TOTAL)${NC}"
        ERRORS=$((ERRORS + 1))
    fi
else
    echo -e "${YELLOW}⚠ No isolated tests found${NC}"
fi
echo ""

echo "3. Checking file sizes (max 800 lines)..."
OVERSIZED=0
while IFS= read -r -d '' file; do
    lines=$(wc -l < "$file")
    if [ "$lines" -gt 800 ]; then
        echo -e "${RED}✗ $file has $lines lines (max 800)${NC}"
        OVERSIZED=$((OVERSIZED + 1))
    fi
done < <(find . -type f \( -name "*.h" -o -name "*.cpp" \) -not -path "./.pio/*" -not -path "./*/\.pio/*" -not -path "*/Generated/*" -print0)

if [ "$OVERSIZED" -eq 0 ]; then
    echo -e "${GREEN}✓ All files under 800 lines${NC}"
else
    echo -e "${RED}✗ $OVERSIZED files exceed 800 lines${NC}"
    ERRORS=$((ERRORS + 1))
fi
echo ""

echo "4. Checking for compiler warnings..."
if pio run -e esp32dev 2>&1 | grep -i "warning:" > /dev/null; then
    echo -e "${YELLOW}⚠ Compiler warnings detected${NC}"
else
    echo -e "${GREEN}✓ No compiler warnings${NC}"
fi
echo ""

echo "5. Building examples (if build_all_examples.sh exists)..."
if [ -f "./build_all_examples.sh" ]; then
    if ./build_all_examples.sh; then
        echo -e "${GREEN}✓ All examples built successfully${NC}"
    else
        echo -e "${RED}✗ Some examples failed to build${NC}"
        ERRORS=$((ERRORS + 1))
    fi
else
    echo -e "${YELLOW}⚠ build_all_examples.sh not found, skipping${NC}"
fi
echo ""

echo "==================================="
if [ "$ERRORS" -eq 0 ]; then
    echo -e "${GREEN}=== All checks passed! ===${NC}"
    exit 0
else
    echo -e "${RED}=== $ERRORS check(s) failed ===${NC}"
    exit 1
fi
