// The M9 text fields have an end cursor. Paste appends, and Replace edits the whole draft.
enum ClipAction {CopyText, PasteText, ReplaceText, CopyMessage, ShowLayout};
ClipAction clipAction=CopyText;
String clipboardText,clipMessage;
enum class PasteError {None, TooLong, InvalidText};
struct {
 PasteError reason=PasteError::None;
 unsigned bytes=0, limit=0;
} pasteError;

unsigned textByteLimit(){
 switch(page){
 case Memo:return selected?64:120;
 case Nodes:return 64;
 case Chat:return meshRadio.messageLimit(recipient);
 case Radio:case ChannelInfo:return regions::NameBytes;
 case Network:return 64;
 case NetList:return 63;
 case ChannelAdd:return channelEditLimit();
 case ServerHome:return serverEditLimit();
 case Remote:return remoteEditLimit();
 case DiceView:return diceEditLimit();
 case PeopleView:return people::NameBytes;
 case PetView:return 15;
 case ChessTourNew:return 32;
 default:return 24; // Device name
 }
}
String& activeText(){return editing?edit:composer;}
bool clipEnabled(ClipAction a){
 switch(a){
 case CopyText:return activeText().length();
 case PasteText:return clipboardText.length();
 case ReplaceText:return clipboardText.length()&&activeText().length();
 case CopyMessage:return clipMessage.length();
 case ShowLayout:return true;
 }
 return false;
}
void openClipboard(){
 clipMenu=true;lastSpace=0;toast="";clipMessage="";pasteError={};
 if(page==Chat){
  int skip=chatOffset;
  for(int i=int(meshRadio.historyCount)-1;i>=0;i--){
   const auto& message=meshRadio.history[i];
   if(belongs(message,recipient)&&skip--<=0){clipMessage=message.text;break;}
  }
 }
 clipAction=clipboardText.length()?PasteText:activeText().length()?CopyText:clipMessage.length()?CopyMessage:CopyText;
}
void lockScreen(){
 finishCredential();if(credentialEnabled())beginProtectedLock();
 locked=true;clipMenu=false;clipboardText="";clipMessage="";
}
void drawClipboard(){
 targetCount=hintCount=0;
 auto& d=g();
 d.fillRect(0,21,320,199,bg);
 text(13,42,t("Clipboard","Буфер обмена"),ink,bold);
 const char* labels[]={tr("Copy text","Копировать текст"),tr("Paste","Вставить"),tr("Replace text","Заменить текст"),tr("Copy message","Копировать сообщение"),tr("Layout","Раскладка")};
 int count=page==Chat?5:3;
 for(int row=0;row<count;row++){
  ClipAction a=ClipAction(row);
  int rowHeight=page==Chat?23:28;
  int y=50+row*rowHeight;
  bool enabled=clipEnabled(a);
  listRow(y,rowHeight-2,clipAction==a);
  text(17,y+18,String("CVRML"[row]),enabled?accent:faint,small);
  text(39,y+18,labels[row],enabled?ink:faint,body);
  if(a==CopyMessage){String preview=clipMessage;preview.replace("\n"," ");int x=51+measure(labels[row]);text(x,y+18,fit(preview,max(0,300-x),small),dim,small);}
  target(8,y,304,rowHeight-2,row);
 }
 if(pasteError.reason!=PasteError::None){
  panel(8,167,304,50,cardHi,6);
  g().fillRoundRect(8,172,3,40,1,warn);
  text(18,182,t("Paste failed","Не удалось вставить"),warn,bold);
  if(page==Display)textRight(300,182,t("Device name","Имя устройства"),dim,small);
  char counts[96];
  snprintf(counts,sizeof counts,tr("%u bytes (limit %u)","%u байт (лимит %u)"),pasteError.bytes,pasteError.limit);
  text(18,199,counts,ink);
  if(pasteError.reason==PasteError::InvalidText)
   text(18,212,fit(t("Invalid text for this field","Недопустимый текст для поля"),282,small),dim,small);
 }else if(!clipboardText.length())text(13,209,t("Clipboard empty","Буфер пуст"),dim,small);
 footer({{"OK",t("Choose","Выбрать")},{"^v",t("Select","Выбор")},{"BACK",t("Back","Назад")}});
}
void clipboardKey(int key){
 int count=page==Chat?5:3;
 if(key==KeyBack||key==Erase||key==KeyHold){clipMenu=false;clipMessage="";return;}
 if(key==KeyDown||key==KeyUp){clipAction=ClipAction((int(clipAction)+count+(key==KeyDown?1:-1))%count);return;}
 if(key!=Enter){
  if(key>='a'&&key<='z')key-=32;
  const char* keys="CVRML";
  int i=0;while(i<count&&keys[i]!=key)i++;
  if(i==count)return;
  clipAction=ClipAction(i);
 }
 if(!clipEnabled(clipAction))return;
 pasteError={};
 String& field=activeText();
 switch(clipAction){
 case CopyText:case CopyMessage:
  clipboardText=clipAction==CopyMessage?clipMessage:field;
  notice(t("Copied","Скопировано"),ok);
  break;
 case PasteText:case ReplaceText:{
  unsigned bytes=clipboardText.length()+(clipAction==ReplaceText?0:field.length());
  unsigned limit=textByteLimit();
  if(bytes>limit){pasteError={PasteError::TooLong,bytes,limit};return;}
  if(!meshmesh::validUtf8((const uint8_t*)clipboardText.c_str(),clipboardText.length())||
     (editing&&(clipboardText.indexOf('\n')>=0||clipboardText.indexOf('\t')>=0))){
   pasteError={PasteError::InvalidText,bytes,limit};return;
  }
  if(clipAction==ReplaceText)field=clipboardText;else field+=clipboardText;
  break;
 }
 case ShowLayout:layoutHelp=true;break;
 }
 clipMenu=false;clipMessage="";
}
