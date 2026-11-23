// Group channels on the 320x240 screen (included by Ui.cpp): joining by hashtag, by name and key,
// creating a private one, joining one heard on air or invited to, and a channel card with its QR code.
Page infoReturn = Threads;
enum AddStep { AddNone, AddTag, AddName, AddKey, AddCreate } addStep = AddNone;
String addName;
int inviteTo = 0;
String regionChoice;
uint64_t regionFor = 0; // the region on the card's row (of channel regionFor): <> picks, OK saves or types
bool privateChannel(const channels::Channel& c) {
  return !channels::isPublic(c.secret) && !channels::isHashtag(c);
}
String channelKind(const channels::Channel& c) {
  return channels::isPublic(c.secret) ? t("Public channel of MeshCore")
         : privateChannel(c)          ? t("Private channel")
                                      : t("Hashtag channel");
}
String resultText(MeshRadio::ChannelResult r) {
  switch (r) {
  case MeshRadio::ChannelAdded: return t("Channel added");
  case MeshRadio::ChannelExists: return t("Already in this channel");
  case MeshRadio::ChannelFull: return t("8 channels at most");
  case MeshRadio::ChannelBadName: return t("Name: 1-31 bytes");
  case MeshRadio::ChannelBadKey: return t("Key: 32 hex or base64");
  case MeshRadio::ChannelBadLink: return t("Not a channel link");
  case MeshRadio::ChannelUnavailable: return t("Not in this mode");
  default: return t("Channel not saved");
  }
}
void openChannelInfo(uint64_t id, Page from) {
  focusChannel = id;
  infoReturn = from;
  inviteTo = 0;
  regionFor = 0;
  change(ChannelInfo);
}
// After joining: the chat of the channel; a created one opens its card, whose QR the others scan.
void joined(MeshRadio::ChannelResult r, uint64_t id, bool card = false) {
  bool in = r == MeshRadio::ChannelAdded || r == MeshRadio::ChannelExists;
  notice(resultText(r), in ? ok : bad);
  if (!in) return;
  editing = false;
  addStep = AddNone;
  if (page == ChannelAdd) focusedThread = id;
  if (card) {
    openChannelInfo(id, Threads);
    return;
  }
  recipient = id;
  chatReturn = Threads;
  composer = "";
  change(Chat);
}
// The newest channel invitation in the open chat.
bool chatInvite(String& name, uint8_t key[16]) {
  for (int i = int(meshRadio.historyCount) - 1; i >= 0; i--)
    if (belongs(meshRadio.history[i], recipient) && channels::parseLink(meshRadio.history[i].text, name, key))
      return true;
  return false;
}
// Left in a chat: join the channel of an invitation in it; otherwise the card of this channel, or the
// channel an invitation of this direct chat leads to.
String chatLeftAction() {
  String name;
  uint8_t key[16];
  bool invite = chatInvite(name, key), in = invite && meshRadio.channel(channels::idOf(key));
  if (invite && !in) return t("Join");
  if (channels::isChannel(recipient)) return t("Channel");
  return invite ? t("Open channel") : "";
}
void chatLeft() {
  String name;
  uint8_t key[16];
  bool invite = chatInvite(name, key), in = invite && meshRadio.channel(channels::idOf(key));
  if (channels::isChannel(recipient) && (!invite || in)) {
    openChannelInfo(recipient, Chat);
    return;
  }
  if (!invite) return;
  uint64_t id = 0;
  auto result = meshRadio.addChannel(channels::isPublic(key) ? String("Public") : name, key, &id);
  joined(result, id);
}
// A link in a bubble reads as an invitation.
String bubbleText(const ChatMessage& m) {
  String name;
  uint8_t key[16];
  if (!channels::parseLink(m.text, name, key)) return m.text;
  bool in = meshRadio.channel(channels::idOf(key)) != nullptr;
  return t("Channel invitation: ") + name + "\n" + (in ? t("you are in it") : t("<- join"));
}

