# bluedragon

Linux driver, command line tool and GTK4 settings window for the M711 gaming mouse (USB `04d9:fc30`).
Reverse engineered from the vendor's Windows configuration tool; no vendor code is included.

| Module | Path | Description |
|---|---|---|
| libbluedragon | `lib/` | C library: DPI, polling rate, buttons, LED, raw memory access over hidraw |
| bluedragon | `cli/` | Command line tool built on libbluedragon |
| bluedragon-gui | `gui/` | GTK4 front end (built when GTK 4.10+ development files are found) |
| tests | `tests/` | Unit tests against a simulated device, no hardware needed |

## Build

```bash
cmake --preset default            # Ninja generator, Release, in build/
cmake --build --preset default
ctest --preset default
```

Builds use Ninja (`sudo apt install ninja-build`). Presets: `default`, `debug`, `asan` (ASan + UBSan).
GUI needs `libgtk-4-dev`.

## Usage

```bash
./build/cli/bluedragon dump
./build/cli/bluedragon dpi 1 1 1600
./build/cli/bluedragon polling 1 500
./build/cli/bluedragon button 1 3 key:ctrl+c
./build/cli/bluedragon led 1 color 00ff00
./build/gui/bluedragon-gui
```

The config interface is a hidraw node (USB interface 2). Your user needs read/write access to it,
for example through a udev rule:

```
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="04d9", ATTRS{idProduct}=="fc30", TAG+="uaccess"
```

## Status

Works on the author's mouse: reading all settings, DPI, profile selection. Writes follow the vendor tool's
sequence but several details are unverified (250/125 Hz polling values, LED effect names, key combos,
device-function buttons, macros). See `NOTES.md` for the protocol and what is still unknown.
Use at your own risk; this is not affiliated with the mouse vendor.

## License

LGPL-3.0-or-later, see `COPYING.LESSER` and `COPYING`.
