#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BUILD_TYPE="${1:-Release}"
APP="${BUILD_DIR}/stemhub_artefacts/${BUILD_TYPE}/Standalone/Stemhub Session.app"
SOURCE_DIR="${SCRIPT_DIR}/Source"

build_and_launch() {
    echo "[watch] Change detected — rebuilding ${BUILD_TYPE}..."
    if cmake --build "${BUILD_DIR}" --target stemhub_Standalone --config "${BUILD_TYPE}"; then
        pkill -f "Stemhub Session" 2>/dev/null || true
        sleep 0.3
        echo "[watch] Launching Standalone..."
        open "${APP}"
    else
        echo "[watch] Build failed — keeping previous instance running"
    fi
}

if ! command -v fswatch &>/dev/null; then
    echo "Error: fswatch is not installed. Run: brew install fswatch"
    exit 1
fi

echo "[watch] Building ${BUILD_TYPE} Standalone..."
cmake --build "${BUILD_DIR}" --target stemhub_Standalone --config "${BUILD_TYPE}"
open "${APP}"
echo "[watch] Watching ${SOURCE_DIR} for changes..."

fswatch -o "${SOURCE_DIR}" | while read -r _; do
    build_and_launch
done
