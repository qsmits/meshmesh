// Use the handheld controls and sample contacts, including a second process with the same storage.
static bool checkHandheld(bool reload){
 auto tap=[](int k){uiKey(k);fakeMillis+=180;uiTick();};
 auto type=[&](const char* s){while(*s)tap(*s++);};
 auto has=[](const char* s){return uiStatus().indexOf(s)>=0;};
 auto require=[&](bool result,const char* what){
  if(!result)fprintf(stderr,"FAIL %s: %s\n",what,uiStatus().c_str());
  return result;
 };
 auto screen=[&](int row){
  tap(0x90);tap(0xb6);tap(13);
  for(int i=0;i<row;i++)tap(0xb6);
 };
 auto nodes=[&](){tap(0x82);tap(0xb6);tap(0xb6);tap(13);};
 auto snapshot=[&](const char* name){save(settingsFile+"."+name+".ppm");};
 auto filter=[&](const char* tag){tap('f');type(tag);tap(13);};
 auto lockEntry=[&](const char* s){type(s);tap(13);};
 auto checkDay=[](){
  Config c=config;c.utcOffset=60;c.theme=2;
  time_t midnight=1704067200;
  for(int minute:{359,360,1139,1140}){
   time_t utc=midnight+60*minute;
   bool day=minute==360||minute==1139;
   if(c.lightAt(utc)!=day||c.backlight(utc)!=(day?c.brightness:c.nightBrightness))return false;
  }
  return !c.lightAt(0)&&c.backlight(0)==c.brightness;
 };
 if(reload){
  if(!require(has("\"locked\":true"),"protected at boot"))return false;
  tap(0xa3);tap(13);tap(13);tap(0x90);
  if(!require(has("\"locked\":true"),"gesture and shortcuts stay locked"))return false;
  lockEntry("Friday 73");
  if(!require(config.inputRussian&&has("\"typing_language\":\"ru\""),"saved typing default after restart"))return false;
  screen(2);tap(0xb7);tap('s');
  if(!require(!config.inputRussian&&has("\"typing_language\":\"en\""),"save English input"))return false;
  if(!require(!has("\"locked\":true")&&config.theme==1&&config.accentColor==1&&config.nightBrightness==180,"correct phrase and saved appearance"))return false;
 }else{
  tap(0x82);
  for(int i=0;i<5;i++)tap(0xb6);
  tap(13);
  if(!require(has("\"page\":\"radar\"")&&radar.count>2,"open live radar"))return false;
  snapshot("radar-sweep");
  RadarTarget expected=radar.targets[1];
  std::swap(radar.targets[0],radar.targets[1]); // Scan refreshed after paint, just before the key.
  tap(0xb6);tap('l');
  snapshot("radar-list");
  if(!require(has("\"radar_paused\":true")&&has("\"radar_list\":true"),"expand paused radar list"))return false;
  tap(13);
  if(!require(radar.tracking&&radar.focus.id==expected.id&&radar.focus.kind==expected.kind,"home on the displayed row after a reorder"))return false;
  tap(0x86);
  if(!require(has("\"radar_row\":1")&&has("\"radar_list\":true"),"return to the same radar row"))return false;
  radar.count=0;
  tap(13);
  if(!require(!radar.tracking&&has("\"radar_row\":1"),"vanished target stays selected"))return false;
  tap('p');
  if(!require(has("\"radar_paused\":false")&&has("\"radar_rows\":0"),"resume uses newest scan"))return false;
  tap(0x86);
  if(!require(has("\"radar_list\":false"),"collapse full-width list"))return false;
  tap(0x82);
  screen(14);tap(0xb7);tap(0xb7);tap('s');
  if(!require(config.theme==2,"save Auto theme"))return false;
  screen(14);tap(0xb7);tap('s');
  for(int accent=1;accent<=4;accent++){
   screen(15);tap(0xb7);tap('s');
   if(!require(config.accentColor==accent%4,"accent selection and wrap"))return false;
  }
  screen(14);tap(0xb7);tap(0x86);
  if(!require(config.theme==0,"cancel theme draft"))return false;
  screen(14);tap(0xb7);tap(0xb6);tap(0xb7);tap(0xb6);tap(0xb4);tap('s');
  if(!require(config.theme==1&&config.accentColor==1&&config.nightBrightness==180&&checkDay(),"theme, accent and day/night boundaries"))return false;
  tap(0x82);fakeMillis+=4000;uiTick();snapshot("light");
 }
 nodes();
 if(!require(has("\"page\":\"nodes\""),"Nodes list"))return false;
 if(!reload){
  tap(0x83);snapshot("nodes-actions");tap(0x86);
  tap(13);tap('n');tap(13);type("Runs the north repeater");tap(13);
  tap(0xb6);tap(13);type("Friday, Field crew");tap(13);snapshot("notes");
  tap(0x86);tap(0x86);
 }
 filter("friday");
 if(!require(has("\"visible_nodes\":1"),"filter persisted tags"))return false;
 tap(13);tap('n');tap(13);tap(0xa3);tap('c');tap(0x86);tap(0x81);tap(13);tap(0xa3);tap('v');
 if(!require(has("Runs the north repeater"),"read persisted note through clipboard"))return false;
 nodes();tap('f');for(int i=0;i<6;i++)tap(8);type("missing");tap(13);
 if(!require(has("\"visible_nodes\":0"),"unmatched tag"))return false;
 auto beforeAdvert=meshRadio.txCount;
 tap(0x83);tap(0x86);
 if(!require(meshRadio.txCount==beforeAdvert,"cancel advert menu"))return false;
 tap(0x83);tap(13);
 if(!require(meshRadio.txCount==beforeAdvert+1&&has("\"page\":\"nodes\""),"advert from empty filtered list"))return false;
 tap(0x85);tap(0x83);tap(0xb6);auto zoom=maps.zoom;tap(13);
 if(!require(maps.zoom==zoom+1,"zoom from map actions"))return false;
 tap(0x83);tap(0xb6);tap(0xb6);tap(13);
 if(!require(maps.zoom==zoom,"zoom out from map actions"))return false;
 nodes();
 tap('f');for(int i=0;i<7;i++)tap(8);tap(13);
 if(!require(has("\"visible_nodes\":6"),"clear filter"))return false;
 if(!reload){
  screen(2);tap(0xb7);tap(0x86);
  if(!require(!config.inputRussian,"cancel keyboard default"))return false;
  screen(2);tap(0xb7);tap('s');
  if(!require(config.inputRussian&&has("\"typing_language\":\"ru\""),"save Russian input"))return false;
  tap(0x81);tap(13);tap(0x83);tap(13);
  if(!require(config.inputRussian&&has("\"typing_language\":\"en\""),"temporary layout keeps saved default"))return false;
 }
 screen(17);tap(13);
 if(reload){
  lockEntry("wrong");lockEntry("Friday 73");lockEntry("5284");lockEntry("5284");
  tap(0x88);lockEntry("Friday 73");
  if(!require(has("\"locked\":true"),"old password no longer works"))return false;
  lockEntry("5284");screen(17);tap(13);lockEntry("5284");tap(13);
  tap(0x88);tap(0xa3);
  return require(!has("\"locked\":true"),"disable password and restore gesture");
 }
 lockEntry("123");lockEntry("Friday 73");lockEntry("mismatch");
 lockEntry("Friday 73");lockEntry("Friday 73");
 tap(0x88);snapshot("locked");
 auto lockPixels=[](){
  const uint16_t* frame=hardware.canvas->getBuffer();
  return std::vector<uint16_t>(frame+21*320,frame+240*320);
 };
 auto beforeMessage=lockPixels();
 ChatMessage received;received.source=peerId(0);received.destination=meshRadio.nodeId;
 strcpy(received.name,"Private sender");strcpy(received.text,"Private preview");
 notifications::incoming(received);fakeMillis+=250;uiTick();
 if(!require(beforeMessage==lockPixels(),"notification content hidden while locked"))return false;
 lockEntry("wrong");lockEntry("wrong");lockEntry("wrong");lockEntry("Friday 73");
 if(!require(has("\"locked\":true"),"retry delay"))return false;
 fakeMillis+=11000;uiTick();lockEntry("Friday 73");
 if(!require(!has("\"locked\":true"),"unlock after delay"))return false;
 fakeMillis+=(config.autoLock+1)*1000;uiTick();
 return require(has("\"locked\":true"),"automatic lock");
}
