// The desktop uses the same private record layout as NVS. Scenario runs use memory.
#include "HandheldPrivate.h"
#include <map>
static std::map<std::string,std::vector<uint8_t>> privateRecords;
bool handheldRead(const char* key,void* value,size_t bytes){
 memset(value,0,bytes);
#ifdef MM_DESKTOP_SIMULATOR
 if(!settingsFile.empty()){
  FILE* f=fopen((settingsFile+"."+key).c_str(),"rb");
  if(!f)return errno==ENOENT;
  bool ok=fread(value,1,bytes,f)==bytes&&fgetc(f)==EOF&&!ferror(f);
  fclose(f);return ok;
 }
#endif
 auto it=privateRecords.find(key);
 if(it==privateRecords.end())return true;
 if(it->second.size()!=bytes)return false;
 memcpy(value,it->second.data(),bytes);return true;
}
bool handheldWrite(const char* key,const void* value,size_t bytes){
#ifdef MM_DESKTOP_SIMULATOR
 if(!settingsFile.empty()){
  std::string path=settingsFile+"."+key,temp=path+".XXXXXX";
  int fd=mkstemp(&temp[0]);if(fd<0)return false;
  FILE* f=fdopen(fd,"wb");
  bool ok=false;
  if(f){ok=fwrite(value,1,bytes,f)==bytes;ok=fclose(f)==0&&ok;}else close(fd);
  if(ok&&rename(temp.c_str(),path.c_str())==0)return true;
  unlink(temp.c_str());return false;
 }
#endif
 const auto* b=static_cast<const uint8_t*>(value);
 privateRecords[key]=std::vector<uint8_t>(b,b+bytes);return true;
}
