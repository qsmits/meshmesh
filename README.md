# MeshMesh for the ThinkNode M9 and Heltec V4

Off-grid messaging firmware for the **Elecrow ThinkNode M9** (hardware v1.0 and v1.1, detected at runtime) and
the **Heltec WiFi LoRa 32 V4**. Messages hop by LoRa radio from device to device, with no internet, cell network
or servers. The radio layer is [MeshCore](https://github.com/meshcore-dev/MeshCore), so the devices talk to
standard MeshCore nodes and work with the official MeshCore apps.

This is a slimmed-down fork of [IKrasnodymov/meshmesh](https://github.com/IKrasnodymov/meshmesh) that keeps only
these two boards and their messaging and field tools. The device always runs as a MeshCore chat node.

The features below are those of the M9. The Heltec V4 has a 128×64 screen and one button, and a subset of them:
see [docs/heltec.md](docs/heltec.md).

## Features

- **Chats** — direct and channel messages with delivery receipts, retries and the route of each message;
  common emoji drawn in colour.
- **Channels** — up to 8 MeshCore channels: join by #hashtag, `meshcore://` link or QR code, name and key; create
  a private one, invite contacts by direct message, find channels heard on air; per-channel notifications and
  sound. [docs/channels.md](docs/channels.md)
- **Contacts and nodes** — signal, hops, distance and bearing of nearby nodes.
- **Remote admin** — log in to MeshCore repeaters and room servers, send CLI commands, read status, trace paths.
- **Regions** — MeshCore flood scope: a default region and one per channel, and a search of the regions nearby
  repeaters serve. [docs/regions.md](docs/regions.md)
- **Regional radio presets** — the community list of the MeshCore app; the device can also search for a network
  on air by trying the presets one by one and listening for repeaters.
- **Offline maps** on the SD card, with nodes and your GPS position; tiles can also be fetched over Wi-Fi.
  [docs/maps-format.md](docs/maps-format.md)
- **Navigation** — GPS and compass.
- **Signal radar** — Wi-Fi, Bluetooth and LoRa around you, with “warmer / colder” homing, and a Wi-Fi CSI motion
  sensor between two devices.
- **Spectrum** — a waterfall of received power across the 863–870 MHz band.
- **Wardriving** — a coverage map of the mesh (packets heard, channel pings and the repeaters that echo them), plus
  Wi-Fi and Bluetooth; export to CSV, GeoJSON, KML and WiGLE. [docs/wardrive.md](docs/wardrive.md)
- **Quick send**: ten preset messages to a chosen contact or channel.
  [docs/notifications.md](docs/notifications.md)
- **Notifications** — screen wake, pop-ups and sound, for all messages or mentions only.
- **Backup and restore** of settings, channels, contacts and optionally the node key in one JSON file.
  [docs/backup.md](docs/backup.md)
- **Web page** served by the device over its own Wi-Fi access point or your home Wi-Fi; text commands over USB and
  BLE.
- **MeshCore companion protocol** over BLE and USB: the official MeshCore apps connect to the device.
  [docs/companion.md](docs/companion.md)
- **USB events and range test**: the board reports incoming messages over USB as they arrive; `tools/range_bot.py`
  makes a USB-connected board answer and map range-test messages. [docs/usb-events.md](docs/usb-events.md)
- **Power** — idle waits, a lower CPU clock when unused, and software power off. [docs/power.md](docs/power.md)
- **14 interface languages**: English, Ukrainian, Spanish, Portuguese, French, German, Italian, Polish, Turkish,
  Chinese, Japanese, Korean, Arabic and Indonesian.

Default radio: 868.731 MHz, BW 62.5 kHz, SF8, CR4/6. Change it (or pick a regional preset) in Settings → Radio;
all nodes of a network must match. Follow your country's radio regulations.

## Building and flashing

```sh
python3 -m venv .venv
.venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9                 # build
.venv/bin/pio run -e m9 -t upload       # build and flash over USB
```

For the Heltec V4 use `-e heltec_v4` (or `-e heltec_v4_r8` for the V4-R8); its USB is the ESP32-S3's own, so the
baud rate does not matter. The M9's USB port goes through a CH340 USB-serial bridge, which is only reliable at 115200 baud (the PlatformIO
environment already uses that speed). An upload does not erase the flash: the node key, settings and channels in
NVS and LittleFS survive. To write only the application, flash `firmware.bin` at `0x10000`:

```sh
python -m esptool --chip esp32s3 --port PORT --baud 115200 write-flash 0x10000 .pio/build/m9/firmware.bin
```

## Screen preview

The screen UI can be rendered on a computer without a device (run a `pio run -e m9` build first so the libraries
are downloaded):

```sh
sh tools/ui_preview/build.sh m9 OUTDIR [LANG]       # the M9
sh tools/ui_preview/build.sh heltec OUTDIR [LANG]   # the Heltec V4
```

## Device tool

```sh
python tools/device.py --port PORT status      # also: config, channels, or any other text command
```

## Translations

English is the source language in the code; translations live in `i18n/`. See [i18n/README.md](i18n/README.md).

```sh
python3 tools/i18n.py sync              # after changing strings in src/
python3 tools/i18n.py todo de           # what a language still lacks
python3 tools/i18n.py check
.venv/bin/python tools/i18n.py generate # tables and glyph subsets for the firmware
```

## Documentation

[backup](docs/backup.md) · [channels](docs/channels.md) · [MeshCore apps](docs/companion.md) ·
[Heltec V4](docs/heltec.md) · [map format](docs/maps-format.md) · [notifications and quick send](docs/notifications.md) ·
[power](docs/power.md) · [range test and USB events](docs/usb-events.md) · [regions](docs/regions.md) · [wardriving](docs/wardrive.md)

## Credits

- **Igor Krasnodymov** ([IKrasnodymov](https://github.com/IKrasnodymov)) — original author of MeshMesh.
- **Bas van Dijk** — GPS and keyboard fixes for the M9 v1.1, regional radio presets, battery charge detection and
  per-channel sound.
- The radar idea follows the RSSI tracker and radar HUDs by [Stevee87](https://github.com/Stevee87).

## License

[MIT](LICENSE). Bundled MeshCore (`lib/MeshCore`) keeps its own MIT license. Screen emoji are drawn from
[Twemoji](https://github.com/jdecked/twemoji) graphics by Twitter, Inc and other contributors,
[CC-BY 4.0](https://creativecommons.org/licenses/by/4.0/). Map data ©
[OpenStreetMap](https://www.openstreetmap.org/copyright) contributors.
