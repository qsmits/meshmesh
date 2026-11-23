#include "Board.h"
#include "HandheldPrivate.h"
#include <Preferences.h>
bool handheldRead(const char* key,void* value,size_t bytes){
 Preferences p;
 if(!p.begin("m9-private",false))return false;
 memset(value,0,bytes);
 return !p.isKey(key)||(p.getBytesLength(key)==bytes&&p.getBytes(key,value,bytes)==bytes);
}
bool handheldWrite(const char* key,const void* value,size_t bytes){
 Preferences p;
 return p.begin("m9-private",false)&&p.putBytes(key,value,bytes)==bytes;
}
