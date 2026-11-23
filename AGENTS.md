# Working on MeshMesh for the ThinkNode M9

## Purpose

Firmware for the Elecrow ThinkNode M9 (hardware v1.0 and v1.1, detected at runtime): a self-contained handheld
MeshCore chat node with channels, contacts, remote admin of repeaters and rooms, regions, regional radio presets,
offline maps, GPS/compass navigation, a signal radar with Wi-Fi CSI motion sensing, spectrum, wardriving, quick
send, a quick counter, notifications, backup/restore, its own web page and the MeshCore companion protocol over
BLE/USB. It is a slimmed-down fork of `IKrasnodymov/meshmesh` (MIT). The device always runs as a chat node; there
are no repeater or room-server modes and no other boards.

Answer in the language the user writes in. Code comments and documentation are in English.

## Repository layout

- `src/`, `include/` — the application and drivers (screens `src/Ui*.cpp|inc`, radio `src/MeshRadio.cpp`,
  companion protocol `src/Companion.inc`, web server `src/Portal.cpp`, power `src/Power.cpp`).
- `lib/MeshCore/` — vendored MeshCore with its own license and headers; keep its original headers and attribution.
  `lib/MeshProtocol/`, `lib/CayenneLPP/` — support libraries.
- `web/index.html` — the device's web page; `tools/pio_web.py` compresses it and its language files into the
  build (`PortalWeb.h` in the build directory, not in Git). `tools/web_demo.py` serves it without a device.
- `i18n/firmware/<lang>.json` — screen translations; `i18n/web/<lang>.json` — web page translations;
  `i18n/fonts/` — font licenses. See `i18n/README.md`.
- `tools/` — USB device tool, map packages, translations, UI preview and hardware checks.
- `boards/meshmesh_m9.json`, `platformio.ini`, `partitions.csv` — the build target.
- `docs/` — feature documentation (English).
- `backups/`, `artifacts/`, `logs/`, `upstream/`, `.venv/`, build output and `.env` stay local (`.gitignore`).

## Commands

```sh
python3 -m venv .venv && .venv/bin/pip install platformio esptool pyserial cryptography
.venv/bin/pio run -e m9                      # build
.venv/bin/pio run -e m9 -t upload            # flash over USB (115200 baud)
sh tools/ui_preview/build.sh m9 OUTDIR [LANG]  # render the screens on a computer (after one pio build)
python3 tools/i18n.py sync|check|todo LANG   # firmware translations
.venv/bin/python tools/i18n.py generate      # include/I18nTable.h and glyph subsets (needs upstream/fonts)
node tools/web_i18n.cjs sync|check           # web page translations (npm ci --prefix tools once)
python tools/device.py --port PORT status|config|channels   # any text command
```

The firmware version lives in `versions.properties`; after changing it run `python tools/version.py --generate`
(`include/Version.h` is generated and committed). The firmware reports `version`, `revision` (Git SHA, `-dirty`
locally) and `build_sha256`: the same SemVer does not prove the same image.

## Hardware notes (M9)

- ESP32-S3, 16 MB flash, 8 MB PSRAM, LR1110 LoRa transceiver, 320×240 screen, keyboard on I2C (0x6c on v1.0,
  0x6d on v1.1 — this is how the revision is detected), SD card, RTC, GPS, IMU and magnetometer (compass).
- USB goes through a CH340 USB-serial bridge, which is only reliable at 115200 baud. `tools/device.py` handles
  DTR/RTS and partial replies; one process owns a USB port at a time.
- Flashing only the application at `0x10000` (bootloader, partitions and `boot_app0` untouched) keeps NVS and
  LittleFS: node identity, settings, channels, contacts and history survive. Do not erase flash, format the SD card
  or reset settings as part of an ordinary update.
- NVS is about 20 KB and shared with Wi-Fi and BLE: keep large or frequently changing data (contacts, read marks,
  history) in LittleFS. `status` reports `nvs_free`.
- With BLE on, keep Wi-Fi modem sleep enabled for the station client (`WiFi.setSleep(true)`); disabling it caused
  an ESP32-S3 driver `abort()`.
- Do not change the CPU clock while the Wi-Fi driver or Bluetooth controller is running, and do not deinitialise
  the Bluetooth controller once started: both caused panics on the M9.
- The radar uses Wi-Fi only while it is open and only with the access point and Wi-Fi probe off.
- GPS/RTC time earlier than the firmware build is rejected (`src/Clock.cpp`, `gps_conflict`); never treat such a fix
  as a real position. A stored compass calibration does not prove accuracy.
- One LoRa transceiver: different radio profiles need switching; never promise simultaneous coverage of all
  networks.

## Compatibility

The radio layer is MeshCore. Matching radio parameters alone do not prove compatibility: check new compatibility
work against a real third-party MeshCore node (adverts both ways, contacts, messages, keys and ACKs). Seeing a
packet or a signal is not proof that messages can be exchanged. Requests to repeaters and rooms need a clock newer
than the previous request (replay protection): set the device clock in checks.

Keep the radio serviced during maps, USB transfers and the lock screen. Split long operations; watch memory,
lengths, CRCs, UTF-8, timeouts and atomic saves. Show DELIVERED only after the matching ACK.

## Rules

- The repository is public. Never commit private data: flash/NVS dumps, message history, private or channel keys,
  Wi-Fi passwords, BLE PINs, real node IDs or addresses, real coordinates. Check the staged files before each
  commit. Keep backups and files with secrets local with mode 0600 and out of logs and reports.
- Distinguish "builds" from "verified on hardware" in every report. Say exactly what was run (build, preview,
  host test, USB command, real radio exchange) and what was not. Host previews and Node checks do not replace a
  look at the real screen or browser; USB key events test the UI code, not the physical keys.
- Hardware checks of LoRa and BLE must show a real radio exchange: RX/TX counters growing, no restarts, one copy of
  each message.
- Keep dependencies pinned in `platformio.ini`; change them only when needed.
- Update `docs/` and this file by hand to match what was actually done.
