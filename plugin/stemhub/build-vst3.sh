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
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    -DCOPY_PLUGIN=ON
)

# Ninja when it is installed and the build folder is new; otherwise CMake's default generator.
if command -v ninja >/dev/null 2>&1 && [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cmake_args+=(-G Ninja)
fi

if [[ -n "${JUCE_DIR:-}" ]]; then
    cmake_args+=("-DJUCE_DIR=${JUCE_DIR}")
fi

targets=(stemhub_VST3)
if [[ "$(uname -s)" == "Darwin" ]]; then
    targets+=(stemhub_AU)
fi

cmake "${cmake_args[@]}"
cmake --build "${BUILD_DIR}" --target "${targets[@]}" --config "${BUILD_TYPE}"

echo "Built ${targets[*]} and copied the plugin to your plug-in folder."
echo "Rescan plugins in your DAW, e.g. in FL Studio: Options -> Manage plugins -> Find plugins"
