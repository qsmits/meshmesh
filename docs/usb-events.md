# USB events and the range test

## USB events

A program on the computer can follow the mesh through the USB cable without polling. Send the text command
`events on` once (the board remembers it; `events off` stops it, `events` shows the setting). From then on the
board writes one line per event, between the replies to commands:

```
EVENT {"type":"message","from":"A1B2C3D4E5F60718","to":"FF0123456789ABCD","channel":"Range test","name":"M9","text":"hello","time":1795470955,"id":2690089274,"snr":9.5,"rssi":-98,"route":"flood","hops":0,"heard":1}
```

| `type` | When |
|---|---|
| `message` | A direct or channel message arrived. `to` is the channel ID, or this node for a direct message. |
| `sent` | A message of ours went out. |
| `repeated` | A repeater was heard passing our channel message on. |
| `delivered` | A direct message was acknowledged. |
| `failed` | A transmit error, or no acknowledgement or repeat heard in time. |
| `node` | A node's advert was heard (`name`, `snr`, `rssi` of that packet). |

`snr` is in dB and `rssi` in dBm, measured on the packet as this board heard it. `hops` is the number of
repeaters the packet passed (0: heard straight from the sender). The SX1262 of the Heltec reports 0 dBm for a
very strong signal (boards next to each other).

Up to 8 lines wait while the computer is not reading; older ones are dropped. While a MeshCore app talks
to the board over USB (companion frames), no event lines are written. Replies use the existing commands, such
as `send CHANNEL_ID text` or `send NODE_ID text`.

## Range test

`tools/range_bot.py` turns a board on USB (the Heltec by default) into a base station for a private channel:

```sh
.venv/bin/python tools/range_bot.py --channel CHANNEL_ID [--home LAT,LON]
```

Walk around with a second board and send messages on the channel with your GPS position: on the M9, point
Quick Send at the channel and turn its GPS option on (the message gets ` @lat,lon`). For every message the
base station:

- replies on the channel, e.g. `#3 heard: SNR 6.5 dB, RSSI -98 dBm, direct, 1.24 km`, so you see in the
  field whether you are still in range;
- appends the event to `logs/range-test.jsonl` and redraws `logs/range-test.html`, a map with each point
  coloured by SNR and its distance from home (without `--home`, the first position received is home).

Lines appended to `logs/range-test.outbox` are sent on the channel as well, so someone at the computer can
answer. The log and the map hold positions: `logs/` is not committed.
