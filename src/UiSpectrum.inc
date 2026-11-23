// LR1110 received power. The preview feeds the same display and controls with synthetic samples.
namespace spectrum {
constexpr int bins=113,lines=24,history=56;
constexpr float low=863.0f,high=870.0f;
constexpr unsigned interval=250;
int8_t waterfall[lines][bins],pending[bins],channel[history];
unsigned passesLeft=0;
unsigned rows=0,used=0,channelHead=0,bandHead=0,bin=0;
uint32_t completed=0,last=0,progressAt=0;
bool waiting=false,band=false,paused=false,running=false,failed=false;
float channelFreq=0;
float frequency(unsigned i){return low+i*0.0625f;}
int xAt(int i,int length){return 39+275*i/(length-1);}
int chartY(int level){return 135-(constrain(level,-128,-48)+128)*60/80;}

enum class Reading {Valid,Busy,Error};
Reading sample(bool sweep,float mhz,int8_t& level){
#ifdef MM_UI_PREVIEW
 if(sweep&&netfind::running())return Reading::Busy;
 uint32_t tick=millis()/interval;
 int i=int(roundf((mhz-low)*16));
 uint32_t noise=tick*2654435761u+uint32_t(i)*2246822519u;
 int peak=max(0,44-abs(i-106)*7);
 level=-117+int((noise>>24)%9)+(tick%24<15?peak:0);
 return Reading::Valid;
#else
 auto result=meshRadio.spectrumSample(sweep,mhz,level);
 if(result==MeshRadio::SampleResult::Busy)return Reading::Busy;
 return result==MeshRadio::SampleResult::Ok?Reading::Valid:Reading::Error;
#endif
}
}
void stopSweep(){spectrum::waiting=false;spectrum::running=false;spectrum::bin=0;spectrum::passesLeft=0;}
void resetSpectrum(){
 using namespace spectrum;
 stopSweep();band=paused=failed=false;used=rows=channelHead=bandHead=completed=last=0;
 channelFreq=config.frequency;
}

