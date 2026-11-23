enum class CredentialStep {Idle, Current, New, Confirm, Unlock};
CredentialStep credentialStep=CredentialStep::Idle;
ScreenCredential credential,pendingCredential;
char phrase[49]={};
unsigned phraseLength=0,wrongAttempts=0;
uint32_t retryAt=0;
String credentialMessage;
bool credentialEnabled(){return credential.version!=0;}
void clearPhrase(){memset(phrase,0,sizeof phrase);phraseLength=0;}
void credentialPrompt(CredentialStep next){
 clearPhrase();credentialMessage="";credentialStep=next;
}
void finishCredential(){
 credentialPrompt(CredentialStep::Idle);pendingCredential={};
}
void beginProtectedLock(){
 credentialPrompt(CredentialStep::Unlock);pendingCredential={};toast="";
}
void drawCredential(){
 auto& d=g();targetCount=hintCount=0;
 d.fillScreen(bg);
 String title=credentialStep==CredentialStep::Unlock?t("Unlock","Разблокировать"):t("Screen password","Пароль экрана");
 text(16,42,title,ink,bold);
 String prompt=credentialStep==CredentialStep::Confirm?t("Repeat password","Повторите пароль"):credentialStep==CredentialStep::New?t("New password","Новый пароль"):t("Enter password","Введите пароль");
 text(16,77,prompt,dim);
 panel(12,88,296,36,cardHi,6);ring(12,88,296,36,6,accent);
 String mask;for(unsigned i=0;i<phraseLength;i++)mask+='*';
 text(22,111,fit(mask,274),ink);
 String hint=credentialStep==CredentialStep::New&&credentialEnabled()?t("Empty disables the lock","Пусто — отключить пароль"):t("4–48 characters","4–48 символов");
 text(16,147,credentialMessage.length()?credentialMessage:hint,credentialMessage.length()?warn:dim,small);
 footer({{"OK",t("Confirm","Подтвердить")},{"DEL",t("Delete","Удалить")}});
}
void storeCredential(const ScreenCredential& value){
 if(!handheldWrite("lock",&value,sizeof value)){credentialMessage=t("Save failed","Ошибка записи");return;}
 credential=value;finishCredential();notice(t("Settings saved","Настройки сохранены"),ok);
}
void credentialKey(int key){
 hardware.lastKey=0;
 if(key==KeyBack){if(locked)credentialPrompt(CredentialStep::Unlock);else finishCredential();return;}
 if(key==Erase){if(phraseLength)phrase[--phraseLength]=0;return;}
 if(key>=32&&key<127){if(phraseLength<sizeof phrase-1)phrase[phraseLength++]=char(key);return;}
 if(key!=Enter)return;
 if(credentialStep==CredentialStep::Current||credentialStep==CredentialStep::Unlock){
  if(retryAt&&int32_t(retryAt-millis())>0){credentialMessage=t("Try again later","Попробуйте позже");clearPhrase();return;}
  if(!credential.matches(phrase)){
   if(++wrongAttempts>=3){retryAt=millis()+10000;wrongAttempts=0;}
   clearPhrase();credentialMessage=t("Incorrect password","Неверный пароль");return;
  }
  retryAt=wrongAttempts=0;
  if(locked){locked=false;finishCredential();toast="";}else credentialPrompt(CredentialStep::New);
 }else if(credentialStep==CredentialStep::New){
  if(!phraseLength&&credentialEnabled()){storeCredential(ScreenCredential{});return;}
  if(phraseLength<4){credentialMessage=t("4–48 characters","4–48 символов");return;}
  if(!pendingCredential.create(phrase)){clearPhrase();credentialMessage=t("Save failed","Ошибка записи");return;}
  credentialPrompt(CredentialStep::Confirm);
 }else if(credentialStep==CredentialStep::Confirm){
  if(pendingCredential.matches(phrase))storeCredential(pendingCredential);
  else{credentialPrompt(CredentialStep::New);credentialMessage=t("Passwords differ","Пароли не совпадают");}
 }
}
