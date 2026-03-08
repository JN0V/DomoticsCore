#!/bin/bash

# Full CI validation script for DomoticsCore
# Strategy: component by component, simplest to most complex
# 1. Unit tests (native) per component — stop on first failure
# 2. Example builds per component — stop on first failure
# All output is written to log files for easy error diagnosis.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

LOG_DIR="$SCRIPT_DIR/_ci-logs"
rm -rf "$LOG_DIR"
mkdir -p "$LOG_DIR"

TIMESTAMP=$(date '+%Y%m%d-%H%M%S')
SUMMARY_LOG="$LOG_DIR/summary-$TIMESTAMP.log"

# Component order: simplest (fewest deps) to most complex
COMPONENTS=(
    DomoticsCore-Core
    DomoticsCore-Wifi
    DomoticsCore-Storage
    DomoticsCore-LED
    DomoticsCore-NTP
    DomoticsCore-MQTT
    DomoticsCore-OTA
    DomoticsCore-SystemInfo
    DomoticsCore-RemoteConsole
    DomoticsCore-WebUI
    DomoticsCore-HomeAssistant
    DomoticsCore-System
)

# Integration test suites (in tests/unit/)
INTEGRATION_TESTS=(
    01-optional-dependencies
    02-lifecycle-callback
    05-storage-namespace
    06-webui-refactor
)

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TOTAL_TESTS_PASSED=0
TOTAL_TESTS_FAILED=0
TOTAL_EXAMPLES_PASSED=0
TOTAL_EXAMPLES_FAILED=0

log() {
    echo "$1" | tee -a "$SUMMARY_LOG"
}

log_color() {
    echo -e "$1" | tee -a "$SUMMARY_LOG"
}

# Clean all .pio directories first to avoid stale state
log "=== Cleaning all .pio directories ==="
find "$SCRIPT_DIR" -name ".pio" -type d -not -path "*/node_modules/*" -exec rm -rf {} + 2>/dev/null || true
log "Done."
log ""

########################################
# PHASE 1: Unit tests (native) per component
########################################
log "=============================================="
log "PHASE 1: Unit tests (native) — component by component"
log "=============================================="
log ""

for component in "${COMPONENTS[@]}"; do
    comp_dir="$SCRIPT_DIR/$component"
    test_dir="$comp_dir/test"

    # Skip components without tests
    if [ ! -d "$test_dir" ]; then
        log "[$component] No test/ directory — skipping"
        continue
    fi

    # Check if component has a native env in platformio.ini
    if ! grep -q '\[env:native\]' "$comp_dir/platformio.ini" 2>/dev/null; then
        log "[$component] No native env — skipping unit tests"
        continue
    fi

    log_color "${YELLOW}[$component] Running native unit tests...${NC}"
    test_log="$LOG_DIR/test-${component}.log"

    # Clean .pio for this component before testing
    rm -rf "$comp_dir/.pio" 2>/dev/null || true

    if pio test -e native --project-dir "$comp_dir" > "$test_log" 2>&1; then
        # Extract test counts from output
        summary=$(grep -E "test cases:" "$test_log" | tail -1)
        log_color "  ${GREEN}PASSED${NC} — $summary"
        log "  Log: $test_log"
        TOTAL_TESTS_PASSED=$((TOTAL_TESTS_PASSED + 1))
    else
        log_color "  ${RED}FAILED${NC}"
        log "  Log: $test_log"
        log ""
        log "--- Last 30 lines of failure output ---"
        tail -30 "$test_log" | tee -a "$SUMMARY_LOG"
        log "--- End of failure output ---"
        TOTAL_TESTS_FAILED=$((TOTAL_TESTS_FAILED + 1))
        log ""
        log_color "${RED}STOPPING: Unit test failure in $component${NC}"
        log ""
        log "=============================================="
        log "SUMMARY (aborted at first failure)"
        log "=============================================="
        log "Unit tests passed: $TOTAL_TESTS_PASSED"
        log "Unit tests failed: $TOTAL_TESTS_FAILED"
        log "Examples built: $TOTAL_EXAMPLES_PASSED"
        log "Examples failed: $TOTAL_EXAMPLES_FAILED"
        log "Logs directory: $LOG_DIR"
        exit 1
    fi
