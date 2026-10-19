#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build_only=false
dfu_args=()

usage() {
    cat <<'EOF'
Usage: ./flash.sh [--build-only] [--serial SERIAL]

Build white panda CANnectivity firmware and flash it using STM32 ROM DFU.
Enter hardware DFU mode before flashing (BOOT0 high during reset/power-on).

  --build-only      Compile the firmware without accessing USB.
  --serial SERIAL   Select a particular STM32 DFU device.
  -h, --help        Show this help.

EOF
}

die() {
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

while (($#)); do
    case "$1" in
        --build-only) build_only=true; shift ;;
        --serial)
            (($# >= 2)) && [[ -n "$2" && "$2" != -* ]] || die "--serial needs a serial number"
            dfu_args+=(--serial "$2")
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) die "Unknown argument: $1 (see --help)" ;;
    esac
done

cd -- "$repo_dir"
if [[ ! -f .west/config ]]; then
    west init --local manifest
fi
[[ $(west topdir) == "$repo_dir" ]] || die "West workspace does not match this repository"

# Smart fetching reuses pinned commits when already present, allowing offline rebuilds.
west update --narrow --fetch smart
export ZEPHYR_BASE="$repo_dir/deps/zephyr"

build_dir="$repo_dir/build/white"
west build --board panda_white/stm32f413xx --build-dir "$build_dir" \
    --pristine=auto "$repo_dir/deps/cannectivity/app" -- \
    "-DBOARD_ROOT=$repo_dir" \
    "-DDTC_OVERLAY_FILE=$repo_dir/config/white.overlay" \
    "-DCONF_FILE=$repo_dir/config/white.conf"

image="$build_dir/zephyr/zephyr.bin"
printf 'Built: %s\n' "$image"
if "$build_only"; then
    exit 0
fi

# Match only STM32 ROM DFU and internal flash, never an arbitrary USB CAN device.
dfu_args+=(--device 0483:df11 --alt 0 --dfuse-address 0x08000000:leave --download "$image")
printf 'Flashing white panda through STM32 ROM DFU (0483:df11).\n'
exec dfu-util "${dfu_args[@]}"
