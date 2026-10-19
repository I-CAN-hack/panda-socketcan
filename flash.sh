#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build_only=false
variant=white
variant_selected=false
dfu_args=()

usage() {
    cat <<'EOF'
Usage: ./flash.sh [--white|--black|--red] [--build-only] [--serial SERIAL]

Build panda CANnectivity firmware and flash it using STM32 ROM DFU.
Enter hardware DFU mode before flashing (BOOT0 high during reset/power-on).

  --white           White panda, STM32F413 (default).
  --black           Black panda, STM32F413 with harness orientation detection.
  --red             Red panda, STM32H725 with CAN FD and harness detection.
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
        --white|--black|--red)
            if "$variant_selected"; then
                die "Select only one panda variant"
            fi
            variant=${1#--}
            variant_selected=true
            shift
            ;;
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

case "$variant" in
    white|black) soc=stm32f413xx ;;
    red) soc=stm32h725xx ;;
esac

cd -- "$repo_dir"
if [[ ! -f .west/config ]]; then
    west init --local manifest
fi
[[ $(west topdir) == "$repo_dir" ]] || die "West workspace does not match this repository"

# Smart fetching reuses pinned commits when already present, allowing offline rebuilds.
west update --narrow --fetch smart
export ZEPHYR_BASE="$repo_dir/deps/zephyr"

build_dir="$repo_dir/build/$variant"
west build --board "panda_$variant/$soc" --build-dir "$build_dir" \
    --pristine=auto "$repo_dir/app" -- \
    "-DBOARD_ROOT=$repo_dir" \
    "-DDTS_ROOT=$repo_dir" \
    "-DDTC_OVERLAY_FILE=$repo_dir/config/$variant.overlay" \
    "-DCONF_FILE=$repo_dir/config/common.conf;$repo_dir/config/$variant.conf"

image="$build_dir/zephyr/zephyr.bin"
printf 'Built: %s\n' "$image"
if "$build_only"; then
    exit 0
fi

# Match only STM32 ROM DFU and internal flash, never an arbitrary USB CAN device.
dfu_args+=(--device 0483:df11 --alt 0 --dfuse-address 0x08000000:leave --download "$image")
printf 'Flashing %s panda through STM32 ROM DFU (0483:df11).\n' "$variant"
exec dfu-util "${dfu_args[@]}"
