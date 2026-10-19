# panda-socketcan

CANnectivity firmware for comma.ai panda adapters, exposing three CAN channels
through Linux's existing `gs_usb` driver.

| Variant | MCU | CAN support | Flash command |
| --- | --- | --- | --- |
| White | STM32F413 | Classic CAN | `./flash.sh --white` (default) |
| Black | STM32F413 | Classic CAN, harness detection | `./flash.sh --black` |
| Red | STM32H725 | Classic CAN and CAN FD, harness detection | `./flash.sh --red` |

## Build and flash

With Nix installed and the panda in **STM32 ROM DFU mode**, enter the
development shell and run the script from this repository:

```sh
nix develop
./flash.sh
```

Select `--black` or `--red` for those boards. The variant is selected explicitly;
STM32 ROM DFU does not identify the panda board. Select only one variant per run.

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
./flash.sh --black --build-only
./flash.sh --red --build-only
```

The output is `build/<variant>/zephyr/zephyr.bin` (and `zephyr.elf` for debugging).
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

`manifest/west.yml` defines these dependencies. Boards live under
`boards/comma/panda_{white,black,red}/`, with shared F4 hardware in
`boards/comma/common/`. `config/common.conf` holds shared application settings;
each variant has its own `.conf` and `.overlay`. `config/channels.dtsi` defines
the three logical USB CAN channels.

`app/` supplies a small entry point and black/red harness support while reusing
upstream CANnectivity's USB protocol, USB descriptors, LEDs and timestamps.
The dependency checkouts remain unmodified. GitHub Actions builds all three variants.

All three targets compile with the pinned dependencies. Hardware operation has
not yet been tested.

## Linux usage

This private compatible build uses the established gs_usb USB ID `1d50:606f`,
so kernels with `CONFIG_CAN_GS_USB` can bind without custom modules or a new-ID
udev rule. The USB product string identifies the selected panda variant.
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

For CAN FD on red panda, configure both nominal and data bitrates:

```sh
sudo ip link set can0 up type can bitrate 500000 dbitrate 2000000 fd on
candump can0
```

Black and red enumerate on USB only after detecting a harness. Connect it before
configuring CAN interfaces. See the harness behavior below.

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

## Black and red panda hardware

These targets use the same [upstream hardware reference](https://github.com/commaai/panda/tree/f849893bdcfc6cd05d4547d64d2802b5a83f409c),
including `board/boards/{black,red}.h`, `board/drivers/harness.h`, and
`board/stm32h7/{clock,peripherals,llfdcan_declarations}.h`.

| Board | Physical controller | RX | TX | Standby (high disables) |
| --- | --- | --- | --- | --- |
| Black | CAN1 | PB8 AF8 | PB9 AF8 | PC1 |
| Black | CAN2, normal | PB5 AF9 | PB6 AF9 | PC13 |
| Black | CAN2, flipped | PB12 AF9 | PB13 AF9 | PB10 |
| Black | CAN3 | PA8 AF11 | PA15 AF11 | PA0 |
| Red | FDCAN1 | PB8 AF9 | PB9 AF9 | PG11 |
| Red | FDCAN2, normal | PB5 AF9 | PB6 AF9 | PB3 |
| Red | FDCAN2, flipped | PB12 AF9 | PB13 AF9 | PB4 |
| Red | FDCAN3 | PG10 AF2 | PG9 AF2 | PD7 |

Black shares white's 16 MHz crystal, 96 MHz CPU clock and 1 MHz TIM2 timestamps.
Its GPS is held off and the USB load switch is enabled. Red uses a 25 MHz crystal,
240 MHz CPU, 80 MHz FDCAN kernel clock, and HSI48 for full-speed USB. TIM2 also
runs at 1 MHz. Each FDCAN controller gets a separate region of the shared 10 KiB
message RAM, with 64-byte payloads and data bitrates up to 5 Mbit/s. Red targets
the STM32H725 with direct SMPS supply.

### Harness connection and orientation

Black reads SBU1/SBU2 on PC0/PC3 (ADC1 channels 10/13). Red reads PC4/PA1
(ADC1 channels 4/17). As in panda's factory detection, both voltages at or above
half of AVDD mean no harness; otherwise, SBU1 below SBU2 means flipped.
The pins return to digital inputs immediately after each ADC measurement.

The adapter waits for three matching readings, 100 ms apart, before selecting
an orientation and enumerating on USB. In normal orientation logical buses 0/1/2
use physical controllers 1/2/3. When flipped, they use 3/2/1, with CAN2 routed
through PB12/PB13 and the fourth transceiver. This keeps Linux's logical bus
mapping consistent in either orientation. LEDs indicate logical channels 0/1/2
as green/blue/red.

Orientation stays fixed for that connection. Three consecutive absent readings
disconnect USB and stop all CAN channels. The MCU then resets to clear the old
channel registration and waits for the next harness connection. Reconnecting in
either orientation enumerates a fresh USB adapter; bring its CAN interfaces up
again. Linux interface names may differ after reconnecting or with other adapters
present. Orientation changes alone do not remap channels or restart USB.

Both harness relay controls remain released in open-drain mode, preserving the
factory passthrough connection. This firmware does not intercept, forward or
apply openpilot safety rules to traffic. Termination is external.
