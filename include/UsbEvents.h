#pragma once
#include "Notifications.h"
// USB events (docs/usb-events.md): with "events on" the board writes one line per mesh event to USB,
// "EVENT {json}", so a program on the computer can act on messages without polling. Off by default.
namespace usbEvents {
void push(notifications::Kind kind, const ChatMessage& message); // from notifications::emit
void node(uint64_t id, const char* name, float snr, int rssi); // a node advert heard
bool next(String& line); // the oldest waiting line; false: none
uint32_t dropped(); // lines lost because the computer did not read them in time
} // namespace usbEvents
