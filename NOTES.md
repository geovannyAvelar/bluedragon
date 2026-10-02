# M711 gaming mouse (04d9:fc30) config protocol — from hid.exe / HIDApi.dll / HidDevice.dll

Interface 2 (hidraw, vendor page 0xFFA0). Feature reports only, no scrambling on this path
(EncodeSecrecy_V11 in HIDApi.dll is only used by the legacy 9-byte input/output path; hid.exe
config traffic goes through HidDevice_SetFeature/GetFeature = plain HidD_Set/GetFeature).

Memory window: report 2 (16 B, 7 data) or report 3 (64 B, up to 32 data)
  [rid, op, addr_lo, addr_hi, len, 0, 0, 0, data...]   op f2 = read, f3 = write
  read = SET_FEATURE request, then GET_FEATURE; data at [8:]
Commit/apply: write [02 f1 02 <flag>]  flag 1=active profile, 2=DPI, 8=polling/etc, 0x10=...

Memory map (verified read-only on device):
  0x20..0x2b  5x u16 per-profile value (+1 extra)   (written via report 3 @0x20)
  0x2c        active profile
  0x32..0x3b  polling rate, 1 byte per profile at 0x32+2*p (raw 1 = 1000 Hz confirmed; commit flag 8)
  profile bases 0x040 0x100 0x1b0 0x260 0x310
    +0   header (05 00 ..)
    +4   5 DPI levels x 6 bytes  [b0, dpi_code, flag, 0, ..]; DPI = step*100, code = table[step] (hid.exe 0x480494)
    +0x42 12 buttons x 4 bytes   [action, ?, arg, ?]
  field semantics for 0x20/0x32 words (polling rate / lift / angle?) NOT yet confirmed.
Buttons: 4-byte entry at base+0x42+4*n.  Apply flag 4.  Programming bracket: [02 f5 00] ... [02 f5 01] (100 ms sleeps).
  81/82/83 left/right/middle, 84/85 button4/5, 90 00 <usage> 00 = single key (HID usage, e0..e7 modifiers),
  8f <mods> <usage> 00 = key combo (mods ctrl1 shift2 alt4 win8), 8e 01 <usage> 00 = consumer (media),
  91 = macro file, 99 = fire key, 00 00 00 00 = none; 88 89 8a 8b 8c 8d 97 98 9b04 = device functions (unnamed).
LED: 7-byte record per profile at 0x449+8*p: [R G B type value sub level].  Written as 6 bytes + 1 byte,
  then commits 0x10,4,1,2,8 (hid.exe SendAll order; which flag latches the LED is unknown).
  mode (type,sub): 0=(1,4) 1=(1,8) 2=(1,2) 3=(2,0) 4=(6,0) 5=(7,0) 6=(1,0x10) 7=(0,0); read path in hid.exe disagrees for 3,4,6.
Not yet decoded: names of device functions, macros, fire key, LED effect names, report 4/5/6.

## Layout
  lib/include/bluedragon.h, lib/src/bluedragon.c   libbluedragon (shared by default, -DBUILD_SHARED_LIBS=OFF for static)
  cli/main.c                           bluedragon (CLI), links bluedragon::bluedragon
  gui/main.c                           bluedragon-gui, GTK4 front end (built only if libgtk-4-dev >= 4.10 is found)
  tests/test_bluedragon.c                    unit tests against a simulated device

## Build
  cmake --preset default && cmake --build --preset default     # Ninja, Release, build/
  ctest --preset default
  cmake --preset asan && cmake --build --preset asan && ctest --preset asan   # ASan + UBSan
Always Ninja: generator is set in CMakePresets.json (presets: default, debug, asan).
  cmake --install build --prefix /usr/local        # lib, header, bluedragon, CMake package (find_package(bluedragon))
Options: BUILD_SHARED_LIBS, M711_SANITIZE, M711_BUILD_CLI, M711_BUILD_TESTS.
Use from C: #include <bluedragon.h>, link bluedragon::bluedragon (or -lbluedragon).  Exported symbols are only the m711_* API
(the m711_ prefix names the mouse model).

## Tests
  Run against a simulated device (m711_open_custom with fake feature-report callbacks); no hardware needed.