done

log ""

########################################
# PHASE 1b: Integration tests (tests/unit/*)
########################################
log "=============================================="
log "PHASE 1b: Integration tests (tests/unit/)"
log "=============================================="
log ""

for suite in "${INTEGRATION_TESTS[@]}"; do
    suite_dir="$SCRIPT_DIR/tests/unit/$suite"

    if [ ! -d "$suite_dir" ] || [ ! -f "$suite_dir/platformio.ini" ]; then
        log "[$suite] Missing — skipping"
        continue
    fi

    log_color "${YELLOW}[$suite] Building integration test...${NC}"
    test_log="$LOG_DIR/integration-${suite}.log"

    # Clean .pio for this test
    rm -rf "$suite_dir/.pio" 2>/dev/null || true

    if pio run -d "$suite_dir" > "$test_log" 2>&1; then
        log_color "  ${GREEN}PASSED${NC}"
        log "  Log: $test_log"
        TOTAL_TESTS_PASSED=$((TOTAL_TESTS_PASSED + 1))
    else
        log_color "  ${RED}FAILED${NC}"
        log "  Log: $test_log"
        log ""
        log "--- Last 30 lines of failure output ---"
        tail -30 "$test_log" | tee -a "$SUMMARY_LOG"
        log "--- End of failure output ---"
        TOTAL_TESTS_FAILED=$((TOTAL_TESTS_FAILED + 1))
        log ""
        log_color "${RED}STOPPING: Integration test failure in $suite${NC}"
        log ""
        log "=============================================="
        log "SUMMARY (aborted at first failure)"
        log "=============================================="
        log "Unit tests passed: $TOTAL_TESTS_PASSED"
        log "Unit tests failed: $TOTAL_TESTS_FAILED"
        log "Examples built: $TOTAL_EXAMPLES_PASSED"
        log "Examples failed: $TOTAL_EXAMPLES_FAILED"
        log "Logs directory: $LOG_DIR"
        exit 1
    fi
done

log ""

########################################
# PHASE 2: Example builds per component
########################################
log "=============================================="
log "PHASE 2: Example builds — component by component"
log "=============================================="
log ""

