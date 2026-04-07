#!/bin/bash
# Run mooneye test suite ROMs and report results
#
# Usage:
#   ./scripts/run-mooneye.sh <rom_directory>
#   ./scripts/run-mooneye.sh path/to/mooneye-test-suite/acceptance
#   ./scripts/run-mooneye.sh path/to/mooneye-test-suite/acceptance --verbose

set -e

ROM_DIR="${1:?Usage: $0 <rom_directory> [--verbose]}"
VERBOSE="${2:-}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build-test"
RUNNER="$BUILD_DIR/mooneye_runner"

# Build the runner
mkdir -p "$BUILD_DIR"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DBUILD_SDL=OFF > /dev/null 2>&1
cmake --build "$BUILD_DIR" --target mooneye_runner > /dev/null 2>&1

if [ ! -x "$RUNNER" ]; then
    echo "ERROR: Failed to build mooneye_runner"
    exit 2
fi

PASS=0
FAIL=0
TOTAL=0
FAILED_TESTS=""

# Find all .gb ROM files
while IFS= read -r rom; do
    TOTAL=$((TOTAL + 1))
    rel_path="${rom#$ROM_DIR/}"

    if "$RUNNER" "$rom" $VERBOSE; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        FAILED_TESTS="$FAILED_TESTS  $rel_path\n"
    fi
done < <(find "$ROM_DIR" -name "*.gb" -type f | sort)

echo ""
echo "=============================="
echo "Results: $PASS/$TOTAL passed, $FAIL failed"
echo "=============================="

if [ $FAIL -gt 0 ]; then
    echo ""
    echo "Failed tests:"
    printf "$FAILED_TESTS"
    exit 1
fi
