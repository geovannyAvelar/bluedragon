# Bluedragon

Linux driver, command line tool and GTK4 settings window for the M711 gaming mouse (USB `04d9:fc30`).
Reverse engineered from the vendor's Windows configuration tool; no vendor code is included.

| Module | Path | Description |
|---|---|---|
| libbluedragon | `lib/` | C library: DPI, polling rate, buttons, LED, raw memory access over hidraw |
| bluedragon | `cli/` | Command line tool built on libbluedragon |
| bluedragon-gui | `gui/` | GTK4 front end: composite templates (`data/ui/*.ui`) in a GResource, one class per widget (built when GTK 4.10+ development files are found) |
| tests | `tests/` | Unit tests against a simulated device, no hardware needed |

## Dependencies

Tested on Ubuntu 24.04. Package names are Debian/Ubuntu ones.

**Using it (runtime)**

| What | Needs |
|---|---|
| `bluedragon` command line tool, `libbluedragon` | nothing beyond libc |
| `bluedragon-gui` | `libgtk-4-1` (GTK 4.10 or newer) and `librsvg2-common` (the SVG loader that draws the mouse picture; without it a drawn mouse is used instead) |
| Access to the mouse | read/write permission on its hidraw node; the udev rule in the `bluedragon` package (or the one under "Usage") grants it |

```bash
sudo apt install libgtk-4-1 librsvg2-common
```

**Building from source**

| What | Package | Notes |
|---|---|---|
| C compiler | `gcc` | C11 |
| Build tool | `cmake` (3.21+ for the presets), `ninja-build` | the presets always use the Ninja generator |
| GTK 4 headers | `libgtk-4-dev` (4.10+), `libglib2.0-dev-bin` | `libglib2.0-dev-bin` provides `glib-compile-resources`; both are pulled in by `libgtk-4-dev`, and `pkg-config` finds GTK. Without GTK 4.10+ the GUI is skipped and the rest still builds (Ubuntu 22.04 has GTK 4.6, so no GUI there) |
| Debian packages | `dpkg-dev` | only for `--target package`; `lintian` is optional for checking them |

```bash
sudo apt install build-essential cmake ninja-build pkg-config libgtk-4-dev librsvg2-common
# only to build the .deb packages:
sudo apt install dpkg-dev lintian fakeroot
```

## Build

```bash
cmake --preset default            # Ninja generator, Release, in build/
cmake --build --preset default
ctest --preset default
```

Presets: `default`, `debug`, `asan` (ASan + UBSan), `deb` (installs under `/usr`, for the packages).

## Debian packages

```bash
cmake --preset deb && cmake --build --preset deb --target package
ls build-deb/packages/
sudo apt install ./build-deb/packages/*.deb
```

| Package | Contents |
|---|---|
| `libbluedragon0` | shared library |
| `libbluedragon-dev` | header, `libbluedragon.so` symlink, CMake package |
| `bluedragon` | command line tool, man page, udev rule (mouse access for the logged-in user) |
| `bluedragon-gui` | GTK4 settings window, desktop entry, icon, man page |

### APT repository (Ubuntu 24.04 and newer)

Every release and every build of `main` is also published to a signed APT repository on GitHub Pages
(amd64 and arm64). The `arch=` option avoids apt's "doesn't support architecture 'i386'" notice on multiarch systems:

```bash
sudo curl -fsSLo /usr/share/keyrings/bluedragon.gpg https://geovannyavelar.github.io/bluedragon/pubkey.gpg
echo "deb [arch=amd64,arm64 signed-by=/usr/share/keyrings/bluedragon.gpg] https://geovannyavelar.github.io/bluedragon stable main" \
  | sudo tee /etc/apt/sources.list.d/bluedragon.list
sudo apt update
sudo apt install bluedragon-gui      # or just: bluedragon
```

`stable` holds every tagged release, so it stays empty (`Unable to locate package`) until the first `v*` tag is
pushed; until then use `unstable`. `unstable` holds only the latest build of `main` (no history, may be
broken); use it instead of `stable`, not together with it. The signing key fingerprint is
`76D9 B403 F116 C55E 7B41 0F34 A911 599B E993 D01C`.

### Releases

GitHub Actions (`.github/workflows/release.yml`) builds and tests the packages for amd64 and arm64:

- every push to `main` replaces the **unstable** pre-release (tag `unstable`) with a fresh build, versioned
  `<project version>~unstable.<date>.<sha>`;
- pushing a tag such as `v0.0.1` publishes a regular release with that version
  (`git tag v0.0.1 && git push origin v0.0.1`); a tag with a suffix such as `v1.0.0-rc1` is marked pre-release;
- pull requests are built and tested only;
- after each release the APT repository is rebuilt from the Releases page and redeployed to GitHub Pages
  (`.github/workflows/apt-repo.yml`, `packaging/apt/build-repo.sh`).

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

## Credits

The mouse picture in the GUI is `Crispy-Computer-mouse-top-down-view.svg` from
[Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Crispy-Computer-mouse-top-down-view.svg), released under
CC0 1.0 (see `gui/data/images/ATTRIBUTION.md`). It needs the SVG loader of gdk-pixbuf (`librsvg2-common`) at runtime;
without it the GUI falls back to a drawn mouse.
