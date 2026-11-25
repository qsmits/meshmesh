# Notifications and quick send

## Notifications

Screen wake and the incoming-message pop-up are independent: off, all messages or mentions. Each channel may
override or inherit them, and each channel has its own sound switch. Configure them in the M9 screen settings
(wake and pop-up), on the channel card, or in the settings of the web page.

The message beep has two settings of its own, at the end of the M9's "Screen & device" settings and on the web
page: **Channel sound** (off, all, mentions only) and **Direct message sound** (on, off). A channel can also be
muted on its own: "Channel sound" in the menu of the open chat or of the chat list, or on the channel card. A
message identical to one the same sender sent in the same chat within 10 minutes is no new message: some apps resend
a channel message when they hear no repeater pass it on. It gets no row of its own and no beep; the earlier row
counts it as heard again. Copies of one message through different repeaters were already merged this way.

Mentions use `@[node name]`, case-insensitive for Latin and Cyrillic. Optional comma-separated words
(`notify_words`) also trigger mentions. A plain unmarked name does not. Events are independent of history
retention; queue overflow is reported by `status.notification_dropped`.

The M9 has no software-controlled notification LED: `status.notification_led` is false, and the `notify_led`
setting has no visible effect.

Optional failure alerts distinguish radio TX failure, missing direct-message ACK and no channel repeat heard within
30 seconds. Missing a repeat does not prove failed delivery. Delivered still requires the appropriate ACK. 
Web page notification settings separately control incoming banners (all, mentions, off) and action feedback (all,
errors, off). Operation errors remain visible in the interface.

Settings (`set {JSON}`): `notify_wake`, `notify_popup`, `notify_led` (0 off, 1 all, 2 mentions), `notify_failed`
(true/false), `notify_words` (up to 96 UTF-8 bytes), `sound` (true/false, the device-wide beep), `sound_channels`
(0 off, 1 all, 2 mentions), `sound_direct` (true/false). Per channel:
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

