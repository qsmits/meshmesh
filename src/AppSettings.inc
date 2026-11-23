namespace {
// How applySettings treats a change of the shared radio parameters: an ordinary change remembers the profile
// before it (config.previous); a test (radio try) keeps the saved one in config.trial; a revert ends the test.
enum RadioWrite:uint8_t {WriteNormal,WriteTry,WriteRevert} radioWrite=WriteNormal;
void profileJson(JsonObject o,const RadioProfile& r){o["frequency"]=r.frequency;o["bandwidth"]=r.bandwidth;o["sf"]=r.sf;o["cr"]=r.cr;}
}
String configJson(bool includeKey) {
  DynamicJsonDocument d(2560);d["name"]=config.name;d["frequency"]=config.frequency;d["bandwidth"]=config.bandwidth;d["sf"]=config.sf;d["cr"]=config.cr;d["power"]=config.power;
  d["hops"]=config.hops;d["path_hash"]=config.pathHash;d["relay"]=config.relay;d["gps"]=config.gps;d["sound"]=config.sound;d["battery_volts"]=config.batteryVolts;d["lang"]=langCodes[config.lang<LangCount?config.lang:0];d["russian"]=config.lang==LangRu;d["brightness"]=config.brightness;if(includeKey)d["key"]=config.keyHex();
  d["theme"]=config.theme;d["accent_color"]=config.accentColor;d["night_light"]=config.nightBrightness;
  d["input_language"]=config.inputRussian?"ru":"en";
  d["launcher_tiles"]=config.launcherTiles;
  d["auto_lock"]=config.autoLock;d["dim_after"]=config.dimAfter;d["lock_details"]=config.lockDetails;
  d["notify_led"]=config.notifyLed;d["notify_wake"]=config.notifyWake;d["notify_popup"]=config.notifyPopup;d["notify_failed"]=config.notifyFailed;d["notify_words"]=config.notifyWords;
  d["utc_offset"]=config.utcOffset;d["apps"]=appsText(config.apps);d["region"]=config.region;d["preset"]=config.preset;d["net_auto"]=config.netAuto;
  if(config.previous.set())profileJson(d.createNestedObject("radio_prev"),config.previous);
  if(config.trial.set())profileJson(d.createNestedObject("radio_trial"),config.trial);
  String s;serializeJson(d,s);return s;
}
String applySettings(JsonObjectConst v) {

  Config next=config;
  for(JsonPairConst kv:v) {
    String name=kv.key().c_str();JsonVariantConst value=kv.value();
    if(name=="radio_prev"||name=="radio_trial")continue; // read-only (configJson): radio previous / radio try
    else if(name=="key") {if(!value.is<const char*>()||!next.setKey(value.as<String>()))return "ERR key must be 64 hexadecimal digits";}
    else if(name=="name") {if(!value.is<const char*>() || value.as<String>().length()>24 || value.as<String>().length()<1)return "ERR name: 1..24 UTF-8 bytes";strlcpy(next.name,value.as<const char*>(),sizeof(next.name));}
    else if(name=="frequency" || name=="bandwidth") {
      if(!value.is<float>() && !value.is<int>())return "ERR numeric value required";
      if(name=="frequency")next.frequency=value.as<float>();else next.bandwidth=value.as<float>();
    } else if(name=="sf"||name=="cr"||name=="power"||name=="hops"||name=="path_hash"||name=="brightness") {
      if(!value.is<int>())return "ERR integer value required";int n=value.as<int>();
      if(name=="sf") {if(n<7||n>12)return "ERR SF 7..12";next.sf=n;}
      if(name=="cr") {if(n<5||n>8)return "ERR CR 5..8";next.cr=n;}
      if(name=="power") {if(n<0||n>MM_MAX_POWER)return "ERR power 0.." + String(MM_MAX_POWER) + " dBm";next.power=n;}
      if(name=="hops") {if(n<0||n>7)return "ERR hops 0..7";next.hops=n;}
      if(name=="path_hash") {if(n<1||n>3)return "ERR path_hash 1..3 bytes";next.pathHash=n;}
      if(name=="brightness") {if(n<10||n>255)return "ERR brightness 10..255";next.brightness=n;}
    } else if(name=="notify_led"||name=="notify_wake"||name=="notify_popup") {
      if(!value.is<int>()||value.as<int>()<0||value.as<int>()>2)return "ERR notification mode 0 off, 1 all, 2 mentions";
      if(name=="notify_led")next.notifyLed=value.as<int>();if(name=="notify_wake")next.notifyWake=value.as<int>();if(name=="notify_popup")next.notifyPopup=value.as<int>();
    } else if(name=="notify_words") {
      if(!value.is<const char*>()||value.as<String>().length()>96)return "ERR notify_words: up to 96 UTF-8 bytes";strlcpy(next.notifyWords,value.as<const char*>(),sizeof next.notifyWords);
    } else if(name=="utc_offset") {
      if(!value.is<int>())return "ERR integer UTC offset required";int n=value.as<int>();if(n<-720||n>840||n%15)return "ERR UTC offset minutes: -720..840, step 15";next.utcOffset=n;
    } else if(name=="auto_lock"||name=="dim_after") {
      if(!value.is<int>())return "ERR integer value required";int n=value.as<int>();
      if(n!=0 && (n<(name=="auto_lock"?30:10)||n>600))return "ERR timeout 0 or 30..600 (dim: 10..600)";
      if(name=="auto_lock")next.autoLock=n;else next.dimAfter=n;
    } else if(name=="apps") {
      AppMenu menu;
      if(!value.is<const char*>()||!appsParse(value.as<const char*>(),menu,true)){String all;for(uint8_t i=0;i<uiAppCount;i++)all+=String(i?" ":"")+uiApps[i];return "ERR apps: IDs separated by spaces, -ID hides, {NAME ID ...} is a folder (up to 6): "+all;}
      String text=appsText(value.as<const char*>());if(text.length()>=sizeof next.apps)return "ERR apps: too long";strlcpy(next.apps,text.c_str(),sizeof next.apps);
    } else if(name=="theme"||name=="accent_color"||name=="night_light"){
      if(!value.is<unsigned>())return "ERR display setting";
      unsigned n=value.as<unsigned>();
      if(name=="theme"&&n<=2)next.theme=n;
      else if(name=="accent_color"&&n<=3)next.accentColor=n;
      else if(name=="night_light"&&(n==0||(n>=10&&n<=255)))next.nightBrightness=n;
      else return "ERR display setting";
    } else if(name=="input_language"){
      String code=value.as<String>();
      if(code!="en"&&code!="ru")return "ERR input_language: en or ru";
      next.inputRussian=code=="ru";
    } else if(name=="launcher_tiles") {
      if(!value.is<bool>())return "ERR boolean required";
      next.launcherTiles=value.as<bool>();
    } else if(name=="region") { // "" or "*": none; the same name keeps its key (an app may have set a private one)
      if(!value.is<const char*>())return "ERR region: text";String raw=value.as<String>();raw.trim();
      if(!raw.length()||raw==regions::Unscoped){next.region[0]=0;memset(next.regionKey,0,16);}
      else if(raw!=config.region){String r=regions::normalize(raw);if(!r.length())return "ERR region: letters, digits and '-', up to 30 bytes";strlcpy(next.region,r.c_str(),sizeof next.region);regions::keyOf(r,next.regionKey);}
    } else if(name=="preset") { // the regional preset chosen last, by name (RadioPresets.h); "" - none
      if(!value.is<const char*>()||value.as<String>().length()>=sizeof next.preset)return "ERR preset: up to 31 bytes";strlcpy(next.preset,value.as<const char*>(),sizeof next.preset);
    } else if(name=="lang") {
      int l=value.is<const char*>()?langFromCode(value.as<String>()):-1;
      if(l<0||!langAvailable(l)){String all;for(int i=0;i<LangCount;i++)if(langAvailable(i))all+=String(all.length()?"|":"")+langCodes[i];return "ERR lang: "+all;}next.lang=l;
    } else if(name=="relay"||name=="gps"||name=="sound"||name=="russian"||name=="battery_volts"||name=="lock_details"||name=="notify_failed"||name=="net_auto"||name=="double_back") {
      if(!value.is<bool>())return "ERR boolean required";bool n=value.as<bool>();if(name=="net_auto")next.netAuto=n;if(name=="double_back")next.doubleBack=n;
      if(name=="notify_failed")next.notifyFailed=n;if(name=="relay")next.relay=n;if(name=="gps")next.gps=n;if(name=="sound")next.sound=n;if(name=="battery_volts")next.batteryVolts=n;if(name=="lock_details")next.lockDetails=n;
      if(name=="russian"&&!v.containsKey("lang")&&(n||next.lang==LangRu))next.lang=n?LangRu:LangEn; // pages from before "lang": false leaves other languages alone
    } else return "ERR unknown setting: "+name;
  }
  if(!next.valid())return "ERR invalid settings; 868 MHz range is 863..870";
  if(!next.sameRadio(config.radio())){
    if(radioWrite==WriteTry){if(!config.trial.set())next.trial=config.radio();if(next.sameRadio(next.trial))next.trial=RadioProfile();}
    else if(radioWrite==WriteRevert)next.trial=RadioProfile();
    else if(config.trial.set()){next.previous=config.trial;next.trial=RadioProfile();} // a change during a test: the saved one was the last
    else next.previous=config.radio();
  }
  Config old=config;config=next;
  bool radioChanged=old.frequency!=config.frequency||old.bandwidth!=config.bandwidth||old.sf!=config.sf||old.cr!=config.cr||old.power!=config.power||old.hops!=config.hops||old.pathHash!=config.pathHash||old.relay!=config.relay||memcmp(old.key,config.key,sizeof old.key)||memcmp(old.regionKey,config.regionKey,sizeof old.regionKey);
  if(radioChanged&&meshRadio.busy()){config=old;return "ERR radio busy; retry";}
  if(radioChanged&&!meshRadio.applyConfig()) {config=old;meshRadio.applyConfig();return "ERR radio rejected settings; restored previous";}
  if(!config.save()){config=old;if(radioChanged)meshRadio.applyConfig();return "ERR settings storage; restored previous";}
  if(old.inputRussian!=config.inputRussian)uiUseDefaultInput();
  meshServer.configChanged();if(strcmp(old.name,config.name))bleRename();if(old.brightness!=config.brightness)hardware.brightness(uiScreenOff()?0:config.brightness);if(old.gps!=config.gps)hardware.setGps(config.gps);
  meshRadio.event="Settings saved";meshRadio.dirty=true;return "OK settings saved";
}
// Radio profiles of the networks catalogue: "radio try {frequency,bandwidth,sf,cr}" applies one for a test (not saved:
// a restart returns the saved one), "radio keep" saves the test, "radio revert" ends it, "radio previous" goes back to
// the profile before the last change (and keeps the current one as the previous).
String radioCommand(const String& line){
  const Config& c=config;String reply;const char* usage="ERR radio try {frequency,bandwidth,sf,cr}|keep|revert|previous";
  if(line.startsWith("radio try ")){
    StaticJsonDocument<192>d;if(deserializeJson(d,line.substring(10))||!d.is<JsonObject>()||d.size()!=4)return usage;
    for(const char* k:{"frequency","bandwidth","sf","cr"})if(!d.containsKey(k))return usage;
    radioWrite=WriteTry;reply=applySettings(d.as<JsonObjectConst>());radioWrite=WriteNormal;
    return reply.startsWith("OK")?(c.trial.set()?"OK radio test":"OK radio saved settings"):reply;
  }
  if(line=="radio keep"){if(!c.trial.set())return "ERR radio: no test";RadioProfile saved=c.trial;config.previous=saved;config.trial=RadioProfile();
    if(!config.save()){config.trial=saved;return "ERR settings storage";}meshRadio.dirty=true;return "OK radio test saved";}
  bool test=c.trial.set(),back=line=="radio revert";
  if(!back&&line!="radio previous")return usage;
  if(back!=test)return test?"ERR radio: test running":"ERR radio: no test";
  if(!test&&!c.previous.set())return "ERR radio: no previous settings";
  StaticJsonDocument<192>j;profileJson(j.to<JsonObject>(),test?c.trial:c.previous);
  radioWrite=test?WriteRevert:WriteNormal;reply=applySettings(j.as<JsonObjectConst>());radioWrite=WriteNormal;
  return reply.startsWith("OK")?"OK radio restored":reply;
}
