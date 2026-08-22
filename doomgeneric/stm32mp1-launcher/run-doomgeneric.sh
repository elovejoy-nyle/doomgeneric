#!/bin/sh
set -eu

DOOM_BIN="/usr/local/bin/doomgeneric"
DOOM_IWAD="${DOOM_IWAD:-/home/weston/freedoom1.wad}"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/1000}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-1}"

if [ ! -x "$DOOM_BIN" ]; then
  echo "Error: $DOOM_BIN not found or not executable."
  exit 1
fi

if [ ! -f "$DOOM_IWAD" ]; then
  echo "Error: IWAD not found: $DOOM_IWAD"
  echo "Set DOOM_IWAD env var or copy freedoom1.wad to /home/weston/."
  exit 1
fi

exec "$DOOM_BIN" -iwad "$DOOM_IWAD"
