# Heltec WiFi LoRa 32 V4

MeshMesh also runs on the Heltec WiFi LoRa 32 V4 (ESP32-S3, SX1262, 0.96" 128×64 OLED, one PRG button). There are
two builds:

- `heltec_v4` for the standard V4 (2 MB quad PSRAM);
- `heltec_v4_r8` for the V4-R8 (8 MB octal PSRAM, other pins for the peripheral power, GNSS and LED).

```sh
.venv/bin/pio run -e heltec_v4                # or heltec_v4_r8
.venv/bin/pio run -e heltec_v4 -t upload      # flash over the board's own USB
sh tools/ui_preview/build.sh heltec OUTDIR [LANG]   # render the screens on a computer
```

The board's USB goes straight to the ESP32-S3 (USB Serial/JTAG), so there is no baud rate to choose.
`tools/ports.py` finds it by 303a:1001 (`MESHMESH_HELTEC` overrides), and `tools/device.py` keeps DTR on and RTS
off so a connection does not reset it.

## Controls

The PRG button does everything:

- **Click**: the next screen. In a menu or list, the next item.
- **Hold** (about a second): the screen's action. A screen with several actions opens a menu instead; in the menu
  a hold runs the item. Quick send and folders open with a hold, so clicking through the screens passes them.
- **Double click**: one step back (closes a menu, popup or app, or goes to the previous screen). It is off by
  default because a single click then waits briefly for a second one. Turn it on in Settings → *Double click* or on
  the web page.
- **Turning off**: Settings → *Turn off...*, then hold again. Hold PRG to turn the board on.

The screens in the click cycle are Home, Messages, Quick send, Nodes, GPS, Radar, Wardrive, Wi-Fi, Bluetooth, Modules
and Settings. The order, the hidden screens and the folders are set on the web page or in the app (`apps`). A
message pop-up, the LED and waking the screen follow the notification settings.

## Differences from the M9

The Heltec keeps the chat node, channels, quick send, nodes with remote login, status and trace, regions, the
network search over the regional presets, the signal radar and CSI motion sensor, GPS (with a module on the GNSS
connector), wardriving, the web page, BLE and the MeshCore companion protocol. Without a keyboard, longer messages
are written on the web page or in an app.

It has no offline maps, navigation, spectrum, SD card, RTC, compass, IMU, buzzer or screen lock. The radio
editor on the device covers the 863–870 MHz band, as on the M9.
