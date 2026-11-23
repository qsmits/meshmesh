// Keyboard action sheet. The underlying page keeps its cursor, draft and scroll position.
void gpsToggle();
void toggleLanguage();
enum class AppAction {Add,Unread,Channel,Language,Clipboard,Maps,Filter,Notes,List,Pause,Motion,Radios,Wifi,Hotspot,Ble,Gps,Advert,NodeMap,ZoomIn,ZoomOut,Recenter,QuickReply};
struct ActionItem {AppAction action; Icon icon; String label; bool on=false;};

int actionItems(ActionItem* rows){
 using A=AppAction;
 int n=0;
 if(activePanel==Panel::Radios){
  rows[n++]={A::Wifi,IcWifi,t("Wi-Fi client","Wi-Fi-клиент"),internet.enabled};
  rows[n++]={A::Hotspot,IcWifi,t("Wi-Fi access point","Точка доступа Wi-Fi"),portalActive()};
  rows[n++]={A::Ble,IcBle,"Bluetooth LE",bleActive()};
  rows[n++]={A::Gps,IcPin,"GPS",config.gps};
  return n;
 }
 if(page==Threads){
  rows[n++]={A::Add,IcHash,t("Add a channel","Добавить канал")};
  rows[n++]={A::Unread,IcMail,t("Next unread","К непрочитанному")};
  if(conversationCount&&channels::isChannel(conversations[selected]))rows[n++]={A::Channel,IcHash,t("Channel details","Сведения о канале")};
 }else if(page==Chat){
  rows[n++]={A::Language,IcScreen,t("Input language","Язык ввода")+String("  ")+(keyboardRussian?"RU":"EN")};
  rows[n++]={A::Clipboard,IcMail,t("Clipboard","Буфер обмена")};
  if(!composer.length())rows[n++]={A::QuickReply,IcSend,t("Quick send","Быстрая отправка")};
  if(channels::isChannel(recipient))rows[n++]={A::Channel,IcHash,t("Channel details","Сведения о канале")};
 }else if(page==Scope){
  rows[n++]={A::List,IcRadar,scopeWide?t("Radar","Радар"):t("Full-width list","Список на весь экран")};
  rows[n++]={A::Pause,IcRadar,scopeHeld?t("Resume","Продолжить"):t("Pause","Пауза")};
  rows[n++]={A::Motion,IcPulse,t("Motion","Движение")};
 }else if(page==Map){
  rows[n++]={A::Recenter,IcPin,t("My location","Моя позиция")};
  rows[n++]={A::ZoomIn,IcCompass,t("Zoom in","Приблизить")};
  rows[n++]={A::ZoomOut,IcCompass,t("Zoom out","Отдалить")};
  rows[n++]={A::Maps,IcFolder,t("Saved maps","Сохранённые карты")};
 }else if(page==Nodes){
  rows[n++]={A::Advert,IcRadio,t("Advertise my node","Объявить мой узел")};
  rows[n++]={A::Filter,IcMesh,t("Filter by tag","Фильтр по метке")};
  const Peer* peer=nodeTotal?focusedPeer():nullptr;
  if(peer&&peer->position)rows[n++]={A::NodeMap,IcPin,t("Show on map","Показать на карте")};
 }
 else if(page==Node&&focusedPeer())rows[n++]={A::Notes,IcMesh,t("Contact notes","Заметки контакта")};
 rows[n++]={A::Radios,IcRadio,t("Radio controls","Управление связью")};
 return n;
}

void openActionPanel(){
 activePanel=Panel::Actions;panelRow=0;
 ActionItem items[5];
 if(actionItems(items)==1){activePanel=Panel::Radios;radioReturnRow=-1;}
}

void drawActionPanel(){
 ActionItem items[5];int n=actionItems(items);
 panelRow=constrain(panelRow,0,n-1);
 bool radios=activePanel==Panel::Radios;
 text(14,47,radios?t("Radio controls","Управление связью"):t("Actions","Действия"),ink,big);
 for(int i=0;i<n;i++){
  int y=56+i*34;bool focus=i==panelRow;
  listRow(y,31,focus);target(8,y,304,31,i);
  icon(items[i].icon,28,y+15,8,focus?accent:dim,focus?cardHi:bg);
  text(48,y+20,fit(items[i].label,200,bold),ink,bold);
  if(radios){textRight(262,y+20,flag(items[i].on),dim,small);toggle(272,y+7,items[i].on);}
  else tri(297,y+15,1,4,focus?accent:faint);
 }
 if(radios)text(15,209,String("LoRa  ")+String(config.frequency,3)+" MHz",dim,small);
 footer({{"OK",radios?t("Toggle","Переключить"):t("Choose","Выбрать")},{"BACK",t("Back","Назад")}});
}

bool actionPanelKey(int key){
 if(key==KeyHome||key==KeySet||key==KeyMap||key==KeyMsg){activePanel=Panel::Closed;return false;}
 if(key==KeyBack||key==KeyMenu){
  if(activePanel==Panel::Radios&&key==KeyBack&&radioReturnRow>=0){activePanel=Panel::Actions;panelRow=radioReturnRow;}
  else activePanel=Panel::Closed;
  return true;
 }
 ActionItem rows[5];int n=actionItems(rows);
 panelRow=constrain(panelRow,0,n-1);
 if(key==KeyUp||key==KeyDown){panelRow=(panelRow+(key==KeyUp?n-1:1))%n;return true;}
 if(key!=Enter)return true;
 using A=AppAction;
 A choice=rows[panelRow].action;
 if(activePanel==Panel::Actions)activePanel=Panel::Closed;
 switch(choice){
 case A::Advert:uiKey(KeyAdv);break;
 case A::NodeMap:uiKey('p');break;
 case A::ZoomIn:uiKey('+');break;
 case A::ZoomOut:uiKey('-');break;
 case A::Recenter:uiKey(Enter);break;
 case A::QuickReply:uiKey(KeyRight);break;
 case A::Add:change(ChannelAdd);break;
 case A::Unread:nextUnread();break;
 case A::Channel:openChannelInfo(page==Chat?recipient:conversations[selected],page);break;
 case A::Language:toggleLanguage();break;
 case A::Clipboard:openClipboard();break;
 case A::Maps:change(Library);break;
 case A::Filter:edit=tagFilter;editing=true;break;
 case A::Notes:change(Memo);break;
 case A::List:uiKey('l');break;
 case A::Pause:uiKey('p');break;
 case A::Motion:change(Motion);break;
 case A::Radios:radioReturnRow=panelRow;activePanel=Panel::Radios;panelRow=0;break;
 case A::Wifi:internet.setEnabled(!internet.enabled);break;
 case A::Hotspot:portalToggle();break;
 case A::Ble:bleToggle();break;
 case A::Gps:gpsToggle();break;
 }
 return true;
}
#include "UiSpectrum.inc"
