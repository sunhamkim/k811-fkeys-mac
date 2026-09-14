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
  # Start from a known-clean installation state.
  clean_old_install

  TMP_BINARY="$(mktemp -t k811-fkeys-watch)"

  cc "$SRC" -o "$TMP_BINARY" \
    -framework IOKit \
    -framework CoreFoundation

  sudo mkdir -p /usr/local/bin

  # Important: this is a normal root-owned executable, not setuid.
  # The LaunchDaemon itself runs it as root.
  sudo install -o root -g wheel -m 0755 "$TMP_BINARY" "$BIN"
  sudo install -o root -g wheel -m 0644 "$PLIST_SRC" "$SYSTEM_PLIST"

  sudo launchctl bootstrap system "$SYSTEM_PLIST"
  sudo launchctl enable "system/$LABEL"

  echo "Installed $LABEL as a root LaunchDaemon."
  echo
  echo "Do NOT rebuild or reinstall the binary before testing."
  echo
  echo "Now remove any stale k811-fkeys-watch entry from Input Monitoring,"
  echo "then add this exact installed binary and enable it:"
  echo "  $BIN"
  echo
  echo "System Settings -> Privacy & Security -> Input Monitoring"
  echo "Use +, then Cmd-Shift-G and enter /usr/local/bin if needed."
  echo
  echo "After granting Input Monitoring, restart the daemon:"
  echo "  sudo launchctl kickstart -k system/$LABEL"
  echo
  echo "Then power-cycle the K811 while Karabiner's open delay is 5000 ms."
}

uninstall_watch() {
  clean_old_install
  echo "Removed $LABEL user agent, system daemon, binary, and logs."
  echo "TCC entries are not modified; remove k811-fkeys-watch manually from"
  echo "Input Monitoring/Accessibility if you want a completely clean UI state."
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
