// Desktop entry point for the M9 preview. uiKey and uiTick belong to src/Ui.cpp.
#include <SDL.h>
#include <Preferences.h>

static int deviceKey(SDL_Keycode key) {
  switch (key) {
    case SDLK_RETURN: case SDLK_KP_ENTER: return 13;
    case SDLK_BACKSPACE: case SDLK_DELETE: return 8;
    case SDLK_LEFT: return 0xb4;
    case SDLK_UP: return 0xb5;
    case SDLK_DOWN: return 0xb6;
    case SDLK_RIGHT: return 0xb7;
    case SDLK_HOME: case SDLK_F1: return 0x82;
    case SDLK_F2: return 0x81;  // Messages
    case SDLK_F3: return 0x85;  // Map
    case SDLK_F4: return 0x90;  // Settings
    case SDLK_F5: return 0x84;  // Advertise
    case SDLK_F6: return 0x87;  // GPS
    case SDLK_F7: return 0x83;  // M9 Menu / app actions
    case SDLK_F8: return 0x88;  // Lock
    case SDLK_F9: return 0xa3;  // Hold OK
    case SDLK_ESCAPE: return 0x86;
    default: return 0;
  }
}

static void handleInput(const SDL_Event& event) {
  if (event.type == SDL_KEYDOWN) {
    int key = deviceKey(event.key.keysym.sym);
    if (key) uiKey(key);
  } else if (event.type == SDL_TEXTINPUT) {
    // The M9 keyboard sends printable ASCII. The firmware handles its phonetic layout.
    for (unsigned char c : event.text.text) {
      if (!c) break;
      if (c >= 32 && c <= 126) uiKey(c);
    }
  }
}

static bool drawWindow(SDL_Renderer* renderer, SDL_Texture* texture) {
  SDL_RenderClear(renderer);
  if (!uiScreenOff()) {
    if (SDL_SetTextureColorMod(texture,screenBrightness,screenBrightness,screenBrightness) ||
        SDL_UpdateTexture(texture, nullptr, hardware.canvas->getBuffer(), 320 * sizeof(uint16_t)) ||
        SDL_RenderCopy(renderer, texture, nullptr, nullptr)) return false;
  }
  SDL_RenderPresent(renderer);
  return true;
}

static bool savedSettingsMatch(){
  return config.brightness==195 && !strcmp(config.name,"M9X") && !config.sound &&
         fabsf(config.frequency-868.732f)<0.0002f && config.previous.set() &&
         fabsf(config.previous.frequency-868.731f)<0.0002f;
}

