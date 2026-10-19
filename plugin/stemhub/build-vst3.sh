#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="${SCRIPT_DIR}"
BUILD_DIR="${PLUGIN_DIR}/build"
BUILD_TYPE="${1:-Release}"

# JUCE release fetched into ${PLUGIN_DIR}/JUCE when no JUCE install is found.
JUCE_VERSION="${JUCE_VERSION:-9.0.4}"
JUCE_REPO_URL="https://github.com/juce-framework/JUCE.git"
LOCAL_JUCE_DIR="${PLUGIN_DIR}/JUCE"

for tool in cmake ninja git; do
    if ! command -v "${tool}" &>/dev/null; then
        echo "Error: ${tool} is not installed."
        if [[ "$(uname -s)" == "Darwin" ]]; then
            echo "Run: brew install ${tool}"
        fi
        exit 1
    fi
done

if [[ -n "${JUCE_DIR:-}" ]]; then
    if [[ ! -f "${JUCE_DIR}/CMakeLists.txt" ]]; then
        echo "Error: JUCE_DIR=${JUCE_DIR} does not contain a JUCE checkout."
        echo "Unset JUCE_DIR to let this script download JUCE ${JUCE_VERSION} into ${LOCAL_JUCE_DIR}."
        exit 1
    fi
else
    if [[ ! -f "${LOCAL_JUCE_DIR}/CMakeLists.txt" ]]; then
        if [[ -e "${LOCAL_JUCE_DIR}" ]]; then
            echo "Error: ${LOCAL_JUCE_DIR} exists but is not a JUCE checkout. Remove it and re-run."
            exit 1
        fi
        echo "JUCE not found. Downloading JUCE ${JUCE_VERSION} into ${LOCAL_JUCE_DIR}..."
        git clone --depth 1 --branch "${JUCE_VERSION}" "${JUCE_REPO_URL}" "${LOCAL_JUCE_DIR}"
    fi
    JUCE_DIR="${LOCAL_JUCE_DIR}"
fi

echo "Using JUCE at: ${JUCE_DIR}"

# Always pass JUCE_DIR so a path cached by a previous configure (e.g. a JUCE
# install that has since been removed) cannot break the build.
cmake_args=(
    -S "${PLUGIN_DIR}"
    -B "${BUILD_DIR}"
    -G Ninja
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    -DCOPY_PLUGIN=ON
    "-DJUCE_DIR=${JUCE_DIR}"
)

cmake "${cmake_args[@]}"
cmake --build "${BUILD_DIR}" --target stemhub_VST3 --config "${BUILD_TYPE}"

if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "Built VST3 target. Rescan plugins in FL Studio from:"
    echo "  Options -> Manage plugins -> Find plugins"
fi
