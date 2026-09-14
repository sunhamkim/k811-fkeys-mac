#!/bin/zsh
set -euo pipefail

ROOT_DIR="${0:A:h}"
SRC="$ROOT_DIR/k811-fkeys-watch.c"
PLIST_SRC="$ROOT_DIR/com.local.k811-fkeys-watch.plist"
BIN="/usr/local/bin/k811-fkeys-watch"
LABEL="com.local.k811-fkeys-watch"
USER_UID="$(id -u)"
USER_AGENT="$HOME/Library/LaunchAgents/$LABEL.plist"
SYSTEM_PLIST="/Library/LaunchDaemons/$LABEL.plist"
TMP_BINARY=""

cleanup_tmp() {
  if [[ -n "${TMP_BINARY:-}" ]]; then
    rm -f "$TMP_BINARY"
  fi
}
trap cleanup_tmp EXIT

usage() {
  echo "Usage: $0 [install|uninstall]"
}

clean_old_install() {
  # Remove both installation styles used during development.
  launchctl bootout "gui/$USER_UID" "$USER_AGENT" 2>/dev/null || true
  sudo launchctl bootout system "$SYSTEM_PLIST" 2>/dev/null || true

  rm -f "$USER_AGENT"
  sudo rm -f "$SYSTEM_PLIST" "$BIN"

  rm -f /tmp/k811-fkeys-watch.log /tmp/k811-fkeys-watch.err.log
}

install_watch() {
  # Build first so a compiler failure does not destroy a working install.
  TMP_BINARY="$(mktemp -t k811-fkeys-watch)"

  cc "$SRC" -o "$TMP_BINARY" \
    -framework IOKit \
    -framework CoreFoundation

  # Only replace the current installation after a successful build.
  clean_old_install

  sudo mkdir -p /usr/local/bin
  sudo install -o root -g wheel -m 0755 "$TMP_BINARY" "$BIN"
  sudo install -o root -g wheel -m 0644 "$PLIST_SRC" "$SYSTEM_PLIST"

  sudo launchctl bootstrap system "$SYSTEM_PLIST"
  sudo launchctl enable "system/$LABEL"

  echo "Installed $LABEL as a root LaunchDaemon."
  echo
  echo "Grant Input Monitoring to this exact installed binary:"
  echo "  $BIN"
  echo
  echo "System Settings -> Privacy & Security -> Input Monitoring"
  echo "Remove any stale k811-fkeys-watch entry first."
  echo "Use +, then Cmd-Shift-G and enter /usr/local/bin if needed."
  echo
  echo "After granting permission, restart the daemon:"
  echo "  sudo launchctl kickstart -k system/$LABEL"
  echo
  echo "If Karabiner-Elements is enabled, configure its Expert setting"
  echo '"Delay before opening a device (ms)" before power-cycling the K811.'
  echo "5000 ms is a known-working starting value."
}

uninstall_watch() {
  clean_old_install
  echo "Removed $LABEL, its binary, older user-agent installs, and logs."
  echo "Privacy-list entries are not modified; remove k811-fkeys-watch"
  echo "from Input Monitoring manually if desired."
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
