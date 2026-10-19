# panda-socketcan

CANnectivity firmware for the **white panda (STM32F413)**, exposing three
independent classic CAN channels through Linux's existing `gs_usb` driver.
Red panda / STM32H7 CAN FD support is planned; this target is for white panda only.

## Build and flash

With Nix installed and the white panda in **STM32 ROM DFU mode**, enter the
development shell and run the script from this repository:

```sh
nix develop
./flash.sh
```

The script downloads the pinned upstream sources, compiles the firmware, and
flashes internal flash at **0x08000000** with `dfu-util`. It replaces the factory panda
bootstub and firmware. There is no application bootloader or firmware update
protocol; subsequent flashing also uses hardware ROM DFU.

Enter ROM DFU by holding **BOOT0 high during reset or power-on**, using the
board's recovery pads or a panda paw. It should enumerate as `0483:df11`
(`STM32 BOOTLOADER`). Return BOOT0 to its normal state for regular boot. You may
need to reset or reconnect the device after flashing.

Build and flashing run as your normal user, using your existing DFU udev rules.

For multiple connected DFU devices, select one explicitly:

```sh
./flash.sh --serial 0123456789AB
```

Build without accessing hardware:

```sh
./flash.sh --build-only
```

The output is `build/white/zephyr/zephyr.bin` (and `zephyr.elf` for debugging).
The first run needs network access for Nix packages and source checkouts; later
builds reuse the pinned dependencies. Generated files stay in `.west/`, `deps/`
and `build/` and are ignored by Git. Do not edit the upstream checkouts: `west
update` manages them.

## Development

```sh
nix develop
./flash.sh --build-only
```

The flake provides GNU Arm Embedded GCC/binutils/GDB, Zephyr's Python build
dependencies and `west`, CMake, Ninja, dtc, dfu-util, USB utilities, can-utils,
iproute2, ShellCheck and nixfmt. No separate Zephyr SDK or pip installation is
needed. The source versions are CANnectivity **v1.4.0** and Zephyr **v4.4.0**;
Zephyr pins the CMSIS and STM32 HAL revisions through its imported manifest.

`manifest/west.yml` defines these dependencies. The board lives under
`boards/comma/panda_white/`; `config/white.overlay` defines the USB CAN channels
and timestamp timer, and `config/white.conf` supplies the application settings.
The upstream CANnectivity application and USB protocol implementation remain
unmodified, allowing a later red panda board target to share them.

The white panda target compiles with the pinned dependencies. Hardware operation
has not yet been tested; run the bench checks below before relying on it.

## Linux usage

This private compatible build uses the established gs_usb USB ID `1d50:606f`,
so kernels with `CONFIG_CAN_GS_USB` can bind without custom modules or a new-ID
udev rule. The USB strings identify it as `White Panda CANnectivity`.
It does not use panda's factory USB protocol.

With no other CAN interfaces present, the three channels normally appear as
`can0`, `can1`, and `can2`. Names can differ if other adapters are attached.

```sh
sudo ip link set can0 up type can bitrate 500000
candump can0
```

Configure each channel separately. For a passive capture:

```sh
sudo ip link set can0 down
sudo ip link set can0 up type can bitrate 500000 listen-only on
```

## White panda hardware

The hardware reference is
[commaai/panda at f849893b](https://github.com/commaai/panda/tree/f849893bdcfc6cd05d4547d64d2802b5a83f409c),
the parent of the commit removing white/black panda support. In particular:
`board/boards/white.h`, `board/stm32f4/peripherals.h`, and
`board/stm32f4/clock.h`.

| gs_usb channel / panda bus | Controller | RX | TX | Standby (high disables) | State LED |
| --- | --- | --- | --- | --- | --- |
| 0 | CAN1 | PB8 AF8 | PB9 AF8 | PC1 | Green |
| 1 | CAN2 | PB5 AF9 | PB6 AF9 | PC13 | Blue |
| 2 | CAN3 | PA8 AF11 | PA15 AF11 | PA0 | Red |

The board uses the 1 MiB flash / 320 KiB SRAM F413 configuration. A 16 MHz HSE
crystal drives the PLL at 96 MHz, with APB1 and APB2 at 48 MHz and USB at 48 MHz.
TIM2 runs at 1 MHz for CANnectivity's timestamps.

Transceivers stay in standby until the host starts their CAN channel. USB stays
in client mode (PA13 high, PB2 low), including when powered from the OBD connector.
The ESP is held off, GMLAN stays asleep, and unused K/L-line TX pins stay at UART
idle. K-line, L-line, GMLAN, Wi-Fi and openpilot's safety/forwarding features are
outside this adapter's scope. CAN termination must be provided on the bench bus.

PA13 and PA14 serve board controls, as in the factory firmware, and overlap the
SWD pins. Hardware ROM DFU is the intended flashing/recovery route.
