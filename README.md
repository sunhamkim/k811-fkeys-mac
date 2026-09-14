# k811-fkeys-mac

Configure the Logitech K811 automatically whenever it reconnects:

- standard F1–F12 mode by default; and
- keyboard backlight off on reconnect.

## Why this is needed

The K811 exposes Logitech HID++ 2.0 features for Fn inversion and backlight control, but the relevant settings are volatile across a power cycle.

Verified reconnect commands used by this project are:

```text
10 FF 06 14 00 00 00   # 0x40A2 Fn inversion: standard F1-F12
10 FF 08 14 00 00 00   # 0x1981 Backlight: Off
```

The backlight setting only establishes the reconnect state. The keyboard's normal Fn/backlight keys can still be used afterward to turn the backlight on and adjust it manually.

## Automatic watcher

`k811-fkeys-watch.c` is an event-driven watcher. It:

1. watches the IOKit registry for Logitech K811 (`046d:b317`) arrival;
2. briefly opens the keyboard with exclusive access;
3. applies standard F-key mode and backlight-off;
4. immediately closes the device; and
5. waits for the next reconnect.

There is no polling while the keyboard is idle.

The watcher is installed as a **root LaunchDaemon**. On current macOS, the installed binary must also be granted **Input Monitoring** permission or `IOHIDDeviceOpen` returns `kIOReturnNotPermitted`.

## Install

```bash
chmod +x install-watch.sh
./install-watch.sh install
```

The installer builds the watcher and installs:

```text
/usr/local/bin/k811-fkeys-watch
/Library/LaunchDaemons/com.local.k811-fkeys-watch.plist
```

### Grant Input Monitoring

After installation:

1. Open **System Settings → Privacy & Security → Input Monitoring**.
2. Remove any stale `k811-fkeys-watch` entry from an earlier build.
3. Press `+` and add:

   ```text
   /usr/local/bin/k811-fkeys-watch
   ```

   Use `Cmd-Shift-G` in the file picker to enter `/usr/local/bin` if necessary.
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

`1000 ms` has been verified to work reliably on the tested setup and is also Karabiner's default value. If a machine shows occasional races, increase it to `2000–5000 ms`.

Equivalent profile setting:

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
  → watcher briefly seizes it
  → Fn mode = standard F1-F12
  → backlight = off
  → watcher closes it
  → Karabiner opens/seizes the K811
```

## Test and diagnostics

Power the K811 off and back on. No manual command should be required.

Check daemon status:

```bash
sudo launchctl print system/com.local.k811-fkeys-watch
```

Check errors:

```bash
cat /tmp/k811-fkeys-watch.err.log
```

Successful operation is intentionally quiet.

## Uninstall

```bash
./install-watch.sh uninstall
```

This removes the watcher binary, root LaunchDaemon, older development LaunchAgent installs, and watcher logs. macOS privacy-list entries must be removed manually if desired.

## Manual tool

`k811-fkeys.c` remains available for one-shot Fn-mode configuration:

```bash
cc k811-fkeys.c -o k811-fkeys \
  -framework IOKit -framework CoreFoundation

sudo ./k811-fkeys on
sudo ./k811-fkeys off
```

## Credits

The original K811 HID report was reverse-engineered by [Julian Eberius](https://github.com/JulianEberius/k811fn) using macOS Packet Logger. The HID++ feature behavior and reconnect strategy were subsequently verified on a K811 under current macOS.
