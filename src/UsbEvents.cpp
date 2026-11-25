#include "UsbEvents.h"
#include "Config.h"
#include <ArduinoJson.h>
namespace usbEvents {
namespace {
constexpr unsigned Slots = 8;
String lines[Slots];
unsigned head = 0, size = 0;
uint32_t lost = 0;
void add(JsonDocument& d) {
  if (size == Slots) { // the computer is not reading: the oldest goes
    head = (head + 1) % Slots;
    size--;
    lost++;
  }
  String& line = lines[(head + size++) % Slots];
  line = "EVENT ";
  serializeJson(d, line);
}
} // namespace
void push(notifications::Kind kind, const ChatMessage& m) {
  if (!config.usbEvents) return;
  const char* type = kind == notifications::Incoming      ? "message"
                     : kind == notifications::Transmitted ? "sent"
                     : kind == notifications::Confirmed   ? "delivered"
                     : kind == notifications::Repeat      ? "repeated"
                                                          : "failed"; // Unconfirmed, TxError
  StaticJsonDocument<768> d;
  d["type"] = type;
  d["from"] = meshRadio.idText(m.source);
  d["to"] = meshRadio.idText(m.destination);
  if (const channels::Channel* c = meshRadio.channel(m.destination)) d["channel"] = c->name;
  d["name"] = m.name;
  d["text"] = m.text;
  d["time"] = m.timestamp;
  d["id"] = m.id;
  if (m.signal) {
    d["snr"] = m.snr / 4.0f; // quarter dB
    d["rssi"] = m.rssi;
  }
  if (m.route) {
    d["route"] = m.route == ChatMessage::RouteDirect ? "direct" : "flood";
    if (m.hops != 255) d["hops"] = m.hops;
  }
  if (m.heard) d["heard"] = m.heard;
  add(d);
}
void node(uint64_t id, const char* name, float snr, int rssi) {
  if (!config.usbEvents) return;
  StaticJsonDocument<256> d;
  d["type"] = "node";
  d["from"] = meshRadio.idText(id);
  d["name"] = name;
  d["snr"] = snr;
  d["rssi"] = rssi;
  add(d);
}
bool next(String& line) {
  if (!size) return false;
  line = std::move(lines[head]);
  lines[head] = String();
  head = (head + 1) % Slots;
  size--;
  return true;
}
uint32_t dropped() {
  return lost;
}
} // namespace usbEvents