bool advanceSpectrum(uint32_t now){
 using namespace spectrum;
 if(channelFreq!=config.frequency){resetSpectrum();return true;}
 if((band&&!running)||(!band&&paused)||now-last<(band&&!waiting?35u:interval))return false;
 last=now;
 int8_t level=0;
 Reading result=sample(band,band?frequency(bin):channelFreq,level);
 if(result!=Reading::Valid){
  if(band&&result==Reading::Error){stopSweep();notice("Sweep stopped: radio error",warn);}
  else if(band&&now-progressAt>=750)waiting=true;
  failed=result==Reading::Error;return true;
 }
 waiting=failed=false;
 if(!band){
  channel[channelHead]=level;channelHead=(channelHead+1)%history;if(used<history)used++;
 }else{
  pending[bin++]=level;progressAt=now;
  if(bin==bins){
   memcpy(waterfall[bandHead],pending,sizeof pending);
   bandHead=(bandHead+1)%lines;if(rows<lines)rows++;
   completed++;bin=0;
   if(--passesLeft==0)stopSweep();
  }
 }
 return true;
}
bool spectrumKey(int key){
 using namespace spectrum;
 if(key==KeyLeft||key==KeyRight){stopSweep();band=!band;failed=false;used=channelHead=0;return true;}
 if(band&&(key=='s'||key=='S'||key=='w'||key=='W')){
  if(running)return true;
  if(channelFreq<low||channelFreq>high){notice("Requires an 863–870 MHz mesh profile",warn);return true;}
  passesLeft=(key=='w'||key=='W')?lines:1;
  bin=0;running=true;failed=false;progressAt=millis();return true;
 }
 if(key==Enter||key=='p'||key=='P'){
  if(band)stopSweep();else paused=!paused;
  return true;
 }
 return false;
}
void spectrumStatus(JsonDocument& d){
 using namespace spectrum;
 d["band_sweep"]=band;d["sweep_running"]=running;d["sweep_waiting"]=waiting;d["sweep_bin"]=bin;d["sweeps_completed"]=completed;
#ifndef MM_UI_PREVIEW
 if(waiting)d["sweep_reason"]=meshRadio.spectrumWaitReason;
#endif
 d["channel_paused"]=paused;d["spectrum_samples"]=used;
 if(used)d["channel_dbm"]=channel[(channelHead+history-1)%history];
 if(rows){
  const int8_t* values=waterfall[(bandHead+lines-1)%lines];int peak=0;
  for(int i=1;i<bins;i++)if(values[i]>values[peak])peak=i;
  d["sweep_peak_dbm"]=values[peak];d["sweep_peak_mhz"]=frequency(peak);
 }
}
namespace spectrum {
// Each swatch spans 2 dB. The same power keeps the same colour across scans and UI themes.
constexpr uint16_t ramp[]={
 rgb(0x191d48),rgb(0x292465),rgb(0x3b2988),rgb(0x49339e),rgb(0x4542b9),rgb(0x3a53ce),
 rgb(0x2b66dc),rgb(0x167be0),rgb(0x0b8edc),rgb(0x099fd1),rgb(0x0eafbe),rgb(0x23bca5),
 rgb(0x43c78b),rgb(0x67d075),rgb(0x91d660),rgb(0xb6dc52),rgb(0xd9e051),rgb(0xf4dd50),
 rgb(0xffcc46),rgb(0xffb43c),rgb(0xff9936),rgb(0xfe7935),rgb(0xfb5838)
};
constexpr int colours=sizeof(ramp)/sizeof(*ramp);
void waterfallRow(const int8_t* values,int count,int y){
 for(int i=0;i<count;i++){
  int x=10+i*300/bins,end=10+(i+1)*300/bins;
  g().fillRect(x,y,end-x,4,ramp[constrain((int(values[i])+126)/2,0,colours-1)]);
 }
}
void drawWaterfall(){
 text(10,44,"Band waterfall",ink,big);
 String state=waiting?String("WAIT · ")+String(bin)+"/"+String(bins):running?String(bin)+"/"+String(bins):completed?String(completed)+" scans":"IDLE";
#ifdef MM_UI_PREVIEW
 state="DEMO / "+state;
#endif
 textRight(309,42,state,running?accent:dim,small);
 const int top=72;
 text(10,65,"863 MHz",dim,small);textCenter(160,65,"866.5",dim,small);textRight(310,65,"870 MHz",dim,small);
 g().fillRect(10,top,300,lines*4,rgb(0x101325));
 const bool partial=running&&bin;
 if(partial)waterfallRow(pending,bin,top);
 for(int i=0;i<int(rows)&&i+int(partial)<lines;i++)
  waterfallRow(waterfall[(bandHead+lines-1-i)%lines],bins,top+4*(i+int(partial)));
 if(!partial&&!rows){
  textCenter(160,112,"S for one scan  ·  W for 24 scans",rgb(0xe9ecf3),bold);
  textCenter(160,136,"Mesh packets may be missed during scans",rgb(0xadb9cb),small);
 }
 text(10,185,"-126",dim,small);
 for(int i=0;i<colours;i++)g().fillRect(44+i*9,176,9,9,ramp[i]);
 textRight(310,185,"-82 dBm",dim,small);
 if(waiting){
  text(10,207,"Waiting for mesh radio",warn,bold);
 }else if(rows){
  const int8_t* row=waterfall[(bandHead+lines-1)%lines];
  int peak=0;for(int i=1;i<bins;i++)if(row[i]>row[peak])peak=i;
  text(10,207,"Peak  "+String(frequency(peak),3)+" MHz",ink,bold);
  textRight(310,207,String(int(row[peak]))+" dBm",accent,bold);
 }
 if(running)footer({{"OK","Stop"},{"<>","Channel"},{"BACK",t("Back","Назад")}});
 else footer({{"S","Scan once"},{"W","Waterfall"},{"<>","Channel"}});
}
}
void drawSpectrum(){
 using namespace spectrum;
 if(band){drawWaterfall();return;}
 text(12,44,"Channel history",ink,big);
 String state=paused?"PAUSED":"LIVE";
#ifdef MM_UI_PREVIEW
 state="DEMO  /  "+state;
#endif
 textRight(308,42,state,dim,small);
 text(12,64,String(channelFreq,3)+" MHz",accent,bold);
 for(int level:{-128,-88,-48}){
  int y=chartY(level);g().drawFastHLine(39,y,276,line);textRight(34,y+4,String(level),faint,small);
 }
 int peakLevel=-129,py=0,px=0;
 for(unsigned i=0;i<used;i++){
  int v=channel[(channelHead+history-used+i)%history];
  peakLevel=max(peakLevel,v);
  int x=xAt(history-used+i,history),y=chartY(v);
  if(i)g().drawLine(px,py,x,y,accent);else g().drawPixel(x,y,accent);
  px=x;py=y;
 }
 text(39,156,"Older",dim,small);textRight(313,156,"Latest",dim,small);
 if(failed||!used)text(12,197,failed?"Radio read unavailable":"Waiting for receiver",dim,bold);
 else text(12,197,"Latest  "+String(int(channel[(channelHead+history-1)%history]))+" dBm",ink,bold);
 if(used)textRight(308,197,"Peak  "+String(peakLevel)+" dBm",accent,small);
 footer({{"<>","Waterfall"},{"OK",paused?"Resume":"Pause"},{"BACK",t("Back","Назад")}});
}
