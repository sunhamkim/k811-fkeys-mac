# k811-fkeys-mac

Make F1–F12 behave as standard function keys by default on the Logitech K811.

## Problem

The K811 ships with media and special functions as the default behavior for F1–F12, requiring Fn to be held for standard function key input. Modern Logitech software no longer supports the K811.

The keyboard exposes HID++ 2.0 feature `0x40A2` (Fn Inversion with Default State). Setting its current state to `0` switches the top row to standard F1–F12, but the setting is volatile: after the keyboard is powered off and on, the K811 restores its hardware default (`1`).

## Manual tool

`k811-fkeys.c` sends the known HID++ report directly with IOKit.

```bash
cc k811-fkeys.c -o k811-fkeys \
  -framework IOKit -framework CoreFoundation

sudo ./k811-fkeys on    # standard F1–F12
sudo ./k811-fkeys off   # media/special keys
```

## Event-driven watcher

`k811-fkeys-watch.c` watches for the K811 (`046d:b317`) with `IOHIDManager`. Whenever the keyboard appears, it sends:

```text
10 FF 06 14 00 00 00
```

and immediately closes the device. There is no polling.

The included installer builds the watcher and installs it as a root LaunchDaemon:

```bash
chmod +x install-watch.sh
./install-watch.sh
```

To remove it:

```bash
./install-watch.sh uninstall
```

After installation, power-cycle the K811. The daemon remains idle until the keyboard connects or reconnects.

## Karabiner-Elements

Karabiner-Elements normally opens physical keyboards with exclusive access, so another process cannot send the HID++ report after Karabiner has seized the K811.

The workaround is to give the watcher a short window before Karabiner opens newly connected devices.

In **Karabiner-Elements Settings → Expert**, set **Delay before opening a device (ms)** initially to:

```text
5000
```

With that delay, the sequence is:

```text
K811 connects
  → k811-fkeys-watch sends 0x40A2 state=0 and closes the device
  → Karabiner opens/seizes the K811
```

Once this is stable, reduce the delay experimentally. The watcher retries for up to about 2 seconds, so a delay above that leaves comfortable margin; a shorter delay may also work reliably on a given machine.

The underlying profile setting is:

```json
"parameters": {
  "delay_milliseconds_before_open_device": 5000
}
```

This is the top-level `parameters` object inside the selected profile, not `complex_modifications.parameters`.

## Notes

The watcher uses a shared `IOHIDDeviceOpen` and closes the device immediately after the report is sent. If Karabiner has already seized the keyboard, the open attempt fails; the Karabiner delay is therefore essential to this approach.

The watcher is intentionally quiet except for successful application and errors. It does not query the Fn state because the K811's feature indices and desired report have already been verified.

## Credits

The original K811 HID report was reverse-engineered by [Julian Eberius](https://github.com/JulianEberius/k811fn) using macOS Packet Logger. The HID++ behavior and reconnect strategy were further tested on current macOS with the K811 itself.
