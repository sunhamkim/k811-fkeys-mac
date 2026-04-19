# k811-fkeys-mac

Make F1–F12 behave as standard function keys by default on the Logitech K811.

## Problem

The K811 ships with media and special functions as the default behavior for F1–F12, requiring Fn to be held for standard function key input. The natural fix — Logitech's configuration software — no longer works: Logi Options+ does not recognize the K811, and the older Logi Options is too outdated to run on current macOS. Installing either just to configure one key behavior is not a reasonable option.

## Fix

The K811 accepts a HID feature report that toggles this behavior at the firmware level. On macOS Tahoe, `hidapi`-based tools fail because the OS manages the keyboard through a HIDShim kernel driver. This tool bypasses that by sending the report via IOKit directly, using `IOHIDDeviceSetReport` against the vendor-specific interface.

## Build

No external dependencies.

```bash
cc k811-fkeys.c -o k811-fkeys -framework IOKit -framework CoreFoundation
```

## Usage

```bash
sudo ./k811-fkeys on    # F1–F12 as standard function keys
sudo ./k811-fkeys off   # F1–F12 as media keys (hardware default)
```

The K811 forgets this setting when powered off. To reapply it automatically on reconnect, install the binary and set up a LaunchAgent:

```bash
sudo cp k811-fkeys /usr/local/bin/k811-fkeys
sudo chown root /usr/local/bin/k811-fkeys
sudo chmod +s /usr/local/bin/k811-fkeys
```

Create `~/Library/LaunchAgents/com.local.k811fkeys.plist`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>       <string>com.local.k811fkeys</string>
  <key>ProgramArguments</key>
  <array>
    <string>/usr/local/bin/k811-fkeys</string>
    <string>on</string>
  </array>
  <key>StartInterval</key> <integer>30</integer>
  <key>RunAtLoad</key>     <true/>
</dict>
</plist>
```

Then load it:

```bash
launchctl load ~/Library/LaunchAgents/com.local.k811fkeys.plist
```

## Credits

The HID feature report bytes for the K811 were originally reverse-engineered by [Julian Eberius](https://github.com/JulianEberius/k811fn) using the macOS Packet Logger. This tool was developed with the assistance of [Claude](https://claude.ai) (Anthropic).