static bool checkSimulator(SDL_Renderer* renderer, SDL_Texture* texture) {
  auto press = [](SDL_Keycode key) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    handleInput(event);
    fakeMillis += 160;
    uiTick();
  };
  auto type = [](const char* value) {
    SDL_Event event{};
    event.type=SDL_TEXTINPUT;
    SDL_strlcpy(event.text.text,value,sizeof(event.text.text));
    handleInput(event);
    fakeMillis+=160;
    uiTick();
  };
  press(SDLK_RETURN);
  auto has = [](const char* state){return uiStatus().indexOf(state)>=0;};
  auto changeLayout = [&](bool save){
    press(SDLK_F4);
    press(SDLK_DOWN);press(SDLK_RETURN); // Screen & device
    for(int i=0;i<4;i++)press(SDLK_DOWN);
    press(SDLK_RIGHT);
    if(save){
      for(int i=0;i<5;i++)press(SDLK_UP); // Wrap from Name to Save changes
      press(SDLK_RETURN);
    }else press(SDLK_ESCAPE);
    press(SDLK_F1);
  };
  if(!has("\"layout\":\"list\""))return false;
  changeLayout(false);
  if(!has("\"layout\":\"list\""))return false;
  changeLayout(true);
  if(!has("\"layout\":\"tiles\""))return false;
  press(SDLK_DOWN);
  if(!has("\"selected\":3,"))return false;
  changeLayout(true);
  press(SDLK_DOWN);
  if(!has("\"layout\":\"list\"")||!has("\"selected\":1,"))return false;
  type("@");
  if(!has("\"panel\":\"closed\""))return false;
  press(SDLK_F7); // Home > Radio controls
  if(!has("\"panel\":\"radios\""))return false;
  fakeMillis+=3600;uiTick();save(settingsFile+".radios.ppm");
  bool client=internet.enabled,ap=portalActive(),ble=bleActive(),gps=config.gps;
  press(SDLK_RETURN);
  if(internet.enabled==client)return false;
  press(SDLK_RETURN);press(SDLK_DOWN);press(SDLK_RETURN);
  if(portalActive()==ap)return false;
  press(SDLK_RETURN);press(SDLK_DOWN);press(SDLK_RETURN);
  if(bleActive()==ble)return false;
  press(SDLK_RETURN);press(SDLK_DOWN);press(SDLK_RETURN);
  if(config.gps==gps)return false;
  press(SDLK_RETURN);press(SDLK_ESCAPE);
  press(SDLK_F2);press(SDLK_F7);fakeMillis+=3600;uiTick();save(settingsFile+".actions.ppm");press(SDLK_RETURN);
  if(!has("\"page\":\"channel_add\""))return false;
  press(SDLK_ESCAPE);
  if(!has("\"page\":\"threads\""))return false;
  // All six seed conversations start unread. Cycle through channels and DMs without opening them.
  for(int row : {1,2,3,4,5,0,1}){
    type("U");
    if(!has((String("\"selected\":")+String(row)+",").c_str()))return false;
  }
  press(SDLK_UP);
  press(SDLK_RETURN);
  press(SDLK_F7);press(SDLK_DOWN);press(SDLK_DOWN);press(SDLK_RETURN);
  if(!has("\"page\":\"quick\""))return false;
  press(SDLK_ESCAPE);
  if(!has("\"page\":\"chat\""))return false;
  type("@");if(!has("\"composer\":\"@\""))return false;
  press(SDLK_BACKSPACE);
  type("a");
  if(!has("\"composer\":\"a\""))return false;
  press(SDLK_BACKSPACE);press(SDLK_F7);press(SDLK_RETURN);
  SDL_Event text{};
  text.type = SDL_TEXTINPUT;
  SDL_strlcpy(text.text.text, "Friday", sizeof(text.text.text));
  handleInput(text);
  press(SDLK_BACKSPACE);
  if (uiStatus().indexOf("\"composer\":\"Frida\"") < 0) return false;
  unsigned before = meshRadio.historyCount;
  press(SDLK_RETURN);
  if (meshRadio.historyCount != before + 1 || strcmp(meshRadio.history[before].text, "Frida")) return false;
  // Scroll past the visible rows, open a DM, and deliver a newer DM from
  // another peer. Back must follow the original recipient to its new row.
  press(SDLK_ESCAPE);
  for(unsigned i=0;i<meshRadio.channelCount;i++)press(SDLK_DOWN);
  press(SDLK_RETURN);
  DynamicJsonDocument status(2048);
  if(deserializeJson(status,uiStatus()))return false;
  String opened=status["recipient"].as<String>();
  if(opened!=meshRadio.idText(peerId(5)))return false;
  ChatMessage incoming;
  incoming.source=peerId(0);incoming.destination=meshRadio.nodeId;
  strcpy(incoming.text,"Simulated arrival");incoming.timestamp=time(nullptr);
  meshRadio.history[meshRadio.historyCount++]=incoming;
  press(SDLK_ESCAPE);
  String row=String("\"selected\":")+String(meshRadio.channelCount+1)+",";
  if(!has("\"page\":\"threads\"")||!has(row.c_str()))return false;
  press(SDLK_RETURN);
  if(deserializeJson(status,uiStatus())||status["recipient"].as<String>()!=opened)return false;
  press(SDLK_F3);
  if(!has("\"page\":\"map\"")||!drawWindow(renderer,texture))return false;

  press(SDLK_F4);press(SDLK_DOWN);press(SDLK_RETURN); // Screen & device
  press(SDLK_RETURN);
  SDL_strlcpy(text.text.text,"X",sizeof(text.text.text));handleInput(text);
  press(SDLK_RETURN); // Finish editing the name
  press(SDLK_DOWN);press(SDLK_DOWN);press(SDLK_DOWN);press(SDLK_RIGHT); // Brightness +15
  press(SDLK_DOWN);press(SDLK_DOWN);press(SDLK_LEFT);  // Sounds off
  type("s");
  if(!has("\"page\":\"settings\"")||screenBrightness!=195)return false;
  press(SDLK_F4);press(SDLK_RETURN); // Radio
  press(SDLK_RIGHT);type("S"); // +1 kHz, save from the frequency row
  if(!has("\"page\":\"settings\"")||!savedSettingsMatch())return false;

  press(SDLK_F2);
  auto homeUnread=[&](unsigned expected){
    press(SDLK_F1);
    bool matches=!deserializeJson(status,uiStatus())&&status["unread"].as<unsigned>()==expected;
    press(SDLK_F2);
    return matches;
  };
  // Leave one unread DM, followed by the all-read case. Only opening a thread consumes it.
  for(unsigned i=0;i<meshRadio.historyCount-1u;++i)meshRadio.history[i].seen=true;
  if(!homeUnread(1))return false;
  type("u");
  if(!has((String("\"selected\":")+String(meshRadio.channelCount)+",").c_str()))return false;
  press(SDLK_RETURN);
  if(deserializeJson(status,uiStatus())||status["recipient"].as<String>()!=meshRadio.idText(peerId(0)))return false;
  press(SDLK_ESCAPE);
  String stayed=String("\"selected\":")+String(meshRadio.channelCount)+",";
  type("u");
  if(!has(stayed.c_str())||!has("\"page\":\"threads\""))return false;

  auto clip=[&](const char* shortcut){press(SDLK_F9);type(shortcut);};
  auto editName=[&](){press(SDLK_F4);press(SDLK_DOWN);press(SDLK_RETURN);press(SDLK_RETURN);};
  auto publicChat=[&](){press(SDLK_F1);press(SDLK_F2);press(SDLK_RETURN);};
  auto draftIs=[&](const char* expected){
    return !deserializeJson(status,uiStatus()) && status["composer"].as<String>()==expected;
  };
  publicChat();
  unsigned messageCount=meshRadio.historyCount;
  press(SDLK_UP); // Earlier UTF-8 message, above the "Frida" written in this test.
  clip("m");clip("v");
  if(!draftIs(meshRadio.history[1].text)||meshRadio.historyCount!=messageCount)return false;
  editName();
  auto clipboardBanner=[](){
    const uint16_t* pixels=hardware.canvas->getBuffer();
    return std::vector<uint16_t>(pixels+167*320,pixels+217*320);
  };
  press(SDLK_F9);
  auto emptyBanner=clipboardBanner();
  type("r"); // The UTF-8 message exceeds the 24-byte name limit.
  auto replaceError=clipboardBanner();
  if(replaceError==emptyBanner)return false;
  fakeMillis+=8000;uiTick();press(SDLK_UP); // Browsing actions must preserve the warning.
  if(clipboardBanner()!=replaceError)return false;
  type("v"); // Append includes the existing three bytes of "M9X".
  if(clipboardBanner()==replaceError||clipboardBanner()==emptyBanner)return false;
  press(SDLK_ESCAPE);press(SDLK_F9);
  if(clipboardBanner()!=emptyBanner)return false;
  type("r");type("c"); // Copy after a failure should clear it and copy the intact field.
  press(SDLK_F9);
  if(clipboardBanner()!=emptyBanner)return false;
  press(SDLK_ESCAPE);press(SDLK_ESCAPE);press(SDLK_ESCAPE);
  publicChat();clip("r");
  if(!draftIs("M9X"))return false;
  type("7");clip("c");
  editName();clip("r");type("A");clip("v");clip("c");
  press(SDLK_RETURN);press(SDLK_ESCAPE); // Finish the editor and discard the settings draft.
  publicChat();clip("r");
  if(!draftIs("M9X7AM9X7")||!savedSettingsMatch())return false;
  press(SDLK_F8);press(SDLK_F9);clip("v");press(SDLK_ESCAPE);
  if(!draftIs("M9X7AM9X7")||meshRadio.historyCount!=messageCount||!homeUnread(0))return false;
  press(SDLK_F1);
  // An outgoing-only history must leave the actual Chats row looking exactly like an empty one.
  unsigned savedCount=meshRadio.historyCount;
  meshRadio.historyCount=0;
  auto chatRow=[&](){
    fakeMillis+=1100;uiTick();
    const auto* pixels=hardware.canvas->getBuffer();
    return std::vector<uint16_t>(pixels+69*320,pixels+94*320);
  };
  auto emptyRow=chatRow();
  ChatMessage original=meshRadio.history[0];
  ChatMessage own{};
  own.source=meshRadio.nodeId;own.destination=meshmesh::Broadcast;own.outgoing=true;
  own.timestamp=uint32_t(time(nullptr))+10;meshRadio.history[0]=own;meshRadio.historyCount=1;
  if(chatRow()!=emptyRow||!has("\"unread\":0"))return false;
  meshRadio.history[0].outgoing=false;meshRadio.history[0].source=peerId(1);
  if(chatRow()==emptyRow||!has("\"unread\":1"))return false;
  meshRadio.history[0]=original;meshRadio.historyCount=savedCount;
  for(int i=0;i<6;i++)press(SDLK_DOWN);
  press(SDLK_RETURN);
  if(!has("\"page\":\"spectrum\""))return false;
  auto advance=[&](int steps){for(int i=0;i<steps;i++){fakeMillis+=160;uiTick();}};
  advance(6);
  if(!has("\"channel_dbm\":-"))return false;
  press(SDLK_RIGHT);press(SDLK_RETURN);advance(5);
  if(!has("\"band_sweep\":true")||!has("\"sweep_running\":false")||!has("\"sweeps_completed\":0"))return false;
  type("s");advance(3);press(SDLK_RETURN);
  if(!has("\"sweep_running\":false")||!has("\"sweeps_completed\":0"))return false;
  type("s");advance(120);advance(5);
  if(!has("\"sweeps_completed\":1")||!has("\"sweep_running\":false")||!has("\"sweep_peak_dbm\":-"))return false;
  type("w");advance(3);
  netfind::state.phase=netfind::State::Running;advance(65);
  if(!has("\"sweep_waiting\":true")||!has("\"sweeps_completed\":1"))return false;
  netfind::state.phase=netfind::State::Idle;advance(113*24+6);
  if(!has("\"sweeps_completed\":25")||!has("\"sweep_running\":false"))return false;
  save(settingsFile+".spectrum.ppm");
  type("w");advance(6);press(SDLK_RETURN);advance(120);
  if(!has("\"sweeps_completed\":25")||!has("\"sweep_running\":false"))return false;
  type("s");press(SDLK_ESCAPE);press(SDLK_RETURN);
  if(!has("\"band_sweep\":false")||!has("\"sweep_running\":false"))return false;
  press(SDLK_ESCAPE);
  return has("\"page\":\"home\"");
}

