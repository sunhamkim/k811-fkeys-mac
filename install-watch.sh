#!/bin/zsh
set -euo pipefail

ROOT_DIR="${0:A:h}"
SRC="$ROOT_DIR/k811-fkeys-watch.c"
PLIST_SRC="$ROOT_DIR/com.local.k811-fkeys-watch.plist"
BIN="/usr/local/bin/k811-fkeys-watch"
PLIST_DST="/Library/LaunchDaemons/com.local.k811-fkeys-watch.plist"
LABEL="com.local.k811-fkeys-watch"

usage() {
  echo "Usage: $0 [install|uninstall]"
}

install_watch() {
  local tmp
  tmp="$(mktemp -t k811-fkeys-watch)"
  trap 'rm -f "$tmp"' EXIT

  cc "$SRC" -o "$tmp" \
    -framework IOKit \
    -framework CoreFoundation

  sudo mkdir -p /usr/local/bin
  sudo install -o root -g wheel -m 0755 "$tmp" "$BIN"
  sudo install -o root -g wheel -m 0644 "$PLIST_SRC" "$PLIST_DST"

  sudo launchctl bootout system "$PLIST_DST" 2>/dev/null || true
  sudo launchctl bootstrap system "$PLIST_DST"
  sudo launchctl enable "system/$LABEL"

  echo "Installed $LABEL"
  echo "Power-cycle the K811 to test it."
}

uninstall_watch() {
  sudo launchctl bootout system "$PLIST_DST" 2>/dev/null || true
  sudo rm -f "$PLIST_DST" "$BIN"
  echo "Removed $LABEL"
}

case "${1:-install}" in
  install)
    install_watch
    ;;
  uninstall)
    uninstall_watch
    ;;
  *)
    usage
    exit 2
    ;;
esac
