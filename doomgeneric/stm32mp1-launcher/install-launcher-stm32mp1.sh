#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
  echo "Please run as root (su -)."
  exit 1
fi

PREFIX="/usr/local/bin"
APPDIR="/usr/share/applications"
SRC_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

install -m 0755 "$SRC_DIR/run-doomgeneric.sh" "$PREFIX/run-doomgeneric.sh"
install -m 0644 "$SRC_DIR/doomgeneric.desktop" "$APPDIR/doomgeneric.desktop"

echo "Launcher installed: $APPDIR/doomgeneric.desktop"
echo "Runtime script installed: $PREFIX/run-doomgeneric.sh"
echo "Make sure /usr/local/bin/doomgeneric exists and your IWAD path is valid."
