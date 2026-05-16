# DoomGeneric on STM32MP1 (Wayland SHM backend)

This fork targets **STM32MP1 / OpenSTLinux** with a minimal Wayland **SHM** backend (no SDL, no EGL, no OpenGL).

## Goal

Run Doom on Weston/Wayland with a 480x272 display by pushing a shared-memory framebuffer (`wl_shm`) to the compositor.

## Prerequisites

### Build machine (WSL/Linux)

- ARM cross toolchain: `arm-linux-gnueabihf-gcc`
- ARM sysroot in `$HOME/stm32-sysroot`
- `make`, `scp`, `ssh`

### STM32MP1 target

- OpenSTLinux
- Weston running in user session `weston`
- Active Wayland socket (often `wayland-1`)
- A WAD file (Freedoom or Doom)

## Populate `~/stm32-sysroot` from Debian ARM packages

Download ARM `.deb` packages and extract them directly into the sysroot.

### 1) Prepare

```bash
mkdir -p "$HOME/stm32-sysroot" "$HOME/armhf-debs"
cd "$HOME/armhf-debs"

sudo dpkg --add-architecture armhf
sudo apt update
```

### 2) Download packages for the Wayland SHM backend (minimal)

```bash
apt download \
  libc6:armhf libc6-dev:armhf \
  libwayland-client0:armhf libwayland-dev:armhf \
  libffi8:armhf libffi-dev:armhf
```

### 3) Extract packages into `~/stm32-sysroot`

```bash
for deb in *.deb; do
  dpkg-deb -x "$deb" "$HOME/stm32-sysroot"
done
```

### 4) Validate

```bash
ls "$HOME/stm32-sysroot/usr/include/wayland-client.h"
ls "$HOME/stm32-sysroot/usr/lib/arm-linux-gnueabihf/libwayland-client.so" \
   "$HOME/stm32-sysroot/usr/lib/arm-linux-gnueabihf/libffi.so"
```


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

On your dev machine, download a Freedoom IWAD (Phase 1, e.g. `freedoom1.wad`) from the official project:

- https://freedoom.github.io/

Then copy it to the board:

```bash
scp freedoom1.wad weston@<BOARD_IP>:/home/weston/
```

## Run on STM32MP1 (manual)

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

## Install in the ST graphical launcher

ST images often use `/usr/local/demo/demo_launcher.py` with YAML app entries in `/usr/local/demo/application/`.

Helper files included in this repo:

- `doomgeneric/stm32mp1-launcher/run-doomgeneric.sh`
- `doomgeneric/stm32mp1-launcher/doomgeneric.desktop`
- `doomgeneric/stm32mp1-launcher/install-launcher-stm32mp1.sh`

Manual installation on the board (as `root`):

```bash
# 1) Install binary and user launcher script
install -m 0755 /tmp/doomgeneric /home/weston/.local/bin/doomgeneric
cat > /home/weston/.local/bin/run-doomgeneric.sh <<'EOS'
#!/bin/sh
set -eu
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/1000}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"
exec /home/weston/.local/bin/doomgeneric -iwad /home/weston/freedoom1.wad
EOS
chmod 0755 /home/weston/.local/bin/run-doomgeneric.sh
chown -R weston:weston /home/weston/.local

# 2) Add ST launcher wrapper and YAML entry
mkdir -p /usr/local/demo/application/doomgeneric/bin
cat > /usr/local/demo/application/doomgeneric/bin/launch_doomgeneric.sh <<'EOS'
#!/bin/sh
exec /home/weston/.local/bin/run-doomgeneric.sh
EOS
chmod 0755 /usr/local/demo/application/doomgeneric/bin/launch_doomgeneric.sh

cat > /usr/local/demo/application/050-doomgeneric.yaml <<'EOS'
Application:
    Name: DoomGeneric
    Description: Doom on Wayland SHM
    Icon: application/video/pictures/Video_playback_logo.png
    Type: script
    Board:
        List: all
    Script:
        Start: application/doomgeneric/bin/launch_doomgeneric.sh
    Action:
        button_release_event: script_start
        button_press_event: highlight_eventBox
EOS
```

Restart only the ST launcher process:

```bash
pkill -f /usr/local/demo/demo_launcher.py
su -l weston -c 'export XDG_RUNTIME_DIR=/run/user/1000; export WAYLAND_DISPLAY=wayland-1; nohup /usr/local/demo/demo_launcher.py >/home/weston/demo_launcher.log 2>&1 &'
```

If the icon is not immediately visible, change page in the launcher (icons are paginated).

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
- ST menu does not show `.desktop` launchers:
  - Use the ST YAML method above (`/usr/local/demo/application/*.yaml`)

## Notes

- Recommended logical Doom resolution: `320x200`
- Target window: `480x272` with nearest-neighbor upscaling
- This port is intentionally minimal and focused on Weston robustness
