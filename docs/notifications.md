# Notifications, quick send and counting

## Notifications

Screen wake and the incoming-message pop-up are independent: off, all messages or mentions. Each channel may
override or inherit them, and each channel has its own sound switch. Configure them in the M9 screen settings
(wake and pop-up), on the channel card, or in the settings of the web page.

Mentions use `@[node name]`, case-insensitive for Latin and Cyrillic. Optional comma-separated words
(`notify_words`) also trigger mentions. A plain unmarked name does not. Events are independent of history
retention; queue overflow is reported by `status.notification_dropped`.

The M9 has no software-controlled notification LED: `status.notification_led` is false, and the `notify_led`
setting has no visible effect.

Optional failure alerts distinguish radio TX failure, missing direct-message ACK and no channel repeat heard within
30 seconds. Missing a repeat does not prove failed delivery. Delivered still requires the appropriate ACK. Manual
counting does not let pop-ups consume the next key press.

Web page notification settings separately control incoming banners (all, mentions, off) and action feedback (all,
errors, off). Operation errors remain visible in the interface.

Settings (`set {JSON}`): `notify_wake`, `notify_popup`, `notify_led` (0 off, 1 all, 2 mentions), `notify_failed`
(true/false), `notify_words` (up to 96 UTF-8 bytes), `sound` (true/false, the device-wide beep). Per channel:
`channel do {"action":"policy","channel":"<ID>",…}` with `led`, `wake`, `popup` (0 off, 1 all, 2 mentions,
3 as the device setting), `sound` (0 silent, 1 a beep on a new message), `priority` (0–2), `device_limit`
(0, 8, 16).

## Quick send

Ten shared presets, a recipient and optional GPS attachment are stored on the device. Public is the initial
recipient. A selected joined channel, chat contact or room is saved by ID; a removed recipient fails instead of
silently falling back to Public. Edit presets on the web page and use them on the device. The ↗ chat button opens
quick reply to that conversation.

The recipient selection sits in a separate card above the presets, visible while scrolling, and shows the current
channel or contact in colour. Normal entry selects the card; confirming a recipient saves its ID and selects the
first preset without sending. Cancelling the recipient list returns to the card.

M9 keys: arrows select, OK on the card opens recipients, OK on a preset sends, back exits. Right with an empty chat
composer opens quick reply.

Presets allow at most 160 UTF-8 bytes; sending also checks the recipient's smaller limit. BLE commands have a
shared 255-byte encoded limit including JSON and escaping; longer quoted/multiline templates can be saved through
USB or Wi-Fi. GPS attachment defaults off and requires a fresh trusted fix, trusted clock and no GPS conflict.
Stale or conflicting coordinates produce an error.

USB/BLE/HTTP: `quick` returns JSON. `quick do {"action":"set","index":0,"text":"OK"}` edits; `target` takes `to`;
`send` takes `index` and optional `to`; `gps` takes `enabled`; `reset` restores defaults. HTTP GET/POST
`/api/quick` maps to the same commands.

The preset and channel-selection idea comes from the
[dt267 guide](https://github.com/dt267/MeshCore-Low-Power-Firmware/blob/main/Companion_Display_Guide.md#quick-send-channel-screen).
MeshMesh uses its own UI and network implementation.

## Channel priority and retention

Channels have low, normal or high priority. Device limits: automatic, 8 or 16. Automatic low priority retains 16
device messages. The device has a global 64-message history and evicts older lower-priority rows first; a channel
with a bounded depth receives space even when the global history is full, then replaces its own oldest rows. The
shared limit remains 64 when channels compete. Active sends are protected until complete. Reducing a cap removes
that channel's oldest rows. A second limit for an external archive (`app_limit`: automatic, 8, 16 or 32) is stored
with the channel but not used by the firmware itself.

Compaction writes one row per main-loop iteration and retains the previous complete file until the new snapshot is
committed. Presets, channel policies and manual counts use two verified LittleFS copies, keeping large records out
of NVS.

## Periodic Wi-Fi time

The device can save a network and enable periodic NTP without the persistent Wi-Fi client. With that client off and
the clock not refreshed for an hour, a temporary connection lasts at most 45 seconds. Success switches Wi-Fi off
until the next hour. Failures retry after 10, 20, 40 and 60 minutes. Access point, radar, CSI and Wi-Fi probes take
priority. Fresh time from a connected host postpones NTP.

Configure this in Connection / Internet. Commands: `internet save {"ssid":"...","password":"..."}`,
`internet ntp on|off`, `internet` for status. `internet add` enables the persistent client. Periodic NTP defaults
off.

## Quick counter

The quick counter holds up to 8 cards, each with its own name, colour and value 0–999999 (people, laps, sets). A
card without a name shows as "Counter N". The open card is shared by the screen and the web page. Values are saved
2 s after the last press (`/meshmesh/tally`); adding, renaming and deleting are saved at once. The last card cannot
be deleted.

- M9 screen: the page opens straight on a card — OK/up +1, down −1, left/right turn the cards, and past the last
  one is "New counter". N or a hold names it, C colours it, R twice resets it to 0, DEL twice deletes it, B and W
  show the BLE and Wi-Fi estimate under the card.
- Web page: cards swipe left and right, a tap on the number is +1, with −1, "Edit" (name and value), colour,
  reset and delete with confirmation, dots and a "New counter" card.

Automatic results are BLE devices and Wi-Fi devices, kept separate from manual counting and from each other. RSSI
threshold −100…−30 dBm, window 30/60/300 seconds, optional BLE phone/watch filter. Each table has 128 temporary
identifiers and indicates saturation and dropped observations. No names, addresses or payloads are saved by the
counter; identifiers are salted per boot and tables stay in RAM.

Background BLE scanning is passive and shares the existing controller with advertising and connections. Passive
Wi-Fi listens to probe requests and client frames, hopping permitted channels; it yields to access point, radar,
CSI, probes and Internet. Both automatic modes default off.

People may carry multiple devices, devices may not advertise, and addresses may change. Automatic device counts are
estimates rather than headcounts and are not added to the manual result. The page explains this beside the
indicators.

Commands: `people` returns JSON (`counters` with `name`, `value`, `color`, `current`, `counters_max`; `manual` is the
open card's value); `people do {"action":"change","delta":1}` (`index` picks a card, the open one by default),
`reset`, `set` with `value`, `select`, `add` with an optional `name`, `rename` with `name`, `color` with `color`
0–14, `delete`, `window_reset`, `settings` with `ble`, `wifi`, `personal_only`, `window`, `rssi`. HTTP GET/POST
`/api/people` maps to the same commands.
