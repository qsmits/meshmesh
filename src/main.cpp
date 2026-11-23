#include "Version.h"
#include <Arduino.h>
#include "App.h"
#include "Config.h"
#include "Hardware.h"
#include "Palette.h"
#include "MeshRadio.h"
#include "BleDiagnostics.h"
#include "Maps.h"
#include "Navigation.h"
#include "Radar.h"
#include "WifiDiagnostics.h"
#include "Internet.h"
#include "Board.h"
#include "Power.h"
#include "Companion.h"
#include "QuickSend.h"
#include "Notifications.h"
#include "People.h"
#include "Wardrive.h"
#include <Wire.h>
#include <esp_system.h>
#include <memory>
#include <new>
#include <utility>
namespace {
uint8_t* usbBytes=nullptr;
std::unique_ptr<String> usbText;
bool usbBusy(){return usbBytes||usbText
;}
size_t usbSize=0,usbOffset=0;
unsigned usbBaud=115200,pendingBaud=0;uint32_t baudExpires=0;
// MeshCore companion apps over USB: '<', a 16-bit length and the frame in; '>', the length and the frame out.
// A text command on the port ends that link, so the frames never mix with text replies.
uint8_t appFrame[companion::MaxFrame];int appState=-1;size_t appLength=0,appGot=0; // -1: text; 0, 1: length; 2: frame
void usbFrame(const uint8_t* frame,size_t length) {
  usbSize=length+3;usbOffset=0;usbBytes=(uint8_t*)malloc(usbSize);if(!usbBytes){usbSize=0;return;}
  usbBytes[0]='>';usbBytes[1]=length&255;usbBytes[2]=length>>8;memcpy(usbBytes+3,frame,length);
}
void usbLine(String value) {
  // Own the command's existing string instead of allocating a second full JSON reply.
  usbText.reset(new(std::nothrow) String(std::move(value)));usbOffset=0;
  if(!usbText){usbSize=0;Serial.println("ERR USB output allocation");return;}
  usbSize=usbText->length()+1; // newline is sent separately, without growing the buffer
}
void usbScreenshot() {
  unsigned width=hardware.canvas->width(),height=hardware.canvas->height(),size=width*height*2;
  char header[64];unsigned length=snprintf(header,sizeof(header),"RGB565 %u %u %u\n",width,height,size);
  usbSize=length+size+1;usbOffset=0;usbBytes=(uint8_t*)malloc(usbSize);
  if(!usbBytes) {usbSize=0;Serial.println("ERR screenshot allocation");return;}
  memcpy(usbBytes,header,length);
  memcpy(usbBytes+length,hardware.canvas->getBuffer(),size);usbBytes[usbSize-1]='\n';
}
void usbTick() {
  if(!usbBusy())return;
  int available=Serial.availableForWrite();if(available<=0)return;
  size_t count=usbSize>=usbOffset?min(size_t(available),min(size_t(256),usbSize-usbOffset)):0;
  if(usbText){
    if(usbOffset<usbText->length())usbOffset+=Serial.write((const uint8_t*)usbText->c_str()+usbOffset,min(count,size_t(usbText->length()-usbOffset)));
    else usbOffset+=Serial.write(uint8_t('\n'));
    if(usbOffset==usbSize){usbText.reset();usbSize=usbOffset=0;}return;
  }
  usbOffset+=Serial.write(usbBytes+usbOffset,count);
  if(usbOffset==usbSize) {free(usbBytes);usbBytes=nullptr;usbSize=usbOffset=0;}
}
}
#if defined(MM_BOOT_TRACE)
// Diagnostic builds only (-D MM_BOOT_TRACE): wait for the USB host, print each start step, and
// before storage is mounted serve "flashread ADDR LEN" until "go", so storage can be saved first,
// and "flashwrite ADDR HEX" inside the storage and core InternalFS (0xD4000-0xF4000), so a saved
// copy (tools/nrf52_dump.py --write) can be put back byte for byte, which UF2 cannot do past 0xEA000.
#define BOOT(step) do{Serial.println("BOOT " step);Serial.flush();delay(30);}while(0)
static void bootWindow(){
  uint32_t t=millis();while(!Serial&&millis()-t<20000)delay(10);delay(200);
  Serial.println("BOOT trace: flashread ADDR LEN, flashwrite ADDR HEX, go");String line;t=millis();
  while(millis()-t<60000){
    if(!Serial.available()){delay(2);continue;}char c=Serial.read();if(c!='\n'){if(c!='\r')line+=c;continue;}
    if(line=="go")break;
    if(line.startsWith("flashread ")){uint32_t at=strtoul(line.c_str()+10,nullptr,16),n=0;int sp=line.indexOf(' ',10);if(sp>0)n=strtoul(line.c_str()+sp+1,nullptr,16);
      if(n&&n<=2048&&at>=0x1000&&at+n<=0x100000){String h="FLASH "+String(at,HEX)+" ";for(uint32_t i=0;i<n;i++){uint8_t v=*(const uint8_t*)(at+i);h+="0123456789abcdef"[v>>4];h+="0123456789abcdef"[v&15];}Serial.println(h);}
      else Serial.println("ERR flashread");t=millis();}
    line="";
  }
}
#else
#define BOOT(step) do{}while(0)
#endif
void appSetup() {
  powerBootCheck(); // woken from power off by a short press: back to sleep
  Serial.setRxBufferSize(2048);Serial.setTxBufferSize(2048);Serial.begin(115200);delay(300);
  Serial.printf("\n" MESHMM_FIRMWARE " / " MM_BOARD_NAME " / reset=%d\n",esp_reset_reason());
  BOOT("config");config.load();BOOT("clock");hardware.beginClock();BOOT("hardware");hardware.begin();
  BOOT("radio");meshRadio.begin();BOOT("maps");maps.begin();
  internet.begin(); // a server keeps Wi-Fi for the device page only
  quickSend::begin();people::begin();wardrive::begin();BOOT("navigation");navigation.begin();BOOT("portal");portalBegin();BOOT("ui");uiBegin();
  // Bluetooth left on comes back after a restart (power, RESET, auto-reset of a USB-UART bridge), unless the restart was a crash.
  {esp_reset_reason_t r=esp_reset_reason();if(config.bleOn&&r!=ESP_RST_PANIC&&r!=ESP_RST_INT_WDT&&r!=ESP_RST_TASK_WDT&&r!=ESP_RST_WDT){BOOT("ble");bleToggle();}}
  BOOT("selftest");
  Serial.println(meshRadio.selfTest()?"SELFTEST crypto/UTF-8/tamper PASS":"SELFTEST FAIL");
  Serial.println("READY: USB commands are available; type help");
}
void appLoop() {
  hardware.tick();meshRadio.tick();
  internet.tick();
  maps.tick();navigation.tick();radar.tick();people::tick();wardrive::tick();notifications::tick();
  int key=hardware.readKey();if(key){powerWake();uiKey(key);}
  static String command;
  unsigned budget=256;
  if(!usbBusy()&&Serial.available())powerWake();
  while(!usbBusy() && Serial.available() && budget--) {
    char c=Serial.read();baudExpires=millis()+10000;
    if(appState>=0) {
      uint8_t b=c;
      if(appState==0){appLength=b;appState=1;}
      else if(appState==1){appLength|=size_t(b)<<8;appGot=0;appState=appLength?2:-1;}
      else{if(appGot<sizeof(appFrame))appFrame[appGot]=b;if(++appGot>=appLength){appState=-1;if(appLength<=sizeof(appFrame))companion::command(appFrame,appLength,companion::LinkUsb);}}
      continue;
    }
    if(c=='<'&&!command.length()){appState=0;continue;}
    if(c=='\n') {
      companion::disconnected(companion::LinkUsb);
      if(command.startsWith("baud ")) {
        unsigned rate=command.substring(5).toInt();if(rate==115200||rate==460800||rate==921600){pendingBaud=rate;usbLine("OK USB baud switching");}else usbLine("ERR baud 115200/460800/921600");
      }
      else if(command=="screenshot")usbScreenshot();
      // Research: raw CSI lines ("CSI us rssi iqhex") while on; beacon frames per second.
      else if(command=="csistream on"||command=="csistream off"){radar.csiStream=command.endsWith("on");usbLine(radar.csiStream?"OK CSI stream on":"OK CSI stream off");}
      else if(command.startsWith("csirate ")){radar.setBeaconHz(command.substring(8).toInt());usbLine("OK beacon "+String(radar.beaconHz)+" Hz");}
      else if(command=="bleprobe")usbLine(bleProbeResult());
      else if(command=="wifiprobe")usbLine(wifiProbeResult());
      else if(command.startsWith("wifiprobe ")){StaticJsonDocument<512>d;if(deserializeJson(d,command.substring(10)))usbLine("ERR Wi-Fi probe JSON");else usbLine(startWifiProbe(d.as<JsonObjectConst>()));}
      else if(command.startsWith("bleprobe ")) {
        StaticJsonDocument<512> options;
        if(deserializeJson(options,command.substring(9)) || !options.is<JsonObject>())usbLine("ERR bleprobe JSON");
        else usbLine(startBleProbe(options.as<JsonObjectConst>()));
      }
      // M9 GPS bring-up: "gps raw" shows about a second of receiver output (escaped), "gps baud N" changes the UART rate.
      else if(command=="gps raw"){if(!config.gps){usbLine("ERR GPS off");}else{String r="GPS ";uint32_t start=millis();while(millis()-start<1200&&r.length()<700){while(Serial1.available()&&r.length()<700){int c=Serial1.read();if(c=='\r')r+="\\r";else if(c=='\n')r+="\\n";else if(c>=32&&c<127)r+=char(c);else{char h[5];snprintf(h,sizeof h,"\\x%02x",c);r+=h;}}delay(2);}usbLine(r);}}
      else if(command.startsWith("gps baud ")){long b=command.substring(9).toInt();if(b<1200||b>921600||!config.gps)usbLine("ERR gps baud 1200..921600, GPS on");else{Serial1.updateBaudRate(b);usbLine("OK gps baud "+String(b));}}
      else if(command.startsWith("uikey ")) {uiKey(strtol(command.substring(6).c_str(),nullptr,0));usbLine("OK UI key");}
      // "uitouch t|h X Y" or "uitouch u|d|l|r": the T-Deck touch gestures, for checks without a finger.
      else if(command.startsWith("uitouch ")&&command.length()>8&&strchr("thudlr",command[8])) {int x=0,y=0;sscanf(command.c_str()+9,"%d %d",&x,&y);uiTouch(command[8],x,y);usbLine("OK UI touch");}
      else usbLine(executeCommand(command));command="";
    }
    else if(c!='\r' && command.length()<1024)command+=c;
    else if(command.length()>=1024) {command="";usbLine("ERR command too long");}
  }
  portalTick();uiTick();usbTick();
  if(!usbBusy()){uint8_t frame[companion::MaxFrame];size_t length=companion::next(frame,companion::LinkUsb);if(length)usbFrame(frame,length);}
  restartTick();powerOffTick();
  if(radar.csiStream&&!usbBusy()){String line;for(int i=0;i<8&&Serial.availableForWrite()>=240&&radar.streamLine(line);i++)Serial.println(line);}
  if(!usbBusy()&&(pendingBaud||(usbBaud!=115200&&int32_t(millis()-baudExpires)>=0))){Serial.flush();usbBaud=pendingBaud?pendingBaud:115200;pendingBaud=0;Serial.updateBaudRate(usbBaud);baudExpires=millis()+10000;}
  powerTick(!usbBusy()&&!pendingBaud&&usbBaud==115200);
}
void setup() {appSetup();}
void loop() {appLoop();}