for component in "${COMPONENTS[@]}"; do
    examples_dir="$SCRIPT_DIR/$component/examples"

    if [ ! -d "$examples_dir" ]; then
        log "[$component] No examples/ directory — skipping"
        continue
    fi

    for example_dir in "$examples_dir"/*/; do
        [ -d "$example_dir" ] || continue
        [ -f "$example_dir/platformio.ini" ] || continue

        example_name=$(basename "$example_dir")
        log_color "${YELLOW}[$component/$example_name] Building...${NC}"
        build_log="$LOG_DIR/build-${component}-${example_name}.log"

        # Clean .pio for this example
        rm -rf "$example_dir/.pio" 2>/dev/null || true

        if pio run -d "$example_dir" > "$build_log" 2>&1; then
            # Extract platform results
            platforms=$(grep -E "^\w+\s+(SUCCESS|FAILED)" "$build_log" | awk '{printf "%s:%s ", $1, $2}')
            log_color "  ${GREEN}PASSED${NC} — $platforms"
            log "  Log: $build_log"
            TOTAL_EXAMPLES_PASSED=$((TOTAL_EXAMPLES_PASSED + 1))
        else
            # Extract which platforms failed
            platforms=$(grep -E "^\w+\s+(SUCCESS|FAILED)" "$build_log" | awk '{printf "%s:%s ", $1, $2}')
            log_color "  ${RED}FAILED${NC} — $platforms"
            log "  Log: $build_log"
            log ""
            log "--- Last 30 lines of failure output ---"
            tail -30 "$build_log" | tee -a "$SUMMARY_LOG"
            log "--- End of failure output ---"
            TOTAL_EXAMPLES_FAILED=$((TOTAL_EXAMPLES_FAILED + 1))
            log ""
            log_color "${RED}STOPPING: Example build failure in $component/$example_name${NC}"
            log ""
            log "=============================================="
            log "SUMMARY (aborted at first failure)"
            log "=============================================="
            log "Unit tests passed: $TOTAL_TESTS_PASSED"
            log "Unit tests failed: $TOTAL_TESTS_FAILED"
            log "Examples built: $TOTAL_EXAMPLES_PASSED"
            log "Examples failed: $TOTAL_EXAMPLES_FAILED"
            log "Logs directory: $LOG_DIR"
            exit 1
        fi
    done
done

# Also build top-level examples if any
if [ -d "$SCRIPT_DIR/examples" ]; then
    for example_dir in "$SCRIPT_DIR/examples"/*/; do
        [ -d "$example_dir" ] || continue
        [ -f "$example_dir/platformio.ini" ] || continue

        example_name=$(basename "$example_dir")
        log_color "${YELLOW}[examples/$example_name] Building...${NC}"
        build_log="$LOG_DIR/build-examples-${example_name}.log"

        rm -rf "$example_dir/.pio" 2>/dev/null || true

        if pio run -d "$example_dir" > "$build_log" 2>&1; then
            platforms=$(grep -E "^\w+\s+(SUCCESS|FAILED)" "$build_log" | awk '{printf "%s:%s ", $1, $2}')
            log_color "  ${GREEN}PASSED${NC} — $platforms"
            log "  Log: $build_log"
            TOTAL_EXAMPLES_PASSED=$((TOTAL_EXAMPLES_PASSED + 1))
        else
            platforms=$(grep -E "^\w+\s+(SUCCESS|FAILED)" "$build_log" | awk '{printf "%s:%s ", $1, $2}')
            log_color "  ${RED}FAILED${NC} — $platforms"
            log "  Log: $build_log"
            log ""
            log "--- Last 30 lines of failure output ---"
            tail -30 "$build_log" | tee -a "$SUMMARY_LOG"
            log "--- End of failure output ---"
            TOTAL_EXAMPLES_FAILED=$((TOTAL_EXAMPLES_FAILED + 1))
            log ""
            log_color "${RED}STOPPING: Example build failure in examples/$example_name${NC}"
            log ""
            log "=============================================="
            log "SUMMARY (aborted at first failure)"
            log "=============================================="
            log "Unit tests passed: $TOTAL_TESTS_PASSED"
            log "Unit tests failed: $TOTAL_TESTS_FAILED"
            log "Examples built: $TOTAL_EXAMPLES_PASSED"
            log "Examples failed: $TOTAL_EXAMPLES_FAILED"
            log "Logs directory: $LOG_DIR"
            exit 1
        fi
    done
fi

log ""
log "=============================================="
log "PHASE 3: Version check"
log "=============================================="
version_log="$LOG_DIR/version-check.log"
if python3 tools/check_versions.py --verbose > "$version_log" 2>&1; then
    log_color "${GREEN}Version check PASSED${NC}"
    cat "$version_log" | tee -a "$SUMMARY_LOG"
else
    log_color "${RED}Version check FAILED${NC}"
    cat "$version_log" | tee -a "$SUMMARY_LOG"
fi

log ""
log "=============================================="
log "FINAL SUMMARY"
log "=============================================="
log "Unit/integration tests passed: $TOTAL_TESTS_PASSED"
log "Unit/integration tests failed: $TOTAL_TESTS_FAILED"
log "Examples built successfully:   $TOTAL_EXAMPLES_PASSED"
log "Examples failed:               $TOTAL_EXAMPLES_FAILED"
log "Logs directory: $LOG_DIR"
log ""

if [ $TOTAL_TESTS_FAILED -eq 0 ] && [ $TOTAL_EXAMPLES_FAILED -eq 0 ]; then
    log_color "${GREEN}ALL CHECKS PASSED${NC}"
    exit 0
else
    log_color "${RED}SOME CHECKS FAILED${NC}"
    exit 1
fi
