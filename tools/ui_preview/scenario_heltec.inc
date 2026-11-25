// Screens of the Heltec V4 (src/UiHeltec.cpp) rendered by the preview: one PRG button, 13 a click, 0xa3 a hold.
// 0x86 (home) is not a key of the board: the checks use it to start from the home screen.
static void scenario() {
  auto expect = [](const char* what, const char* part) {
    if (uiStatus().indexOf(part) < 0) {
      printf("FAIL %s: %s\n", what, uiStatus().c_str());
      exit(1);
    }
  };
  auto has = [](const char* part) { return uiStatus().indexOf(part) >= 0; };
  // A menu still open times out first.
  auto closeMenu = [&] {
    for (int i = 0; i < 11 && has("\"menu\":true"); i++) tick();
  };
  auto home = [&] {
    closeMenu();
    for (int i = 0; i < 4 && !has("\"page\":\"home\""); i++) key(0x86);
  };
  // Each screen is reached by name: clicks step through the screens (quick send stays out of the way).
  auto go = [&](const char* n) {
    char savedApps[sizeof config.apps];
    strlcpy(savedApps, config.apps, sizeof savedApps);
    strlcpy(config.apps, "-quick", sizeof config.apps);
    std::string want = std::string("\"page\":\"") + n + "\"";
    closeMenu();
    for (int i = 0; i < 60 && !has(want.c_str()); i++) key(13);
    expect(n, want.c_str());
    strlcpy(config.apps, savedApps, sizeof config.apps);
  };
  for (const char* n : {"home", "messages", "nodes", "radar", "gps", "wifi", "ble", "settings", "modules"}) {
    go(n);
    shot((std::string("heltec-") + n).c_str());
    if (!strcmp(n, "radar")) {
      key(0xa3);
      expect("radar menu", "\"menu\":true");
      shot("heltec-radar-menu");
      key(13);
      key(13);
      key(0xa3); // Home in
      for (int i = 0; i < 4; i++) tick();
      shot("heltec-homing");
      key(0xa3);
      key(0xa3); // Stop homing
    }
    if (!strcmp(n, "messages")) {
      key(0xa3);
      expect("messages menu", "\"menu\":true");
      shot("heltec-messages-menu");
      closeMenu();
    }
    if (!strcmp(n, "nodes")) {
      key(0xa3);
      expect("nodes menu", "\"menu\":true");
      shot("heltec-nodes-menu");
      closeMenu();
    }
    if (!strcmp(n, "wifi")) {
      key(0xa3);
      shot("heltec-wifi-on");
      key(0xa3);
    }
    if (!strcmp(n, "ble")) {
      for (int i = 0; i < 2 && !bleActive(); i++) key(0xa3);
      if (!bleActive()) {
        printf("FAIL BLE on\n");
        exit(1);
      }
      shot("heltec-ble-on"); // the MeshCore- name and the PIN
      for (int i = 0; i < 2 && bleActive(); i++) key(0xa3);
    }
  }
  // A message arrives: its popup, emoji included; a click closes it.
  {
    ChatMessage m;
    m.source = 0xA1B2C3D40000ULL;
    m.destination = meshRadio.nodeId;
    strcpy(m.name, "Heltec V4");
    strcpy(m.text, "Can you hear me? 😀 See you at the spot in 5 minutes 👍❤️🔥");
    m.timestamp = time(nullptr);
    m.id = 77;
    meshRadio.history[meshRadio.historyCount++] = m;
    notifications::incoming(m);
  }
  tick();
  expect("message popup", "\"popup\":true");
  shot("heltec-popup");
  key(13);
  expect("a click closes the popup", "\"popup\":false");
  // An invitation to a channel: its card in Messages, "Join" first in the menu; a channel message names its channel.
  {
    channels::Channel c{};
    strcpy(c.name, "Fishing");
    for (int i = 0; i < 16; i++) c.secret[i] = uint8_t(i * 29 + 3);
    ChatMessage m;
    m.source = 0xA1B2C3D40000ULL;
    m.destination = meshRadio.nodeId;
    strcpy(m.name, "Heltec V4");
    strcpy(m.text, channels::link(c).c_str());
    m.timestamp = time(nullptr);
    m.id = 78;
    meshRadio.history[meshRadio.historyCount++] = m;
  }
  home();
  key(13);
  expect("messages", "\"page\":\"messages\"");
  shot("heltec-invite");
  key(0xa3);
  shot("heltec-invite-menu");
  key(0xa3);
  if (strcmp(meshRadio.channelList[meshRadio.channelCount - 1].name, "Fishing")) {
    printf("FAIL heltec invitation not joined\n");
    exit(1);
  }
  {
    ChatMessage m;
    m.source = 0xA1B2C3D45555ULL;
    m.destination = meshRadio.channelList[1].id;
    strcpy(m.name, "Marat");
    strcpy(m.text, "Is the ice on the lake holding?");
    m.timestamp = time(nullptr);
    m.id = 79;
    meshRadio.history[meshRadio.historyCount++] = m;
  }
  home();
  key(13);
  shot("heltec-channel-message");
  // Storage left by another firmware: "Create storage..." heads the home menu, a second hold formats.
  hardware.fsOk = false;
  home();
  previewCommands = "";
  key(0xa3);
  shot("heltec-format-menu");
  key(0xa3);
  shot("heltec-format-armed");
  key(0xa3);
  if (previewCommands != "fsformat;restart;") {
    printf("FAIL create storage: %s\n", previewCommands.c_str());
    exit(1);
  }
  hardware.fsOk = true;
  // Radio settings with one button: hold on "Radio...", the frequency digit by digit (click +1, hold next), SF, Save.
  go("settings");
  key(0xa3);
  expect("settings menu", "\"menu\":true");
  shot("heltec-settings-menu");
  key(0xa3);
  expect("radio screen", "\"page\":\"radio\"");
  shot("heltec-radio");
  key(0xa3);
  key(13);
  expect("MHz +1", "\"radio_draft\":\"869.731\"");
  key(0xa3);
  key(0xa3);
  key(13);
  key(13);
  expect("10 kHz +2", "\"radio_draft\":\"869.751\"");
  shot("heltec-radio-freq");
  key(0xa3);
  key(0xa3);
  expect("digits done", "\"radio_editing\":false");
  key(13);
  key(13);
  key(0xa3);
  key(13);
  expect("SF9", "\"radio_draft\":\"9\"");
  shot("heltec-radio-sf");
  key(0xa3);
  for (int i = 0; i < 7; i++) key(13);
  expect("Find row", "\"radio_row\":9");
  key(0xa3);
  shot("heltec-radio-find");
  for (int i = 0; i < 13; i++) key(13);
  expect("Region row", "\"radio_row\":8");
  key(0xa3);
  key(13);
  key(13);
  key(13);
  key(0xa3);
  shot("heltec-radio-region");
  for (int i = 0; i < 2; i++) key(13);
  expect("Previous row", "\"radio_row\":10");
  shot("heltec-radio-prev");
  key(13);
  expect("Network row", "\"radio_row\":11");
  key(0xa3); // the network search over the regional presets
  shot("heltec-radio-net");
  key(13);
  expect("Save row", "\"radio_row\":12");
  shot("heltec-radio-save");
  key(0xa3);
  expect("saved and closed", "\"page\":\"settings\"");
  // Apps in the chosen order ("apps" setting): hidden ones get no click.
  {
    auto page = [&](const char* want) {
      std::string w = std::string("\"page\":\"") + want + "\"";
      expect("apps order", w.c_str());
    };
    strlcpy(config.apps, "settings -chats -nodes -quick", sizeof config.apps);
    home();
    page("home");
    key(13);
    page("settings");
    key(13);
    page("gps"); // the apps the setting does not name follow in the default order
    for (int i = 0; i < 14 && !has("\"page\":\"home\""); i++) {
      key(13);
      if (has("\"page\":\"messages\"") || has("\"page\":\"nodes\"")) {
        printf("FAIL hidden page shown\n");
        exit(1);
      }
    }
    page("home");
    config.apps[0] = 0;
    printf("apps order checks passed\n");
  }
  // Double click = back (config.doubleBack): a click waits for a second one.
  {
    auto must = [](bool ok, const char* why) {
      if (!ok) {
        printf("FAIL double click %s: %s\n", why, uiStatus().c_str());
        exit(1);
      }
    };
    auto twice = [] {
      uiKey(13);
      fakeMillis += 200;
      uiKey(13);
      tick();
    };
    strlcpy(config.apps, "quick gps", sizeof config.apps);
    config.dimAfter = 0;
    home();
    twice();
    must(has("\"page\":\"gps\""), "off: two quick clicks are two clicks");
    key(0x86);
    config.doubleBack = true;
    uiKey(13);
    must(has("\"page\":\"home\"") && has("\"click_wait\":true"), "a click waits for a second one");
    tick();
    must(has("\"page\":\"quick\"") && has("\"click_wait\":false"), "a single click acts after the wait");
    key(0xa3);
    key(0xa3);
    must(has("\"app_open\":true") && has("\"quick_targets\":true"), "hold opens Quick Send and its recipients");
    twice();
    must(has("\"quick_targets\":false") && has("\"app_open\":true"), "back closes the recipients");
    twice();
    must(has("\"page\":\"quick\"") && has("\"app_open\":false"), "back closes the app");
    twice();
    must(has("\"page\":\"home\""), "back: the previous page");
    uiKey(13);
    fakeMillis += 200;
    uiKey(0xa3);
    tick();
    must(has("\"page\":\"quick\"") && has("\"app_open\":true"), "a hold after a click: the click first");
    config.doubleBack = false;
    key(0x86);
    config.apps[0] = 0;
    go("settings");
    key(0xa3);
    for (int i = 0; i < 5; i++) key(13);
    shot("heltec-settings-double");
    key(0xa3);
    must(config.doubleBack && has("\"menu\":true"), "the settings menu switches it on");
    shot("heltec-settings-double-on");
    key(0xa3);
    must(!config.doubleBack, "and off");
    closeMenu();
    key(0x86);
    printf("double click checks passed\n");
  }
  // Quick send: entered with a hold, a recipient chosen on its card, the presets sent with a hold.
  {
    auto must = [](bool ok, const char* why) {
      if (!ok) {
        printf("FAIL quick send UI %s: %s\n", why, uiStatus().c_str());
        exit(1);
      }
    };
    const int step = 13, hold = 0xa3, choose = 13;
    strlcpy(config.apps, "quick gps", sizeof config.apps);
    key(0x86);
    key(step);
    must(has("\"page\":\"quick\"") && has("\"app_open\":false"), "Quick Send waits for a hold");
    shot("heltec-quick-closed");
    key(step);
    must(has("\"page\":\"gps\""), "click passes Quick Send");
    key(0x86);
    key(step);
    key(hold);
    must(has("\"page\":\"quick\"") && has("\"quick_row\":0"), "open Quick Send on recipient card");
    shot("heltec-quick");
    unsigned sent = meshRadio.historyCount;
    key(hold);
    must(has("\"quick_targets\":true") && meshRadio.historyCount == sent,
         "recipient card opens chooser without sending");
    shot("heltec-quick-targets");
    key(0x86);
    must(has("\"page\":\"quick\"") && has("\"quick_row\":0"), "cancel returns to recipient card");
    key(hold);
    key(choose);
    uint64_t to = quickSend::target(1);
    key(hold);
    must(quickSend::state.to == to && has("\"quick_index\":0") && meshRadio.historyCount == sent,
         "recipient confirmation selects first preset without sending");
    for (int i = 0; i < 4; i++) tick();
    shot("heltec-quick-preset");
    key(hold);
    must(meshRadio.historyCount == sent + 1 && String(meshRadio.history[sent].text) == quickSend::state.text[0] &&
             meshRadio.history[sent].destination == to,
         "first preset sends its own text to the chosen recipient");
    for (unsigned i = 1; i < quickSend::Count; i++) key(choose);
    for (int i = 0; i < 4; i++) tick();
    shot("heltec-quick-last");
    key(hold);
    must(meshRadio.historyCount == sent + 2 &&
             String(meshRadio.history[sent + 1].text) == quickSend::state.text[quickSend::Count - 1] &&
             meshRadio.history[sent + 1].destination == to,
         "last preset remains reachable and sends its own text");
    key(13);
    key(hold);
    must(has("\"page\":\"gps\""), "Exit advances to the next app");
    config.apps[0] = 0;
    printf("quick send UI checks passed\n");
  }
  // Wardrive: off, then listening and pings along a street, the repeaters heard.
  {
    auto must = [](bool ok, const char* why) {
      if (!ok) {
        printf("FAIL wardrive UI %s: %s\n", why, uiStatus().c_str());
        exit(1);
      }
    };
    strlcpy(config.apps, "wardrive", sizeof config.apps);
    home();
    key(13);
    must(has("\"page\":\"wardrive\""), "open wardrive");
    shot("heltec-wardrive-off");
    key(0xa3);
    shot("heltec-wardrive-menu");
    key(0xa3);
    must(wardrive::running(), "menu starts it");
    wardriveData();
    tick();
    shot("heltec-wardrive");
    key(0xa3);
    key(13);
    key(13);
    key(0xa3);
    shot("heltec-wardrive-repeaters");
    key(0xa3);
    key(13);
    key(13);
    key(0xa3);
    key(0x86);
    {
      StaticJsonDocument<64> d;
      d["action"] = "stop";
      wardrive::command(d.as<JsonObjectConst>());
    }
    config.apps[0] = 0;
    printf("wardrive UI checks passed\n");
  }
  // Power off last: the screens stop taking keys once it is requested.
  powerOff();
  shot("heltec-poweroff");
  uiFarewell();
  shot("heltec-poweroff-done");
}
