#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="${SCRIPT_DIR}"
BUILD_DIR="${PLUGIN_DIR}/build"
BUILD_TYPE="${1:-Release}"

if [[ -z "${JUCE_DIR:-}" ]] && [[ ! -f "${PLUGIN_DIR}/JUCE/CMakeLists.txt" ]]; then
    echo "Error: JUCE_DIR is not set and ${PLUGIN_DIR}/JUCE was not found."
    echo "Run with: JUCE_DIR=/absolute/path/to/JUCE ${0} [Release|Debug]"
    exit 1
fi

cmake_args=(
    -S "${PLUGIN_DIR}"
    -B "${BUILD_DIR}"
    -G Ninja
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    -DCOPY_PLUGIN=ON
)

if [[ -n "${JUCE_DIR:-}" ]]; then
    cmake_args+=("-DJUCE_DIR=${JUCE_DIR}")
fi

cmake "${cmake_args[@]}"
cmake --build "${BUILD_DIR}" --target stemhub_VST3 --config "${BUILD_TYPE}"

if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "Built VST3 target. Rescan plugins in FL Studio from:"
    echo "  Options -> Manage plugins -> Find plugins"
fi
