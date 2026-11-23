// Notes stay on this handheld. Key the record by the peer's ID and verify its full identity.
struct ContactMemo {
  uint8_t identity[32] = {};
  char note[121] = {}, tags[65] = {};
};
struct MemoCache {
  uint64_t id = 0;
  ContactMemo memo;
  bool readable = false;
};
MemoCache* memoCache = nullptr;
String tagFilter;
MemoCache& contactMemo(const Peer& peer) {
  if (!memoCache) memoCache = new MemoCache[24];
  auto& entry = memoCache[&peer - meshRadio.peers];
  if (entry.id != peer.id || memcmp(entry.memo.identity, peer.publicKey, 32)) {
    entry = {};
    entry.id = peer.id;
    entry.readable = handheldRead(readKey(peer.id).c_str(), &entry.memo, sizeof entry.memo);
    if (memcmp(entry.memo.identity, peer.publicKey, 32)) entry.memo = {};
    memcpy(entry.memo.identity, peer.publicKey, 32);
    entry.memo.note[sizeof entry.memo.note - 1] = entry.memo.tags[sizeof entry.memo.tags - 1] = 0;
  }
  return entry;
}
bool tagMatches(const char* tags) {
  if (!tagFilter.length()) return true;
  String list(tags);
  list.toLowerCase();
  for (unsigned start = 0; start < list.length();) {
    int end = list.indexOf(',', start);
    if (end < 0) end = list.length();
    String tag = list.substring(start, end);
    tag.trim();
    if (tag == tagFilter) return true;
    start = end + 1;
  }
  return false;
}
String memoLabel() {
  return selected ? t("Tags") : t("Note");
}
void saveMemo() {
  if (page == Nodes) {
    tagFilter = edit;
    tagFilter.trim();
    tagFilter.toLowerCase();
    editing = false;
    selected = 0;
    return;
  }
  Peer* peer = peerOf(focusNode);
  if (!peer) return;
  auto& entry = contactMemo(*peer);
  auto next = entry.memo;
  if (selected)
    strlcpy(next.tags, edit.c_str(), sizeof next.tags);
  else
    strlcpy(next.note, edit.c_str(), sizeof next.note);
  if (!entry.readable || !handheldWrite(readKey(peer->id).c_str(), &next, sizeof next)) {
    notice(t("Save failed"), bad);
    return;
  }
  entry.memo = next;
  editing = false;
}
void drawMemo() {
  Peer* peer = peerOf(focusNode);
  if (!peer) return;
  auto& memo = contactMemo(*peer).memo;
  text(12, 42, fit(peer->name, 286, bold), ink, bold);
  for (int i = 0; i < 2; i++) {
    int y = 50 + i * 80;
    listRow(y, 76, i == selected);
    target(8, y, 304, 76, i);
    text(18, y + 17, i ? t("Tags") : t("Note"), dim, small);
    String rows[4];
    unsigned n = wrap(i ? memo.tags : memo.note, rows, 4, 280);
    for (unsigned j = 0; j < n; j++) text(18, y + 31 + j * 14, rows[j], ink);
  }
  footer({{"OK", t("Edit")}, {"BACK", t("Back")}});
}
