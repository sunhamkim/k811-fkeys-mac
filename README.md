# k811-fkeys-mac

Make F1–F12 behave as standard function keys by default on the Logitech K811, including after the keyboard is powered off and reconnected.

## Why this is needed

The K811 exposes Logitech HID++ 2.0 feature `0x40A2` (Fn Inversion with Default State). Setting its current state to `0` makes the top row behave as standard F1–F12. The setting is volatile: after a power cycle the keyboard restores its hardware default (`1`, media/special keys).

The verified K811 command is:

```text
10 FF 06 14 00 00 00
```

Modern Logitech software no longer supports the K811, so this project reapplies the setting automatically whenever the keyboard reconnects.

## Automatic watcher

`k811-fkeys-watch.c` is an event-driven watcher. It:

1. watches the IOKit registry for Logitech K811 (`046d:b317`) arrival;
2. briefly opens the keyboard with exclusive access;
3. sends the HID++ Fn-mode report;
4. immediately closes the device; and
5. waits for the next reconnect.

There is no polling while the keyboard is idle.

The watcher is installed as a **root LaunchDaemon**. On current macOS, the installed binary must also be granted **Input Monitoring** permission or `IOHIDDeviceOpen` returns `kIOReturnNotPermitted`.

## Install

```bash
chmod +x install-watch.sh
./install-watch.sh install
```

The installer removes older user-LaunchAgent/system-LaunchDaemon variants, builds the watcher, installs it at:

```text
/usr/local/bin/k811-fkeys-watch
```

and installs the LaunchDaemon at:

```text
/Library/LaunchDaemons/com.local.k811-fkeys-watch.plist
```

### Grant Input Monitoring

After installation:

1. Open **System Settings → Privacy & Security → Input Monitoring**.
2. Remove any stale `k811-fkeys-watch` entry left by an earlier build.
3. Press `+` and add the exact installed binary:

   ```text
   /usr/local/bin/k811-fkeys-watch
   ```

   In the file picker, press `Cmd-Shift-G` to enter `/usr/local/bin` if necessary.
4. Enable the entry.
5. Restart the daemon:

```bash
sudo launchctl kickstart -k system/com.local.k811-fkeys-watch
```

If the binary is rebuilt or replaced later, macOS may require the Input Monitoring entry to be removed and added again.

## Karabiner-Elements

Karabiner-Elements normally seizes physical keyboards exclusively. The watcher therefore needs a short window to configure the K811 before Karabiner opens it.

In **Karabiner-Elements Settings → Expert**, set **Delay before opening a device (ms)** to:

```text
1000
```

`1000 ms` has been verified to work reliably on the tested setup and is also Karabiner's default value. If a particular machine shows occasional races, use a larger value such as `2000–5000 ms` for more margin.

The corresponding Karabiner profile setting is:

```json
"parameters": {
  "delay_milliseconds_before_open_device": 1000
}
```

This is the top-level `parameters` object inside the selected profile, not `complex_modifications.parameters`.

The resulting sequence is:

```text
K811 connects
  → watcher detects the new IOHIDDevice
  → watcher seizes it briefly and sends 0x40A2 state=0
  → watcher closes it
  → Karabiner opens/seizes the K811 after its configured delay
```

## Test and diagnostics

Power the K811 off and back on. No manual command should be required; F1–F12 should come up in standard function-key mode.

Check daemon status:

```bash
sudo launchctl print system/com.local.k811-fkeys-watch
```

Check errors:

```bash
cat /tmp/k811-fkeys-watch.err.log
```

Successful operation is intentionally quiet. Errors such as `kIOReturnNotPermitted` or an exclusive-access conflict are written to stderr.

## Uninstall

```bash
./install-watch.sh uninstall
```

This removes the watcher binary, the root LaunchDaemon, any older user LaunchAgent left by development versions, and watcher logs. macOS privacy-list entries must be removed manually if desired.

## Manual tool

`k811-fkeys.c` remains available for one-shot manual configuration:

```bash
cc k811-fkeys.c -o k811-fkeys \
  -framework IOKit -framework CoreFoundation

sudo ./k811-fkeys on
sudo ./k811-fkeys off
```

## Credits

The original K811 HID report was reverse-engineered by [Julian Eberius](https://github.com/JulianEberius/k811fn) using macOS Packet Logger. The HID++ feature behavior and reconnect strategy were subsequently verified on a K811 under current macOS.
