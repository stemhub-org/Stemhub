#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="${SCRIPT_DIR}"
BUILD_DIR="${PLUGIN_DIR}/build"
BUILD_TYPE="${1:-Release}"

# JUCE release fetched into ${PLUGIN_DIR}/JUCE when no JUCE install is found.
# Keep in sync with JUCE_VERSION in .github/workflows/plugin-build.yml.
JUCE_VERSION="${JUCE_VERSION:-9.0.4}"
JUCE_REPO_URL="https://github.com/juce-framework/JUCE.git"
LOCAL_JUCE_DIR="${PLUGIN_DIR}/JUCE"

for tool in cmake git; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
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
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    -DCOPY_PLUGIN=ON
    "-DJUCE_DIR=${JUCE_DIR}"
)

# Ninja when it is installed and the build folder is new; otherwise CMake's default generator.
if command -v ninja >/dev/null 2>&1 && [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cmake_args+=(-G Ninja)
fi

targets=(stemhub_VST3)
if [[ "$(uname -s)" == "Darwin" ]]; then
    targets+=(stemhub_AU)
fi

cmake "${cmake_args[@]}"
cmake --build "${BUILD_DIR}" --target "${targets[@]}" --config "${BUILD_TYPE}"

echo "Built ${targets[*]} and copied the plugin to your plug-in folder."
echo "Rescan plugins in your DAW, e.g. in FL Studio: Options -> Manage plugins -> Find plugins"
