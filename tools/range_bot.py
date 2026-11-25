#!/usr/bin/env python3
"""Range test: a board on USB answers every message on one channel with how it heard it, and maps the results.

Walk around with a second board and send messages on the channel with your GPS position (Quick Send with GPS
on adds " @lat,lon"). For each message this program:
  - replies on the channel, e.g. "#3 heard: SNR 6.5 dB, RSSI -98 dBm, direct, 1.24 km";
  - appends the event to LOG (JSON lines) and redraws MAP (an HTML page with the points coloured by SNR).
Lines appended to OUTBOX are sent on the channel too (a way for someone at the computer to answer).

The board must run firmware with USB events ("events on", docs/usb-events.md). LOG and MAP hold positions:
keep them out of git (logs/ is ignored).

  .venv/bin/python tools/range_bot.py --channel ID [--home LAT,LON] [--port PORT]
"""
import argparse, json, math, os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import device
from ports import HELTEC_PORT

POSITION = re.compile(r'@(-?\d{1,2}\.\d+),\s*(-?\d{1,3}\.\d+)')


def km(a, b):
    lat1, lon1, lat2, lon2 = map(math.radians, (*a, *b))
    h = math.sin((lat2 - lat1) / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin((lon2 - lon1) / 2) ** 2
    return 6371 * 2 * math.asin(math.sqrt(h))


def route_text(e):
    hops = e.get('hops')
    if hops is None: return e.get('route', '?')
    return 'direct' if hops == 0 else f'via {hops} repeater' + ('s' if hops > 1 else '')


MAP_PAGE = """<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Range test</title>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<style>html,body,#map{height:100%;margin:0;font:14px system-ui,sans-serif}
.legend{background:#fff;padding:6px 8px;border-radius:6px;line-height:1.5}.legend i{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px}</style>
</head><body><div id="map"></div><script>
const home=__HOME__, points=__POINTS__;
const map=L.map('map').setView(home,13);
L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'&copy; OpenStreetMap'}).addTo(map);
L.marker(home).addTo(map).bindPopup('Base station');
const colour=s=>s>=5?'#1a9850':s>=0?'#91cf60':s>=-5?'#fee08b':s>=-10?'#fc8d59':'#d73027';
for(const p of points){L.circleMarker([p.lat,p.lon],{radius:8,color:'#333',weight:1,fillColor:colour(p.snr),fillOpacity:.9}).addTo(map)
 .bindPopup(`#${p.n} ${p.time}<br>SNR ${p.snr} dB, RSSI ${p.rssi} dBm<br>${p.route}, ${p.km.toFixed(2)} km<br>${p.text}`)}
if(points.length)map.fitBounds(L.latLngBounds([home,...points.map(p=>[p.lat,p.lon])]).pad(.2));
const legend=L.control({position:'bottomright'});legend.onAdd=()=>{const d=L.DomUtil.create('div','legend');
 d.innerHTML='<b>SNR</b><br>'+[[5,'≥ 5 dB'],[0,'0 … 5'],[-5,'−5 … 0'],[-10,'−10 … −5'],[-99,'< −10']].map(([s,t])=>`<i style="background:${colour(s)}"></i>${t}`).join('<br>');return d};legend.addTo(map);
</script></body></html>
"""


def write_map(path, home, points):
    page = MAP_PAGE.replace('__HOME__', json.dumps(list(home))).replace('__POINTS__', json.dumps(points))
    tmp = path + '.tmp'
    with open(tmp, 'w') as f: f.write(page)
    os.replace(tmp, path)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--port', default=HELTEC_PORT)
    ap.add_argument('--channel', required=True, help='channel ID as the "channels" command lists it')
    ap.add_argument('--home', help='LAT,LON of the board on USB (default: the first position received)')
    ap.add_argument('--log', default=os.path.join(root, 'logs', 'range-test.jsonl'))
    ap.add_argument('--map', default=os.path.join(root, 'logs', 'range-test.html'))
    ap.add_argument('--outbox', default=os.path.join(root, 'logs', 'range-test.outbox'))
    ap.add_argument('--no-reply', action='store_true', help='log and map only')
    a = ap.parse_args()
    home = tuple(float(x) for x in a.home.split(',')) if a.home else None
    os.makedirs(os.path.dirname(a.log), exist_ok=True)
    old = os.umask(0o077)
    open(a.outbox, 'a').close()
    os.umask(old)

    points, n = [], 0
    if os.path.exists(a.log): # a restart continues the numbering and the map
        for line in open(a.log):
            try: e = json.loads(line)
            except ValueError: continue
            if e.get('type') == 'message' and e.get('to') == a.channel:
                n += 1
                if 'lat' in e:
                    points.append(e['point'])
                    home = home or (e['lat'], e['lon'])
    if home: write_map(a.map, home, points)

    s = device.connect(a.port)
    print(device.command(s, 'events on'), flush=True)
    s.timeout = 0.2
    buf = b''
    sending = []

    def send(text):
        sending.append(f'send {a.channel} {text}')

    print(f'range test on channel {a.channel}: log {a.log}, map {a.map}', flush=True)
    while True:
        if sending and not buf:
            s.write((sending.pop(0) + '\n').encode()); s.flush()
        try:
            with open(a.outbox, 'r+') as f:
                lines = [l.strip() for l in f if l.strip()]
                if lines: f.seek(0); f.truncate()
            for l in lines:
                send(l); print(f'>> {l}', flush=True)
        except OSError:
            pass
        buf += s.read(4096)
        while b'\n' in buf:
            raw, buf = buf.split(b'\n', 1)
            line = raw.decode('utf-8', 'replace').strip()
            if not line.startswith('EVENT '): continue
            try: e = json.loads(line[6:])
            except ValueError: continue
            e['at'] = time.strftime('%Y-%m-%d %H:%M:%S')
            if e.get('type') == 'message' and e.get('to') == a.channel:
                n += 1
                text = e.get('text', '')
                # Channel messages arrive as "name: text"; the position is the sender's " @lat,lon".
                m = POSITION.search(text)
                reply = f"#{n} heard: SNR {e.get('snr', '?')} dB, RSSI {e.get('rssi', '?')} dBm, {route_text(e)}"
                if m:
                    pos = (float(m.group(1)), float(m.group(2)))
                    home = home or pos
                    dist = km(home, pos)
                    reply += f', {dist:.2f} km'
                    e.update(lat=pos[0], lon=pos[1], km=round(dist, 3))
                    e['point'] = dict(n=n, lat=pos[0], lon=pos[1], snr=e.get('snr'), rssi=e.get('rssi'),
                                      route=route_text(e), km=dist, time=e['at'][11:], text=text)
                    points.append(e['point'])
                    write_map(a.map, home, points)
                print(f"<< #{n} {e['at'][11:]} {text!r} snr={e.get('snr')} rssi={e.get('rssi')} {route_text(e)}"
                      + (f" {e['km']} km" if 'km' in e else ' (no position)'), flush=True)
                if not a.no_reply: send(reply)
            elif e.get('type') in ('sent', 'failed') and e.get('to') == a.channel:
                print(f"   {e['type']}: {e.get('text', '')!r}", flush=True)
            with open(a.log, 'a') as f: f.write(json.dumps(e, ensure_ascii=False) + '\n')


if __name__ == '__main__':
    try: main()
    except KeyboardInterrupt: pass
