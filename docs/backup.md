# Backup

The device's settings can be saved to one JSON file and written back to the same M9 or moved to another one:
Settings → Backup on the device's web page. The page reads and writes the copy with the device's ordinary
commands, so the same is available over USB and BLE (`tools/device.py`).

## What it holds

- **Settings** (`config`): name, radio profile, power, relaying, path hash, region, regional preset, screen
  language and brightness, theme, lock, notifications, time zone, app order. The previous and test radio profiles
  (`radio_prev`, `radio_trial`) are not carried over.
- **Node key** (optional, ticked by default): the MeshCore private key in MeshCore's export format — 64 bytes as
  hexadecimal digits. With it a restored or new device stays the same node for your contacts.
- **Channels** with their links and keys, region and notification settings; for Public, its region and notifications.
- **Contacts**: public key, name, type, position. The path to a node is not kept: after a restore the first message
  goes through the whole mesh (flood) and the path is found again.
- **Quick Send**: templates, recipient, position in the message.
- **Quick counter**: cards (name, value, colour), the open card, the device counting settings.

Not included: messages, maps, the wardrive log (it has its own export), the Wi-Fi access point password (new at
every start) and the BLE PIN, and notification settings the browser keeps itself.

The file holds the channel keys and, if ticked, the node key as plain text: with them one can read your direct
messages and private channels. Keep it like a password.

## Restore

Once a file is chosen the page compares it with the device and shows only what will change:

- every changed setting with its old and new value, each with its own tick; a change of the radio profile gets its
  own warning; settings this firmware does not know are listed and skipped;
- new channels and channels whose region or notifications differ;
- new contacts (existing ones are not overwritten);
- changed Quick Send templates, the counter cards;
- the node key, if it differs from the current one.

Only what is ticked is written, in this order: settings (the radio profile in one command, the rest in pieces of up
to 200 bytes — a BLE write holds 255; if a piece fails, its settings are retried one by one), channels, contacts,
Quick Send, counters and, last, the node key. The result is shown line by line. A new key is stored and takes
effect after a restart the device makes by itself 1.5 s later; contact changes still pending are saved first.
With the new key the device announces itself once.

Do not run two devices with the same key at once: the mesh cannot tell them apart, and replies and acknowledgements
reach both.

## Commands

Backup-specific commands (USB, BLE, `/api/command`):

| Command | Answer |
|---|---|
| `identity` | `{"private_key":"<128 hex>","public_key":"<64 hex>"}` |
| `identity set <128 hex>` | `OK identity saved; restarting`, `OK identity unchanged`, `ERR identity: …` |
| `contact add {"key":"<64 hex>","name":"…","type":1,"lat":52.09,"lon":5.12}` | `OK contact saved` (a contact already known stays as it is) |

Over Wi-Fi, `identity` and `identity set` work only through the device's own access point: on a home network the
page travels unencrypted, and the device answers `ERR the key is given out on the device access point only`. A key
is accepted only if it gives a public key MeshCore allows (not starting with 00 or FF).

Everything else is read and written with existing commands: `config`/`set`, `channels`/`channel do`, `nodes`,
`quick`/`quick do`, `people`/`people do`.

## File format

```json
{"format":"meshmesh-backup","version":1,"created":"2026-11-09T07:09:28Z","firmware":"0.33.0",
 "board":"m9","board_name":"ThinkNode M9 v1.1","node":{"id":"…","name":"…","public_key":"…"},
 "settings":{…},"identity":{"private_key":"…"},
 "channels":[{"id":"…","name":"…","kind":"private","link":"meshcore://channel/add?…","region":"","policy":{…}}],
 "contacts":[{"key":"…","name":"…","type":1,"lat":0,"lon":0}],
 "quick":{"to":"ALL","gps":true,"presets":["…"]},
 "counters":{"current":0,"counters":[{"name":"","value":0,"color":0}],"settings":{…}}}
```

`board` is always `m9`; `board_name` names the hardware revision the copy was made on.

The file holds one public key, `node.public_key`: it shows which node the copy is from and is compared with the
current device. A restore applies only the private key `identity.private_key`; the device derives the public key
from it and keeps both together (one `identity` record in the MeshCore storage). Older copies may repeat the public
key in `identity.public_key`; it is not applied, the page uses it only for the preview.

The file name is `meshmesh-backup-<name>-YYYY-MM-DD-HH-MM.json` in the local time of the phone or computer;
`created` inside the file is UTC in ISO 8601.

If the node key could not be read (home network), `identity_error` is written instead of `identity`. A section is
missing when the device does not report that feature.

## Limits

- The contact table holds 24 nodes: when it is full a new contact replaces one not heard for a long time.
- At most 8 channels including Public.
