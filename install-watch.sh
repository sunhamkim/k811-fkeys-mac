#!/bin/zsh
set -euo pipefail

ROOT_DIR="${0:A:h}"
SRC="$ROOT_DIR/k811-fkeys-watch.c"
PLIST_SRC="$ROOT_DIR/com.local.k811-fkeys-watch.plist"
BIN="/usr/local/bin/k811-fkeys-watch"
LABEL="com.local.k811-fkeys-watch"
USER_UID="$(id -u)"
AGENT_DIR="$HOME/Library/LaunchAgents"
PLIST_DST="$AGENT_DIR/$LABEL.plist"
OLD_SYSTEM_PLIST="/Library/LaunchDaemons/$LABEL.plist"
TMP_BINARY=""

cleanup() {
  if [[ -n "${TMP_BINARY:-}" ]]; then
    rm -f "$TMP_BINARY"
  fi
}
trap cleanup EXIT

usage() {
  echo "Usage: $0 [install|uninstall]"
}

install_watch() {
  TMP_BINARY="$(mktemp -t k811-fkeys-watch)"

  cc "$SRC" -o "$TMP_BINARY" \
    -framework IOKit \
    -framework CoreFoundation

  sudo mkdir -p /usr/local/bin
  sudo install -o root -g wheel -m 0755 "$TMP_BINARY" "$BIN"

  # Remove the earlier system LaunchDaemon, if present.
  sudo launchctl bootout system "$OLD_SYSTEM_PLIST" 2>/dev/null || true
  sudo rm -f "$OLD_SYSTEM_PLIST"

  mkdir -p "$AGENT_DIR"
  install -m 0644 "$PLIST_SRC" "$PLIST_DST"

  launchctl bootout "gui/$USER_UID" "$PLIST_DST" 2>/dev/null || true
  launchctl bootstrap "gui/$USER_UID" "$PLIST_DST"
  launchctl enable "gui/$USER_UID/$LABEL"

  echo "Installed $LABEL as a user LaunchAgent."
  echo
  echo "IMPORTANT: grant Input Monitoring permission to:"
  echo "  $BIN"
  echo
  echo "System Settings -> Privacy & Security -> Input Monitoring"
  echo "Use + and press Cmd-Shift-G if needed to enter /usr/local/bin."
  echo
  echo "After granting permission, run:"
  echo "  launchctl kickstart -k gui/$USER_UID/$LABEL"
  echo
  echo "Then power-cycle the K811 while Karabiner's open delay is enabled."
}

uninstall_watch() {
  launchctl bootout "gui/$USER_UID" "$PLIST_DST" 2>/dev/null || true
  rm -f "$PLIST_DST"

  sudo launchctl bootout system "$OLD_SYSTEM_PLIST" 2>/dev/null || true
  sudo rm -f "$OLD_SYSTEM_PLIST" "$BIN"

  rm -f /tmp/k811-fkeys-watch.log /tmp/k811-fkeys-watch.err.log

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