// Add channel: three ways, then the channels heard on air that this node has not joined.
const int addFixed = 3;
int addRows() {
  return addFixed + meshRadio.heardCount;
}
void startEdit(AddStep step, const String& initial) {
  addStep = step;
  edit = initial;
  editing = true;
}
void drawChannelAdd() {
  int total = addRows();
  selected = constrain(selected, 0, total - 1);
  int first = max(0, selected - 3);
  bool full = meshRadio.channelCount >= channels::Max;
  for (int i = first; i < total && i < first + 4; i++) {
    int y = 24 + (i - first) * 48;
    bool focus = selected == i;
    listRow(y, 45, focus);
    target(8, y, 304, 45, i);
    String name, detail;
    Icon ic = IcHash;
    uint16_t hue = accent;
    if (i == 0) {
      name = t("Join by hashtag");
      detail = t("Open channel: its key comes from the #name");
    } else if (i == 1) {
      ic = IcKey;
      hue = warn;
      name = t("Name and key");
      detail = t("Key from another app: 32 hex or base64");
    } else if (i == 2) {
      ic = IcLock;
      hue = violet;
      name = t("Create a private channel");
      detail = t("Random key; its QR invites the others");
    } else {
      const auto& h = meshRadio.heard[i - addFixed];
      ic = IcRadio;
      hue = h.name[0] ? ok : faint;
      char hash[3];
      snprintf(hash, 3, "%02X", h.hash);
      String heard = count(h.packets, "packet", "packets") + " · " + ago(millis() - h.at);
      if (h.name[0]) {
        name = h.name;
        detail = t("Heard on air: ") + heard + t(" · OK: join");
      } else {
        name = t("Unknown channel ") + hash;
        detail = heard + t(" · OK: try a #name");
      }
    }
    g().fillCircle(32, y + 22, 15, card);
    icon(ic, 32, y + 22, 7, hue, card);
    text(56, y + 18, fit(name, 250, bold), ink, bold);
    text(56, y + 36, fit(i < addFixed && full ? t("Full: 8 channels, Public included") : detail, 250, small),
         i < addFixed && full ? warn : dim, small);
    if (i == addFixed) {
      g().drawFastHLine(8, y - 2, 304, line);
    }
  }
  if (total == addFixed) {
    textCenter(160, 24 + 3 * 48 + 20,
               meshRadio.heardSamples ? t("Other channels heard: none") : t("Channels heard on air will appear here"),
               faint, small);
  }
  scrollbar(first, 4, total, 24, 190);
  footer({{"OK", t("Choose")}, {"^v", t("Select")}, {"BACK", t("Chats")}});
}
void channelAddEnter() {
  if (selected == 0)
    startEdit(AddTag, "#");
  else if (selected == 1) {
    addName = "";
    startEdit(AddName, "");
  } else if (selected == 2)
    startEdit(AddCreate, "");
  else if (selected - addFixed < int(meshRadio.heardCount)) {
    const auto& h = meshRadio.heard[selected - addFixed];
    if (h.name[0]) {
      uint64_t id = 0;
      auto result = meshRadio.joinHashtag(h.name, &id);
      joined(result, id);
    } else
      startEdit(AddTag, "#");
  }
}
// The editor dialog of Ui.cpp asks these for its title, limit, hints and OK.
String channelEditTitle() {
  switch (addStep) {
  case AddTag: return t("Hashtag channel");
  case AddName: return t("Channel name");
  case AddKey: return fit(t("Key of ") + addName, 268, bold);
  default: return t("New private channel");
  }
}
unsigned channelEditLimit() {
  return addStep == AddKey ? 44 : channels::NameBytes;
}
bool channelEditRaw() {
  return addStep == AddKey;
}
String channelEditHint() {
  if (addStep == AddKey) return t("32 hex digits or base64 (16 bytes)");
  if (addStep == AddTag) {
    int n = meshRadio.heardSamples ? meshRadio.probeHashtag(edit) : 0;
    if (n > 0) return t("Heard on air: ") + count(n, "message", "messages");
    return t("Same #name, same channel as in MeshCore apps");
  }
  if (addStep == AddCreate) return t("The key is made on this device");
  return t("Then the key");
}
String channelEditOk() {
  return addStep == AddName     ? t("OK: next   BACK: cancel")
         : addStep == AddCreate ? t("OK: create   BACK: cancel")
                                : t("OK: join   BACK: cancel");
}
void channelEditSave() {
  uint64_t id = 0;
  switch (addStep) {
  case AddTag: {
    auto result = meshRadio.joinHashtag(edit, &id);
    joined(result, id);
    break;
  }
  case AddName:
    if (!channels::validName(edit)) {
      notice(resultText(MeshRadio::ChannelBadName), bad);
      break;
    }
    addName = edit;
    startEdit(AddKey, "");
    break;
  case AddKey: {
    uint8_t key[16];
    if (!channels::parseKey(edit, key)) {
      notice(resultText(MeshRadio::ChannelBadKey), bad);
      break;
    }
    auto result = meshRadio.addChannel(addName, key, &id);
    joined(result, id);
    memset(key, 0, 16);
    break;
  }
  case AddCreate: {
    auto result = meshRadio.createChannel(edit, &id);
    joined(result, id, true);
    break;
  }
  default: editing = false;
  }
}