#include "handheld_checks.inc"

static int runSimulator(bool test,bool reload=false) {
  if (test) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL video: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow("MeshMesh M9 simulator",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, -1, 0) : nullptr;
  SDL_Texture* texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
      SDL_TEXTUREACCESS_STREAMING, 320, 240) : nullptr;
  bool ok = texture && SDL_RenderSetLogicalSize(renderer, 320, 240) == 0;
  if (ok && test) {
    ok = (reload||checkSimulator(renderer, texture))&&checkHandheld(reload);
    if(ok)puts(reload?"Fresh-process credential, contact and appearance checks passed.":"M9 navigation, chat, clipboard, maps, contact notes, tag filter, appearance and screen lock checks passed.");
  } else if (ok) {
    SDL_StartTextInput();
    Uint32 last = SDL_GetTicks();
    bool running = true;
    while (running && ok) {
      Uint32 now = SDL_GetTicks();
      fakeMillis += now - last;
      last = now;
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) running = false;
        else handleInput(event);
      }
      meshRadio.tick();
      radar.tick();
      wardrive::tick();
      notifications::tick();
      uiTick();
      ok = drawWindow(renderer, texture);
      SDL_Delay(32);
    }
    SDL_StopTextInput();
  }
  if (!ok) fprintf(stderr, "Simulator failed. %s\nUI: %s\n", SDL_GetError(), uiStatus().c_str());
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return ok ? 0 : 1;
}
