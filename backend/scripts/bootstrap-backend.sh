#!/usr/bin/env bash
set -euo pipefail

git submodule sync --recursive
git submodule update --init --recursive
pip install -e backend/vendor/PyFLP_enhanced
pip install -e backend

