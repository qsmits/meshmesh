// Network search (include/NetFind.h), included at the end of MeshRadio.cpp after RegionSearch.inc: the regional
// presets (RadioPresets.h) one by one, a zero-hop node discovery for repeaters on each.
#include "RadioPresets.h"
struct NetFindAccess {
  static MeshCoreBackend* core() { return meshRadio.core; }
};
namespace netfind {
State state;
namespace {
// A stock repeater answers 4 discoveries per 2 minutes with a random delay of up to a few seconds.
constexpr uint32_t ListenMs = 8000, SettleMs = 200;
enum Stage : uint8_t { Tune, Send, Listen, Restore } stage = Tune;
uint32_t due = 0, tag = 0, rxBefore = 0;
MeshCoreBackend* core() {
  return NetFindAccess::core();
}
RadioProfile profileOf(const radiopresets::Preset& p) {
  RadioProfile r;
  r.frequency = p.frequency;
  r.bandwidth = p.bandwidth;
  r.sf = p.sf;
  r.cr = p.cr;
  return r;
}
// One frequency, bandwidth and SF hear each other at any CR: the LoRa header carries it.
bool hears(const RadioProfile& a, const RadioProfile& b) {
  return fabsf(a.frequency - b.frequency) < .0005f && a.bandwidth == b.bandwidth && a.sf == b.sf;
}
int placeOf(const RadioProfile& r) {
  for (int i = 0; i < radiopresets::Count; i++)
    if (hears(profileOf(radiopresets::list[i]), r)) return i;
  return 255;
}
void add(const RadioProfile& r, uint8_t place) {
  if (state.count >= MaxResults) return;
  for (unsigned i = 0; i < state.count; i++)
    if (hears(state.results[i].radio, r)) return;
  auto& x = state.results[state.count++];
  x = Result();
  x.radio = r;
  x.place = place;
}
bool discover() {
  uint8_t data[10] = {0x80, uint8_t(1 << ADV_TYPE_REPEATER)};
  coreRandom.random(data + 2, 4);
  memcpy(&tag, data + 2, 4); // since: 0, every repeater
  if (!tag) tag = 1, memcpy(data + 2, &tag, 4);
  mesh::Packet* p = core()->createControlData(data, sizeof(data));
  if (!p) {
    tag = 0;
    return false;
  }
  core()->sendZeroHop(p);
  return true;
}
uint32_t score(const Result& r) {
  return r.repeaters * 1000u + min<uint16_t>(r.packets, 999);
}
void finish() {
  tag = 0;
  state.phase = State::Done;
  state.best = 255;
  uint32_t mine = 0, top = 0;
  for (unsigned i = 0; i < state.count; i++)
    if (state.results[i].current) mine = score(state.results[i]);
  for (unsigned i = 0; i < state.count; i++) {
    auto& r = state.results[i];
    if (!r.current && r.done && score(r) > max(top, mine)) {
      top = score(r);
      state.best = i;
    }
  }
  if (state.best != 255) {
    const auto& r = state.results[state.best];
    meshRadio.event = "Network found: " + profileText(r.radio) + " (" + String(r.repeaters) + " repeaters)";
  } else
    meshRadio.event = "Network search: nothing new";
  meshRadio.dirty = true;
}
void next() {
  tag = 0;
  if (++state.step >= state.count)
    stage = Restore;
  else
    stage = Tune;
}
} // namespace
String profileText(const RadioProfile& r) {
  return String(r.frequency, 3) + " SF" + String(r.sf) + " BW" + String(r.bandwidth, 1);
}
String placeName(uint8_t place) {
  return place < radiopresets::Count ? String(radiopresets::list[place].name) : String();
}
bool start() {
  if (!core() || !meshRadio.ready || !config.bootCounter) return false;
  if (running()) return true;
  if (regions::searching() || meshRadio.busy()) return false;
  if (!state.results && !(state.results = new (std::nothrow) Result[MaxResults])) return false;
  Result* list = state.results;
  state = State();
  state.results = list;
  add(config.radio(), placeOf(config.radio()));
  state.results[0].current = true;
  for (int i = 0; i < radiopresets::Count; i++)
    if (radiopresets::usable(config, i)) add(profileOf(radiopresets::list[i]), i); // add() skips a profile already in
  state.phase = State::Running;
  stage = Send;
  due = millis();
  tag = 0; // the first one is the current profile: no tuning
  meshRadio.event = "Network search: " + String(state.count) + " profiles";
  meshRadio.dirty = true;
  return true;
}
void stop() {
  if (running()) {
    state.step = state.count;
    stage = Restore;
  }
}
void cancel() {
  if (running()) finish();
}
void tick() {
  uint32_t now = millis();
  if (running()) {
    if (stage == Restore) {
      if (meshRadio.busy()) return;
      meshRadio.tune(RadioProfile());
      finish();
      return;
    }
    auto& r = state.results[state.step];
    if (stage == Tune) {
      if (meshRadio.busy()) return;
      if (!meshRadio.tune(r.radio)) {
        next();
        return;
      }
      stage = Send;
      due = now + SettleMs;
      return;
    }
    if (stage == Send) {
      if (int32_t(now - due) < 0 || meshRadio.busy()) return;
      rxBefore = meshRadio.rxCount;
      if (!discover()) {
        next();
        return;
      }
      stage = Listen;
      due = now + ListenMs;
      meshRadio.dirty = true;
      return;
    }
    if (int32_t(now - due) < 0) return;
    uint32_t heard = meshRadio.rxCount - rxBefore;
    r.packets = heard > r.repeaters ? min<uint32_t>(heard - r.repeaters, 65535) : 0;
    r.done = true;
    next();
    meshRadio.dirty = true;
    return;
  }
}
// The answers to our discovery: the repeaters on the candidate profile.
bool onControl(const mesh::Packet* p) {
  if ((p->payload[0] & 0xf0) != 0x90 || !tag || p->payload_len < 6) return false;
  uint32_t t;
  memcpy(&t, p->payload + 2, 4);
  if (t != tag) return false;
  if (running() && stage == Listen && (p->payload[0] & 0x0f) == ADV_TYPE_REPEATER) {
    auto& r = state.results[state.step];
    if (r.repeaters < 255) r.repeaters++;
    r.snr = max<int>(r.snr, constrain(lroundf(meshRadio.lastSnr * 4), -127, 127));
    meshRadio.dirty = true;
  }
  return true;
}
String use(uint8_t i) {
  if (!state.results || i >= state.count || !state.results[i].done) return "ERR netfind: no such result";
  if (running()) return "ERR netfind: search running";
  if (config.sameRadio(state.results[i].radio)) return "ERR netfind: already on it";
  const auto& r = state.results[i].radio;
  StaticJsonDocument<128> j;
  j["frequency"] = r.frequency;
  j["bandwidth"] = r.bandwidth;
  j["sf"] = r.sf;
  j["cr"] = r.cr;
  String s;
  serializeJson(j, s);
  return executeCommand("radio try " + s);
}
String json() {
  DynamicJsonDocument d(768 + state.count * 176);
  d["state"] = state.phase == State::Running ? "running" : state.phase == State::Done ? "done" : "idle";
  d["step"] = state.step;
  d["total"] = state.count;
  d["best"] = state.best == 255 ? -1 : int(state.best);
  if (running() && state.step < state.count) {
    d["now"] = profileText(state.results[state.step].radio);
  }
  JsonArray a = d.createNestedArray("results");
  for (unsigned i = 0; i < state.count; i++) {
    const auto& r = state.results[i];
    JsonObject o = a.createNestedObject();
    o["frequency"] = r.radio.frequency;
    o["bandwidth"] = r.radio.bandwidth;
    o["sf"] = r.radio.sf;
    o["cr"] = r.radio.cr;
    if (r.place != 255) o["place"] = radiopresets::list[r.place].name;
    if (r.current) o["current"] = true;
    o["done"] = r.done;
    o["repeaters"] = r.repeaters;
    o["packets"] = r.packets;
    if (r.snr != -128) o["snr"] = r.snr / 4.0f;
  }
  String s;
  serializeJson(d, s);
  return s;
}
} // namespace netfind
