# DoomGeneric on STM32MP1 (Wayland SHM backend)

This fork targets **STM32MP1 / OpenSTLinux** with a minimal Wayland **SHM** backend (no SDL, no EGL, no OpenGL).

## Goal

Run Doom on Weston/Wayland with a 480x272 display by pushing a shared-memory framebuffer (`wl_shm`) to the compositor.

## Prerequisites

### Build machine (WSL/Linux)

- ARM cross toolchain: `arm-linux-gnueabihf-gcc`
- STM32 sysroot available (example: `$HOME/stm32-sysroot`)
- `make`, `scp`

### STM32MP1 target

- OpenSTLinux
- Weston running in user session `weston`
- Active Wayland socket (often `wayland-1`)
- A WAD file (Freedoom or Doom)

## Build (cross-compilation)

From the project's `doomgeneric/` directory:

```bash
make -f Makefile.wayland_shm \
  CC=arm-linux-gnueabihf-gcc \
  CFLAGS="-DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -I$HOME/stm32-sysroot/usr/include -I$HOME/stm32-sysroot/usr/include/arm-linux-gnueabihf" \
  LDFLAGS="-L$HOME/stm32-sysroot/usr/lib/arm-linux-gnueabihf -Wl,-rpath-link,$HOME/stm32-sysroot/usr/lib/arm-linux-gnueabihf" \
  LIBS="-lwayland-client -lffi"
```

The generated binary is `doomgeneric`.

## Copy to the board

Example:

```bash
scp doomgeneric weston@172.19.165.127:/tmp/
```

## Install Freedoom (IWAD)

### Option A (recommended): install from OpenSTLinux packages

On the board:

```bash
su -
opkg update
opkg install freedoom
```

Then locate installed IWAD files (path may vary by image):

```bash
find / -type f \( -name "freedoom*.wad" -o -name "doom*.wad" \) 2>/dev/null
```

### Option B: copy a WAD from your PC

On your dev machine, download a Freedoom IWAD (Phase 1, e.g. `freedoom1.wad`) from the official project:

- https://freedoom.github.io/

Then copy it to the board:

```bash
scp freedoom1.wad weston@<BOARD_IP>:/home/weston/
```

## Run on STM32MP1

Log in as `weston` user (or switch to it), then:

```bash
su - weston
export XDG_RUNTIME_DIR=/run/user/1000
export WAYLAND_DISPLAY=wayland-1
cd /tmp
./doomgeneric -iwad /home/weston/freedoom1.wad
```

If everything is OK, you should see logs like:

- `Scaling: Doom 320x200 -> 435x272, offset 22,0`
- `Wayland SHM init OK: 480x272`

## Useful checks

Wayland active:

```bash
find /run/user -name "wayland-*"
```

Weston running:

```bash
ps aux | grep weston
```

Weston logs:

```bash
tail -f /home/weston/weston.log
```

## Troubleshooting

- Black screen at startup:
  - Check `XDG_RUNTIME_DIR` and `WAYLAND_DISPLAY`
  - Verify socket name (`wayland-1` vs `wayland-0`)
- Linker errors during build (`-lwayland-client`, `-lffi`):
  - Check `LDFLAGS` and sysroot paths
- SDL/EGL rendering issues on some setups:
  - This SHM backend does not use EGL or OpenGL

## Notes

- Recommended logical Doom resolution: `320x200`
- Target window: `480x272` with nearest-neighbor upscaling
- This port is intentionally minimal and focused on Weston robustness
