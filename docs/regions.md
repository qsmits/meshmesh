# MeshCore regions

A region (flood scope) limits where flood packets spread. A node marks a packet with the transport code of a
region, and repeaters with regions set up forward it only if they serve that region. A packet without a region is
common: repeaters that allow `*` (`region allowf *`) forward it. Regions exist since MeshCore 1.10; the
`region def` syntax since 1.16.

The code is the first 2 bytes of HMAC-SHA256 keyed with the region key over the packet's type byte and payload.
The key of a public region is the first 16 bytes of SHA-256 of the string `#name` (`nl-ut` → SHA-256(`#nl-ut`)).
Names are case-sensitive, as on repeaters: `nl-ut` and `NL-UT` are different regions. A region starting with `$` is
private: its key does not come from the name, and only the MeshCore app sets it.

Code: `include/Regions.h`, `src/Regions.cpp`, the search in `src/RegionSearch.inc`, sending in
`MeshCoreBackend::floodScoped` and `MeshRadio::scopeKey` of `src/MeshRadio.cpp`.

## Which region a packet carries

Which region a flood packet of the node carries (messages, ACKs, path returns, requests to repeaters and rooms,
adverts):

1. While a MeshCore app is connected and has chosen a session region (`CMD_SET_FLOOD_SCOPE_KEY`): that region or
   "no region". The choice is dropped when the app disconnects.
2. A channel message: the channel's region — its own, "no region" (`*`) or "default".
3. Otherwise the default region of the radio settings; when it is empty, the packet goes without a region.

A message's region is fixed when it is queued; retries keep it. Direct and zero-hop packets carry no region, as
in the stock firmware. Reception does not depend on regions: the device hears all traffic. The device's own relay
does not check regions, like the stock companion.

While not every repeater of a route has its regions set up, messages with a region may not arrive. Then give the
channel "no region" (`*`) or clear the default region.

| Where | Default region | Channel region | Search |
|---|---|---|---|
| M9 screen | "Settings → Radio → Region": OK types a name, ← → the regions found | the channel card, "Region" row: ← → default, no region, the regions found; OK saves or types a name | "Radio → Find regions" |
| Web page | "Settings → Radio → Region" with the regions found suggested | the channel card, "Region" section | "Radio → Regions → Find regions" |
| MeshCore app | "Default scope" (`CMD_SET_DEFAULT_FLOOD_SCOPE` / `GET`) | a chat's region scope (the session region before sending) | "Discover regions" (control data and anonymous requests) |
| USB / BLE | `set {"region":"nl-ut"}`, `""` none | `channel do {"action":"region","channel":ID,"region":"nl-ut"}`, `""` default, `"*"` no region | `regions find`, `regions` |

A channel link carries its region in the `region_scope` parameter (MeshCore App 1.47+ format): joining by such a
link or QR code sets the channel's region at once. "No region" is not put into links.

Storage: the default region in the NVS keys `region` and `region_key` (`meshmesh`); channel regions in a record of
their own, `meshmesh-mc/ch_regions` (channel ID and region); the `channels` list keeps its format, so the channels
stay readable by older firmware.

### Region search

The device sends a zero-hop repeater discovery (control packet `0x80`, filter "repeater"), collects answers for
8 s (up to 8 repeaters; when none answers, it sends the discovery once more), then sends each in turn the anonymous "regions" request (`ANON_REQ_TYPE_REGIONS`, a
zero-hop reply); a silent repeater is asked once more. A repeater answers with the regions it floods; `*` is left out of the choice. Up to 12 names in
all. A search takes from 35 s (one repeater) to a minute; late answers count for 30 s more: a busy repeater queues its answer.

Only repeaters that hear the device directly answer. A stock repeater answers at most 4 anonymous requests per
3 minutes from all nodes together, so frequent repeated searches return an empty list. Repeaters are not added to the
contacts: an answer is opened with the key from the discovery answer, so the search works with a full contact table.

## Setting up regions on a repeater

The device itself always runs as a chat node; regions are configured on the repeaters, with the `region` CLI
commands (MeshCore 1.10+; `region def` since 1.16). From the M9 they can be sent after an admin login to the repeater: on the screen (remote
admin), on the web page, or over USB as `remote login NODE_ID password`, then `remote cli NODE_ID <command>`. The
MeshCore app's remote admin works too.

An example scheme: country `nl` → province `nl-ut` (Utrecht) → city `nl-ut-utrecht`. A repeater in the city of
Utrecht:

```
region allowf *
region def nl nl-ut nl-ut-utrecht
region
region save
```

`region` afterwards prints:

```
*^ F
 nl F
  nl-ut F
   nl-ut-utrecht F
```

The same step by step (1.15 and older syntax):

```
region allowf *
region put nl
region put nl-ut nl
region put nl-ut-utrecht nl-ut
region allowf nl
region allowf nl-ut
region allowf nl-ut-utrecht
region save
```

A wrong region is removed with `region remove name`, children first. Give a repeater only the regions where it
stands: a node on a border may have two, a node with all regions makes one big network again.

## Checking

`tools/region_check.py --port PORT` checks over USB: flood frames are taken from `txframe` and the region code is
recomputed independently in Python — an advert with the default region (`TRANSPORT_FLOOD`), a channel with its own
region, "no region" and "default", an invalid name refused, the companion commands 54/55/63/64. A repeater
forwarding or refusing a scoped packet needs a second, out-of-range node and has to be checked in the field.
