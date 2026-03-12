#!/usr/bin/env bash
# setup.sh - Configure NuttX build environment for external board/app tree
# Usage: scripts/setup.sh [config] [build_dir]
#   config:    defconfig name under boards/arm/stm32/olimexino-stm32/configs/
#              default: can-io
#   build_dir: CMake build directory, default: build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

NUTTX_DIR="${PROJECT_ROOT}/nuttx"
APPS_DIR="${PROJECT_ROOT}/nuttx-apps"
CUSTOM_APPS_DIR="${PROJECT_ROOT}/custom-apps"
CUSTOM_BOARDS_DIR="${PROJECT_ROOT}/boards"
BUILD_DIR="${PROJECT_ROOT}/${2:-build}"

CONFIG="${1:-can-io}"
BOARD="olimexino-stm32"
BOARD_ARCH="arm/stm32"

# Sanity checks
for d in "$NUTTX_DIR" "$APPS_DIR"; do
    if [[ ! -d "$d" ]]; then
        echo "ERROR: Required directory not found: $d" >&2
        exit 1
    fi
done

# Determine BOARD_CONFIG:
#   Out-of-tree: absolute path to the config directory
#   In-tree fallback: <board>:<config>
CUSTOM_CONFIG_DIR="${CUSTOM_BOARDS_DIR}/${BOARD_ARCH}/${BOARD}/configs/${CONFIG}"
if [[ -d "${CUSTOM_CONFIG_DIR}" ]]; then
    BOARD_CONFIG="${CUSTOM_CONFIG_DIR}"
    echo "Using out-of-tree board config: ${BOARD_CONFIG}"
else
    BOARD_CONFIG="${BOARD}:${CONFIG}"
    echo "Custom config not found, falling back to in-tree: ${BOARD_CONFIG}"
fi

CMAKE_ARGS=(
    -S "${NUTTX_DIR}"
    -B "${BUILD_DIR}"
    -DBOARD_CONFIG="${BOARD_CONFIG}"
    -DNUTTX_APPS_DIR="${APPS_DIR}"
    -G "Ninja"
)

echo "Project root : ${PROJECT_ROOT}"
echo "NuttX        : ${NUTTX_DIR}"
echo "nuttx-apps   : ${APPS_DIR}"
echo "Custom boards: ${CUSTOM_BOARDS_DIR}"
echo "Build dir    : ${BUILD_DIR}"
echo ""

echo "Running: cmake ${CMAKE_ARGS[*]}"
cmake "${CMAKE_ARGS[@]}"

echo ""
# Ensure symlink from nuttx-apps/external -> custom-apps exists
EXTERNAL_LINK="${APPS_DIR}/external"
if [[ ! -L "${EXTERNAL_LINK}" ]]; then
    echo "Creating symlink: nuttx-apps/external -> ../custom-apps"
    ln -s "../custom-apps" "${EXTERNAL_LINK}"
fi

echo "Done. Build with:"
echo "  ninja -C ${BUILD_DIR} "
