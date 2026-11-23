# MeshCore apps (companion protocol)

The official MeshCore apps can connect to an M9 running MeshMesh: MeshCore, MeshCore Open and other clients of
the companion protocol, for example the `meshcore` Python library. The app works with the device's own chats,
contacts and channels. Maps, the radar, wardriving and the other MeshMesh tools stay on the screen and the
device's web page.

## Connecting

There is nothing to switch. The device's Bluetooth advertises as stock firmware does: the Nordic UART
service (`6E400001-B5A3-F393-E0A9-E50E24DCCA9E`) and the name `MeshCore-<node name> XXXX` (the name is cut to
15 bytes, XXXX are the first 4 hex digits of the node key, so devices with the same name differ in the list;
on the mesh the node keeps its plain name). MeshCore apps find the device by this name. The advert also carries
an `MM` mark in the manufacturer data (company ID 0xFFFF, "no company"), and the device keeps its own text-command
service `7a9e0001-…` (used by `tools/ble_device.py`). One phone connects at a time.

Pairing is protected by the PIN shown on the screen. The `MeshCore-…` name is shown in "Connections"
on the screen and on the web page. The radar counts Bluetooth devices named `MeshCore-` and `MeshMesh ` as
mesh nodes.

The same protocol works over USB, as on the stock firmware: the byte `<`, a 2-byte length (low byte
first), then the frame; replies come as `>`, the length, the frame. Text USB commands keep working.
The first text command closes the link with the app, so frames never mix with text.

## What works

Protocol version 10 (`examples/companion_radio` of MeshCore companion-v1.17.1), code in `src/Companion.inc`.

- Device and node information: name, key, radio parameters, power.
- Contacts: the list (also only those changed since a given mark), lookup by key, adding, editing,
  removing, path reset, exporting and importing a contact card, sending a contact's card to nearby nodes.
- Direct messages. The app retries on its own; the device makes one attempt per request. A retry of
  the same message does not add a new history row. The delivery acknowledgement (ACK) reaches the app
  as on the stock firmware, also after its timeout has passed.
- Channels: the list by number, joining by key or hashtag, leaving. Public stays first.
- Receiving: direct and channel messages wait for the app in a queue (12 frames, in RAM). When it
  overflows, old channel messages go first. Messages appear on the screen as usual.
- Node advert (flooded or zero-hop), time (read and set), name, radio parameters, power, relaying,
  path hash size (`CMD_SET_PATH_HASH_MODE`: mode 0–2 = 1–3 bytes; the same setting as “Path hash size” on the
  device), battery and storage, core, radio and packet statistics, restart.
- Messages sent from the app are kept in the device's history and shown on the screen and the web page.
- MeshCore repeaters and room servers: login with a password (admin, member, guest), CLI commands and their
  replies, status, telemetry and binary requests (neighbours, access list), anonymous requests, room posts with
  ACK and the room's posts (signed messages with their author), connection check and logout. Login, status, CLI
  and trace are also available on the M9 screen, on the web page and over USB (`remote login|status|cli|trace`).
- Path trace through given repeaters (hashes and the SNR of each hop) and path discovery to a node (a flooded
  telemetry request; both paths go to the app and are not stored as the route). The device's own telemetry is its
  battery voltage.
- Regions ([regions.md](regions.md)): the default region (`CMD_SET_DEFAULT_FLOOD_SCOPE` 63,
  `CMD_GET_DEFAULT_FLOOD_SCOPE` 64; the same "Region" as in the device's radio settings), the session region
  (`CMD_SET_FLOOD_SCOPE_KEY` 54: a key, a reset or the "unscoped" flag; it holds while the app is connected) and
  zero-hop control packets (`CMD_SEND_CONTROL_DATA` 55, answers as `PUSH_CODE_CONTROL_DATA` 0x8E) for the app's
  region search.

The clock. A repeater or room drops a request whose time is not newer than the client's previous one
(replay protection). The official apps set the device's clock when they connect; if the clock is not set
and the app has already sent a message with its own time, the device's next requests are refused until
the clock is set. The room's "read up to" mark is kept only until a restart: after it the room may send past
posts again.

Input limits are those of the screen: radio parameters within MeshMesh settings (863–870 MHz,
BW 62.5/125/250/500 kHz), the name up to 24 bytes of UTF-8, a channel name by the rules of
[channels.md](channels.md).

## What is missing

The device answers these commands with "unsupported" (`ERR_CODE_UNSUPPORTED_CMD`):

- raw packets, channel data, data signing;
- answers to other nodes' telemetry requests (as on stock firmware by default: "deny"), so a path
  discovery to a MeshMesh node gets no answer; repeaters and rooms answer;
- coordinates for the advert set from the app (the position comes from the device's GPS), other
  parameters (telemetry mode, extra ACKs, manual contact adding), auto-add settings, receive delay,
  PIN change;
- exporting and importing the private key (answered "disabled", as on stock firmware without that option;
  use the [backup](backup.md) instead).

Channels with 256-bit keys are not supported by MeshCore either.

## Checking

`tools/companion_check.py --port APP_PORT --peer PEER_PORT` drives one MeshMesh device over USB frames as a
MeshCore app would, against a second MeshMesh device: device information, contacts, a direct message with its ACK,
direct and channel messages synced by the app. Use over Bluetooth from a phone has to be checked separately.