// Channel card: QR of its link on white, name, kind, key of a private channel; its region; the beep of its new
// messages (per-channel sound, after Bas van Dijk); invite a chat contact; leave.
int chatContacts(Peer** out) {
  int n = 0;
  for (unsigned i = 0; i < meshRadio.peerCount; i++)
    if (meshRadio.peers[i].type == 1) out[n++] = &meshRadio.peers[i];
  return n;
}
enum InfoAction { InfoRegion, InfoSound, InfoInvite, InfoLeave };
int infoActions() {
  const channels::Channel* c = meshRadio.channel(focusChannel);
  return !c ? 0 : channels::isPublic(c->secret) ? 2 : 4;
} // Public: its region and sound only
String channelRegionText(const String& r) {
  if (r == regions::Unscoped) return t("None");
  if (r.length()) return r;
  return config.region[0] ? t("Default: ") + String(config.region) : String(t("Default: none"));
}
void drawChannelInfo() {
  const channels::Channel* c = meshRadio.channel(focusChannel);
  if (!c) {
    textCenter(160, 110, t("Channel removed"), dim);
    footer({{"BACK", t("Back")}});
    return;
  }
  if (regionFor != focusChannel) {
    regionFor = focusChannel;
    regionChoice = c->region;
  }
  auto& d = g();
  static uint8_t modules[channels::QrBytes];
  static String coded;
  static int size = 0;
  String link = channels::link(*c);
  if (coded != link) {
    size = channels::qr(link, modules);
    coded = link;
  }
  int s = size ? constrain(186 / (size + 6), 2, 4) : 0, side = (size + 6) * s, x0 = 8, y0 = 27;
  if (size) {
    d.fillRoundRect(x0, y0, side, side, 4, 0xffff);
    for (int y = 0; y < size; y++)
      for (int x = 0; x < size; x++)
        if (channels::qrDark(modules, size, x, y)) d.fillRect(x0 + (x + 3) * s, y0 + (y + 3) * s, s, s, 0);
  }
  int x = x0 + side + 10, w = 312 - x, y = 40;
  bool priv = privateChannel(*c);
  int actions = infoActions();
  text(x, y, fit(c->name, w, bold), ink, bold);
  y += 14;
  text(x, y, fit(channelKind(*c), w, small), priv ? violet : accent, small);
  y += 12;
  char hash[3];
  snprintf(hash, 3, "%02X", channels::hashOf(c->secret));
  text(x, y, t("Hash ") + hash, faint, small);
  y += 16;
  if (priv) {
    String key = channels::keyHex(c->secret);
    text(x, y, t("Key:"), dim, small);
    y += 10;
    for (int k = 0; k < 32; k += 16) {
      text(x, y, key.substring(k, k + 16), ink, small);
      y += 10;
    }
    y += 4;
  }
  String rows[3];
  unsigned n = wrap(channels::isPublic(c->secret) ? t("Every MeshCore node has it")
                                                  : t("Scan with MeshMesh or the MeshCore app"),
                    rows,
                    priv && actions > 1 ? 0
                    : actions > 1       ? 2
                                        : 3,
                    w, small);
  for (unsigned i = 0; i < n; i++) {
    text(x, y, rows[i], dim, small);
    y += 10;
  }
  Peer* list[24];
  int people = chatContacts(list);
  inviteTo = people ? (inviteTo % people + people) % people : 0;
  selected = constrain(selected, 0, actions - 1);
  int step = actions > 3 ? 24 : 26, bh = step - 2;
  for (int i = 0; i < actions; i++) {
    int ry = 218 - (actions - i) * step, cx = x - 2 + (w + 2) / 2;
    bool focus = selected == i, danger = i == InfoLeave;
    target(x - 2, ry, w + 2, bh, i);
    panel(x - 2, ry, w + 2, bh, focus ? (danger && deleteArmed ? bad : cardHi) : card, 7);
    if (focus) ring(x - 2, ry, w + 2, bh, 7, danger ? bad : accent);
    if (i == InfoSound) {
      textCenter(cx, ry + 9, t("Sound on new messages"), dim, small);
      textCenter(cx, ry + 19, flag(c->policy.sound), ink, small);
      if (focus) {
        tri(x + 5, ry + 14, 3, 3, accent);
        tri(x + w - 7, ry + 14, 1, 3, accent);
      }
      continue;
    }
    if (i == InfoRegion || (i == InfoInvite && people)) {
      bool region = i == InfoRegion;
      textCenter(cx, ry + 9, region ? (regionChoice == c->region ? t("Region") : t("Region · OK: save")) : t("Invite"),
                 region && regionChoice != c->region ? accent : dim, small);
      textCenter(cx, ry + 20,
                 fit(region ? channelRegionText(regionChoice) : String(list[inviteTo]->name), w - 20, small), ink,
                 small);
      if (focus) {
        tri(x + 5, ry + 15, 3, 3, accent);
        tri(x + w - 7, ry + 15, 1, 3, accent);
      }
      continue;
    }
    String label = i == InfoInvite ? t("No chat contacts") : deleteArmed ? t("Sure? OK again") : t("Leave channel");
    textCenter(cx, ry + 16, fit(label, w - 6), danger ? (focus ? ink : bad) : i == InfoLeave ? ink : dim);
  }
  if (selected == InfoRegion)
    footer({{"<>", t("Region")},
            {"OK", regionChoice == c->region ? t("Type") : t("Save")},
            {"^v", t("Select")},
            {"BACK", t("Back")}});
  else if (selected == InfoSound)
    footer({{"OK", t("Change")}, {"^v", t("Select")}, {"BACK", t("Back")}});
  else
    footer({{"^v", t("Select")}, {"<>", t("Contact")}, {"OK", t("Run")}, {"BACK", t("Back")}});
}
// The beep of a channel's new messages, saved with its other notification settings (ChannelPolicy.h).
void channelSoundToggle(const channels::Channel* c) {
  StaticJsonDocument<32> d;
  d["sound"] = c->policy.sound ? 0 : 1;
  String r = channelPolicy::set(c->id, d.as<JsonObjectConst>());
  bool saved = r.startsWith("OK");
  notice(saved ? (c->policy.sound ? t("Sound on") : t("Sound off")) : r, saved ? ok : bad);
}
void saveChannelRegion(const String& r) {
  if (meshRadio.setChannelRegion(focusChannel, r)) {
    regionChoice = meshRadio.channel(focusChannel)->region;
    notice(t("Region saved"), ok);
  } else
    notice(t("Region: letters, digits, '-'"), bad);
}
void channelInfoKey(int key) {
  int actions = infoActions();
  if (!actions) return;
  const channels::Channel* c = meshRadio.channel(focusChannel);
  if (key == KeyUp || key == KeyDown) {
    selected = (selected + (key == KeyUp ? actions - 1 : 1)) % actions;
    deleteArmed = false;
    return;
  }
  if (key == KeyLeft || key == KeyRight) {
    deleteArmed = false;
    int dir = key == KeyLeft ? -1 : 1;
    if (selected == InfoRegion) {
      regionChoice = regions::cycle(regionChoice, dir, true);
      return;
    }
    if (selected == InfoSound) {
      channelSoundToggle(c);
      return;
    }
    inviteTo += dir;
    selected = InfoInvite;
    return;
  }
  if (key != Enter && key != KeyHold) return;
  if (selected == InfoSound) {
    deleteArmed = false;
    channelSoundToggle(c);
    return;
  }
  if (selected == InfoRegion) {
    deleteArmed = false;
    if (regionChoice != c->region)
      saveChannelRegion(regionChoice);
    else {
      edit = regionChoice == regions::Unscoped ? String() : regionChoice;
      editing = true;
    }
    return;
  }
  if (selected == InfoInvite) {
    deleteArmed = false;
    Peer* list[24];
    int people = chatContacts(list);
    if (!people) {
      notice(t("Discover a chat contact first: ADV"), warn);
      return;
    }
    Peer* p = list[(inviteTo % people + people) % people];
    bool sent = meshRadio.sendInvite(p->id, focusChannel);
    notice(sent ? t("Invitation queued to ") + p->name : t("Invitation not sent"), sent ? ok : bad);
    return;
  }
  if (!deleteArmed) {
    deleteArmed = true;
    notice(t("History stays; joining again shows it"), warn);
    return;
  }
  deleteArmed = false;
  uint64_t id = focusChannel;
  String name = meshRadio.channel(id) ? meshRadio.channel(id)->name : "";
  if (meshRadio.removeChannel(id)) {
    notice(t("Left ") + name, ok);
    change(Threads, true);
  } else
    notice(t("Cannot leave while sending"), bad);
}
// The region typed on the card (the editor dialog of Ui.cpp): empty - the default region.
void channelRegionSave() {
  String r = regions::normalize(edit);
  if (edit.length() && !r.length()) {
    notice(t("Region: letters, digits, '-'"), bad);
    return;
  }
  editing = false;
  saveChannelRegion(r);
}
