// Screens rendered by the preview; the key codes are the production M9 codes.
// Selects an app's tile on the M9 home grid (three a row) from the first one; an app in a folder: the folder's tile
// opens the folder, then the app's tile there is selected.
static void tile(const char* id){
 key(0x82);uint8_t o[AppEntriesMax];unsigned n=appsShown(o);int app=-1;for(uint8_t i=0;i<uiAppCount;i++)if(!strcmp(uiApps[i],id))app=i;
 auto pick=[](unsigned i){for(unsigned k=0;k<i/3;k++)key(0xb6);for(unsigned k=0;k<i%3;k++)key(0xb7);};
 for(unsigned i=0;i<n;i++)if(o[i]==app){pick(i);return;}
 int f=app<0?-1:appsFolderOf(uint8_t(app));
 for(unsigned i=0;i<n&&f>=0;i++)if(o[i]==(AppFolder|f)){pick(i);key(13);uint8_t in[AppEntriesMax];unsigned m=appsShown(in,f);for(unsigned j=0;j<m;j++)if(in[j]==app){pick(j);return;}}
 printf("FAIL no home tile %s\n",id);exit(1);}
// Switch using the same draft editor and Save action as the handheld.
static void switchHomeLayout(){
 key(0x90);key(0xb6);key(13);
 for(int i=0;i<4;i++)key(0xb6);
 key(0xb7);shot("display-home-layout");
 for(int i=0;i<5;i++)key(0xb5);
 key(13);key(0x82);
}
static void scenario(){
 shot("role-boot");key(0x86);shot("home");
 // Cover the default list, then run the existing grid keyboard scenarios.
 {
  auto expect=[](const char* s){if(uiStatus().indexOf(s)<0){printf("FAIL launcher: %s, wanted %s\n",uiStatus().c_str(),s);exit(1);}};
  expect("\"layout\":\"list\"");
  strlcpy(config.apps,"chats quick {Tools radar spectrum} nodes",sizeof config.apps); // a folder to open
  uint8_t order[AppEntriesMax];int n=appsShown(order);
  key(0xb5);String last=String("\"selected\":")+String(n-1)+",";expect(last.c_str());key(0xb6);
  int folder=0;while(folder<n&&!(order[folder]&AppFolder))folder++;
  if(folder==n){printf("FAIL missing the folder\n");exit(1);}
  for(int i=0;i<folder;i++)key(0xb6);
  key(13);expect("\"page\":\"folder\"");expect("\"layout\":\"list\"");shot("folder-list");
  switchHomeLayout();expect("\"layout\":\"tiles\"");
  for(int i=0;i<folder/3;i++)key(0xb6);for(int i=0;i<folder%3;i++)key(0xb7);
  key(13);key(0xb7);expect("\"page\":\"folder\"");expect("\"selected\":1,");shot("folder-tiles");
  key(13);expect("\"page\":\"spectrum\"");key(0x86);expect("\"selected\":1,");
  key(0x86);expect("\"layout\":\"tiles\"");key(0x82);shot("home-tiles");
  config.apps[0]=0;
 }
 key(0x81);shot("threads");key(13);shot("chat-public");key(0x86);
 // Channels: the list, a hashtag channel's card, a private channel and its card, adding one, an invitation.
 key(0xb6);shot("threads-channels");key(0xb7);shot("channel-hashtag");key(0x86);
 key(0xb6);key(13);shot("chat-channel");key(0xb4);shot("channel-private");key(0xb7);shot("channel-region");key(0xb4);key(0xb6);shot("channel-sound");{auto sound=[&]{String st=uiStatus();for(unsigned i=0;i<meshRadio.channelCount;i++){const auto& c=meshRadio.channelList[i];if(st.indexOf("\"channel\":\""+meshRadio.idText(c.id)+"\"")>=0)return int(c.policy.sound);}return -1;};if(sound()!=1){printf("FAIL channel sound default: %d\n",sound());exit(1);}key(13);if(sound()!=0){printf("FAIL channel sound off: %d\n",sound());exit(1);}key(13);if(sound()!=1){printf("FAIL channel sound on: %d\n",sound());exit(1);}}key(0xb6);key(0xb6);key(13);shot("channel-leave");key(0x86);key(0x86);
 key(0x81);key(0x83);key(13);shot("channel-add");key(13);key(0x83);for(char ch:std::string("test"))key(ch);shot("channel-add-tag");key(0x83);key(0x86);key(0x86);
 {String marat="\"recipient\":\""+meshRadio.idText(peerId(5))+"\"";bool found=false; // the invitation is in the chat with Marat
  for(int at=0;at<10&&!found;at++){key(0x81);for(int i=0;i<at;i++)key(0xb6);key(13);if(uiStatus().indexOf(marat)>=0)found=true;else key(0x86);}
  if(!found){printf("FAIL chat with the invitation not found\n");exit(1);}}
 shot("chat-invite");key(0xb4);shot("chat-invite-joined");key(0x86);
 if(!meshRadio.channel(channels::idOf(meshRadio.channelList[3].secret))||strcmp(meshRadio.channelList[3].name,"Походы")){printf("FAIL invitation not joined\n");exit(1);}
 key(0xb6);key(13);shot("chat");
 for(char ch:std::string("Privet! Kak dela? 5to novogo @ 5"))key(ch);key(0xb7);shot("compose");key(0xa3);shot("clipboard");key('l');shot("layout");key(0x86);key(0x86);key(0x86);
 key(0x85);shot("map");key(0xb7);key(0xb6);shot("map-browse");key('L');shot("library");key(0x86);
 key(0x82);key(0xb7);key(0xb7);key(13);shot("nodes");key(13);shot("node");key(0xb7);key(0xb7);key(0xb7);key(13);shot("node-forget");key(0x86);key(0x86);
 // A repeater's card and its server page: login, status, CLI replies and the trace (fake answers of the preview).
 key(0x82);key(0xb7);key(0xb7);key(13);
 {String want=String("\"selected_node\":\"")+meshRadio.idText(peerId(1))+"\"";for(int i=0;i<8&&uiStatus().indexOf(want)<0;i++)key(0xb6);
  if(uiStatus().indexOf(want)<0){printf("FAIL repeater not in the node list: %s\n",uiStatus().c_str());exit(1);}}
 key(13);shot("node-repeater");key(13);if(uiStatus().indexOf("\"page\":\"remote\"")<0){printf("FAIL server page: %s\n",uiStatus().c_str());exit(1);}shot("remote");key(0x86);key(0x86);key(0x86);
 key(0x82);key(0xb6);key(13);shot("sensors");
 // Navigation: the GPS switch, then BACK returns where the page was opened (the menu tile, the settings row).
 {bool gps=config.gps;key(13);if(config.gps==gps){printf("FAIL GPS switch in navigation\n");exit(1);}shot("sensors-gps");key(13);
  key(0x86);if(uiStatus().indexOf("\"page\":\"home\"")<0||uiStatus().indexOf("\"selected\":3,")<0){printf("FAIL back from navigation to the menu: %s\n",uiStatus().c_str());exit(1);}
  key(0x90);key(0xb6);key(0xb6);key(13);key(0x86);if(uiStatus().indexOf("\"page\":\"settings\"")<0||uiStatus().indexOf("\"selected\":2,")<0){printf("FAIL back from navigation to the settings: %s\n",uiStatus().c_str());exit(1);}}
 // Network of your city (site feature card): a catalogue profile under test, the saved one comes back on a restart.
 {RadioProfile saved=config.radio(),test;test.frequency=868.731f;test.bandwidth=62.5f;test.sf=7;test.cr=7;
  RadioProfile before;before.frequency=869.525f;before.bandwidth=250;before.sf=11;before.cr=5;
  config.trial=before;config.setRadio(test);key(0x90);key(13);for(int i=0;i<11;i++)key(0xb6);shot("radio-test");
  key(0x86);config.trial=RadioProfile();config.setRadio(saved);}
 // The network search found the Moscow profile (the preview's netfind): the row offers to test it.
 key(0x90);key(13);for(int i=0;i<12;i++)key(0xb6);key(13);shot("radio-net");netfind::state.phase=netfind::State::Idle;key(0x86);
 // Regional presets (Bas van Dijk): <> on the first row steps through the presets this radio takes and fills the radio rows.
 {key(0x90);key(13);for(int i=0;i<40&&uiStatus().indexOf("\"radio_preset\":\"Netherlands (Limburg)\"")<0;i++)key(0xb7);shot("radio-preset");
  if(uiStatus().indexOf("\"radio_preset\":\"Netherlands (Limburg)\"")<0){printf("FAIL preset Netherlands (Limburg): %s\n",uiStatus().c_str());exit(1);}
  for(int i=0;i<4;i++)key(0xb6);key(0xb4);if(uiStatus().indexOf("\"radio_preset\":\"custom\"")<0){printf("FAIL preset after a changed CR: %s\n",uiStatus().c_str());exit(1);}key(0x86);}
 key(0x90);shot("settings");key(13);key(0xb6);key(0xb7);shot("radio");for(int i=0;i<7;i++)key(0xb6);key(0xb7);shot("radio-hash");key(0xb6);key(0xb6);key(13);shot("radio-regions");key(0xb5);key(0xb7);key(0xb7);key(0xb7);shot("radio-region");key(13);for(int i=0;i<3;i++)key(8);for(char ch:std::string("sao"))key(ch);shot("radio-region-type");key(0x86);key(0x86);key(0xb6);key(13);for(int i=0;i<9;i++)key(0xb6);key(0xb7);shot("display-battery");for(int i=0;i<9;i++)key(0xb5);shot("display");key(13);shot("display-name");key(0x86);key(0x86);
 tile("connect");key(13);key(13);key(0xb6);key(13);shot("network");key(13);key(0xb6);key(13);tile("health");key(13);shot("diagnostics");key(0x90);for(int i=0;i<5;i++)key(0xb6);key(13);shot("help");key(13);shot("help-layout");key(0x86);key(13);shot("help-layout");key(0x86);
 // Connections with Bluetooth on: the MeshCore- name the phone apps see, then off again.
 key(0x90);for(int i=0;i<3;i++)key(0xb6);key(13);key(0xb6);key(0xb6);key(13);
 if(!bleActive()||uiStatus().indexOf("\"page\":\"network\"")<0){printf("FAIL Bluetooth on: %s\n",uiStatus().c_str());exit(1);}shot("network-ble");key(13);key(0x86);key(0x86);
 key(0x88);shot("locked");
 // Two OK presses within a second unlock as the hold does; one OK, or two far apart, do not.
 key(13);if(uiStatus().indexOf("\"locked\":true")<0){printf("FAIL one OK unlocked\n");exit(1);} // key() ticks 1 s
 key(13);if(uiStatus().indexOf("\"locked\":true")<0){printf("FAIL slow OK presses unlocked\n");exit(1);}fakeMillis+=1500;
 uiKey(13);fakeMillis+=300;uiKey(13);tick();if(uiStatus().indexOf("\"locked\":false")<0){printf("FAIL two OK presses did not unlock: %s\n",uiStatus().c_str());exit(1);}
 key(0x88);key(0xa3);key(0x82);
 key(0xb6);key(0xb7);key(0xb7);shot("home-radar");key(0xb6);shot("home-scrolled");key(0xb5);key(13);shot("radar");key(0xb6);key(0xb6);shot("radar-select");key(0xb5);key(0xb5);key(13);shot("homing");key(0x86);shot("radar-back");key(0xb7);shot("motion");key('b');for(int i=0;i<4;i++)tick();shot("motion-beacon");key('b');key(0xb7);key(0x86);
 // Device modes: the repeater and room pages, the mode choice from Settings.
 config.role=RoleRepeater;key(0x82);shot("repeater");key(0x86);
 config.role=RoleRoom;key(0x82);shot("room");key(13);for(char ch:std::string(config.lang==LangRu||config.lang==LangUk?"Vstre1a u vhoda posle obeda":"Meet at the gate after lunch"))key(ch);shot("room-post");key(0x86);
 key(0x90);key(0xb5);key(13);key(0xb6);shot("role-pick");key(0x86);config.role=RoleNormal;key(0x82);
// Apps in the chosen order ("apps" setting): hidden ones get no tile and no click.
 {auto page=[](const char* want){std::string w=std::string("\"page\":\"")+want+"\"";if(uiStatus().indexOf(w.c_str())<0){printf("FAIL apps order, want %s: %s\n",want,uiStatus().c_str());exit(1);}};
  strlcpy(config.apps,"settings -chats -nodes map -quick -people",sizeof config.apps);
  key(0x82);shot("home-apps");key(13);page("settings");key(0x82);key(0xb7);key(13);page("map");key(0x82);
  config.apps[0]=0;printf("apps order checks passed\n");}
// Quick Send and manual counting: the production key paths.
 {auto must=[](bool ok,const char* why){if(!ok){printf("FAIL wishlist UI %s: %s\n",why,uiStatus().c_str());exit(1);}};
  strlcpy(config.apps,"quick people",sizeof config.apps);config.dimAfter=0;config.autoLock=0;
  key(0x82);key(13);must(uiStatus().indexOf("\"page\":\"quick\"")>=0,"open Quick Send");shot("quick");
  unsigned sent=meshRadio.historyCount;key(13);must(uiStatus().indexOf("\"quick_targets\":true")>=0&&meshRadio.historyCount==sent,"first row opens recipients without sending");shot("quick-targets");
  key(0x86);must(uiStatus().indexOf("\"selected\":0")>=0,"cancel returns to recipient card");key(13);key(13);
  must(uiStatus().indexOf("\"quick_index\":0")>=0,"recipient confirmation selects first preset");shot("quick-preset");key(13);
  must(meshRadio.historyCount==sent+1&&String(meshRadio.history[sent].text)==quickSend::state.text[0],"first preset still sends its own text");
  key(0xb5);key(0xb5);shot("quick-last");key(13);
  must(meshRadio.historyCount==sent+2&&String(meshRadio.history[sent+1].text)==quickSend::state.text[quickSend::Count-1],"last preset remains reachable and sends its own text");
  key(0x86);key(0xb7);key(13);int32_t manual=people::value();key(13);must(people::value()==manual+1,"counter +1");key(0xb6);must(people::value()==manual,"counter -1");
  bool ru=config.lang==LangRu||config.lang==LangUk;people::rename(0,ru?"Гости":"Guests");people::set(0,27);shot("people");
  key(0xb7);must(uiStatus().indexOf("\"tally_new\":true")>=0,"right of the last card: a new one");shot("people-new");key(13);must(people::tally.count==2&&people::tally.current==1,"OK adds a card");
  key('n');for(char ch:std::string(ru?"Kofe":"Coffee"))key(ch);shot("people-name");key(13);key(13);key(13);must(people::value()==2,"the new card counts on its own");
  key(0xb4);must(people::tally.current==0&&people::value()==27,"left turns back to the first card");key(0xb7);key(0x08);key(0x08);must(people::tally.count==1,"DEL twice deletes");
  people::add(ru?"Круги":"Laps");people::set(1,12);people::recolor(1,7);people::add(ru?"Подходы":"Sets");people::set(2,4);people::recolor(2,5);people::select(0);fakeMillis+=4000;tick();shot("people-cards");
  people::remove(2);people::remove(1);key(0x86);
  config.apps[0]=0;printf("wishlist UI checks passed\n");}
// Wardrive: off, then listening and pings along a street; its points on the map.
 {auto must=[](bool ok,const char* why){if(!ok){printf("FAIL wardrive UI %s: %s\n",why,uiStatus().c_str());exit(1);}};
  strlcpy(config.apps,"wardrive",sizeof config.apps);config.dimAfter=0;config.autoLock=0;
  key(0x86);key(0x82);key(13);must(uiStatus().indexOf("\"page\":\"wardrive\"")>=0,"open wardrive");shot("wardrive-off");
  key(13);must(wardrive::settings.passive&&wardrive::settings.ping,"OK starts listening and pings");wardriveData();tick();shot("wardrive");
  key(0x85);bool follow=maps.follow;maps.follow=false;maps.center(55.7975,49.1137);shot("wardrive-map");maps.follow=follow;key(0x82);
  {StaticJsonDocument<64>d;d["action"]="stop";wardrive::command(d.as<JsonObjectConst>());}config.apps[0]=0;printf("wardrive UI checks passed\n");}
// Power off last: the screens stop taking keys once it is requested.
 key(0x82);showPasteFailure();shot("clipboard-paste-failed");
 key(0x86);key(0x86); // Close clipboard and name editor.
 key(0x90);key(0xb5);shot("settings-poweroff");key(13);key(13);
 if(!powerOffPending()){printf("FAIL Turn off needs two OK\n");exit(1);}
 shot("poweroff");uiFarewell();shot("poweroff-done");
}
