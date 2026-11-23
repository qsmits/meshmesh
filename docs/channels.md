# MeshCore channels

Channels are MeshCore group chats: all members share one 16-byte key (AES-128), messages flood through
the mesh without delivery acknowledgements, and the sender's name is not verified. MeshMesh keeps up to
8 channels, Public included; the list lives in NVS (`meshmesh-mc/channels`, the name and key of each
joined channel, about 50 bytes each) and survives firmware updates. Keys and links use the formats of the
MeshCore app, so channels and QR codes are interchangeable with it.

## Adding a channel

| Way | M9 screen | Web page |
|---|---|---|
| Hashtag (`#name`) | Chats → Menu → “Add a channel” | yes |
| Link `meshcore://channel/add?...` | — (no need to type a long link: see invitations) | paste |
| QR code | shows the QR of its channel | shows the QR |
| Name and key (32 hex or base64) | yes | yes |
| Create a private channel (random key) | yes, then the card with its QR | yes |
| Hashtag heard on air | list in “Add a channel” | list |
| Invitation in a direct message | ← in the chat: “Join” | “Join” button |

**Hashtag.** The key is the first 16 bytes of SHA-256 of the string `#name`; leading `#` and spaces are
removed first and Latin letters lowered (as the MeshCore web client does). `#Test`, `test` and
`# test` give the same channel `#test` with the key `9cd8fcf22a47333b591d96a2b848b73f`. A hashtag channel
is open: anyone who knows or guesses the name can read it.

**Link and QR code.** The MeshCore format (`docs.meshcore.io/qr_codes`):
`meshcore://channel/add?name=<URL-encoded name>&secret=<32 hex>`. The `region_scope` parameter (the channel's
region, MeshCore App 1.47+) sets the region on joining, and MeshMesh links carry it ([regions.md](regions.md)).
The link is found anywhere in a text, so a pasted piece of a conversation works too.
QR code: byte mode, error correction M, versions 1–10; the encoder is our own (`src/Channels.cpp`, its
JavaScript copy is in `web/index.html`).

**Name and key.** 16 bytes: 32 hexadecimal digits or base64 (24 characters with `==`, like the Public key
`izOH6cXN6mrJ5e26oRXNcg==`). MeshCore does not support 256-bit keys, neither does MeshMesh.

**New private channel.** The device's random number generator makes the key; then open the channel card
and show its QR code or send an invitation. The key of a private channel is shown on the M9 card (QR and
32 hex) and on the page over the device's own access point; the page over the home network (plain
HTTP) does not get the links of private channels, just as it does not get the node key.

**Heard on air.** Packets of channels this node has not joined cannot be decrypted; only the channel's hash
byte is visible. MeshMesh counts such packets per hash (relayed copies once), keeps the last six and tries
common hashtags on them (`#test`, `#ping`, `#mesh`, `#meshcore`, `#wardrive`, `#eu` and others, the list is
`channels::commonTags` in `src/Channels.cpp`). When a name fits, the channel is shown as “#name — join”. While a
hashtag is being typed, the screen and the page show how many stored packets it opens. The key of a private
channel cannot be found this way.

**Invitation.** Channel card → “Invite” → a chat contact: a direct message with the channel link is sent
(encrypted with the node keys, with a delivery acknowledgement). MeshMesh shows it as “Channel invitation …”
and offers to join; stock MeshCore shows the link text. A channel name with many non-ASCII letters may not fit
a message (151 bytes) URL-encoded; then its letters go as they are, without percent-encoding; MeshMesh reads both.

## Controls

- **M9 screen.** Chats shows Public, joined channels and direct conversations. Press U for the next unread
  conversation. Use Chats → Menu → Add a channel to join or create one. On a channel row ←/→ open the channel
  card, in a channel chat the ← key. Card: QR code, hash, key of a private channel, region, notifications
  (including a per-channel sound), “Invite” (←/→ choose the contact), “Leave channel” (OK twice).
- **Web page.** Chat list with the channels, “+ Channel”, channel card with the QR code, invitations, removal.
- **USB and BLE.** `channels` lists them (with keys); `channel do {JSON}`:
  `{"action":"add","hashtag":"test"}`, `{"action":"add","link":"meshcore://..."}`,
  `{"action":"add","name":"Friends","key":"<32 hex>"}`, `{"action":"add","create":"Friends"}`,
  `{"action":"remove","channel":"<ID>"}`, `{"action":"invite","channel":"<ID>","to":"<node>"}`,
  `{"action":"probe","hashtag":"utrecht"}`, `{"action":"region","channel":"<ID>","region":"nl-ut"}`,
  `{"action":"policy","channel":"<ID>","sound":1}`. Sending: `send <channel ID> text`. HTTP has the same
  actions: `GET/POST /api/channels`.

A channel's ID in the history is `FF` followed by the first 7 bytes of SHA-256 of its key (Public: `ALL`).
A channel that was left disappears from the list; its messages stay in the history and come back when it
is joined again. Public cannot be removed.

## Limits

- One transceiver: channels work on the current radio profile; a channel on another frequency is not heard.
- Channels have no delivery acknowledgements (as in MeshCore); the sender's name is part of the text and is not verified.

See also [notifications.md](notifications.md) for per-channel notifications, priority and retention.
