// Host storage for the M9 settings editor. Demo contacts and messages are separate.
#include <cerrno>
#include <unistd.h>

static std::string settingsFile;

bool Config::save(){
  // Scenario runs and the initial load never write the user's preferences.
  if(settingsFile.empty())return true;
  DynamicJsonDocument doc(4096);
  if(deserializeJson(doc,configJson()))return false;
  if(trial.set())profileJson(doc.as<JsonObject>(),trial); // A trial reverts on restart, as on the M9.
  doc.remove("radio_trial");
  String json;
  if(!serializeJson(doc,json))return false;

  std::string temp=settingsFile+".XXXXXX";
  int fd=mkstemp(&temp[0]);
  if(fd<0)return false;
  FILE* out=fdopen(fd,"wb");
  bool ok=false;
  if(out){
    bool complete=fwrite(json.c_str(),1,json.length(),out)==json.length();
    ok=fclose(out)==0 && complete;
  }else close(fd);
  if(ok && rename(temp.c_str(),settingsFile.c_str())==0)return true;
  unlink(temp.c_str());
  return false;
}

static bool loadSimulatorSettings(const std::string& directory){
  const std::string path=directory+"settings.json";
  FILE* file=fopen(path.c_str(),"rb");
  if(file){
    char bytes[8193];
    size_t size=fread(bytes,1,sizeof bytes,file);
    bool complete=!ferror(file)&&size<sizeof bytes;
    fclose(file);
    DynamicJsonDocument doc(4096);
    if(!complete||deserializeJson(doc,bytes,size)||!doc.is<JsonObject>()){
      fprintf(stderr,"Cannot read simulator settings in %s\n",path.c_str());
      return false;
    }
    // Apply through the same validation as an edit, before enabling disk writes.
    String reply=applySettings(doc.as<JsonObjectConst>());
    if(!reply.startsWith("OK")){
      fprintf(stderr,"%s: %s\n",path.c_str(),reply.c_str());
      return false;
    }
    Config previous=config;
    JsonObjectConst saved=doc["radio_prev"];
    previous.frequency=saved["frequency"];previous.bandwidth=saved["bandwidth"];
    previous.sf=saved["sf"];previous.cr=saved["cr"];
    config.previous=previous.valid()?previous.radio():RadioProfile();
  }else if(errno==ENOENT){
    // The first simulator with a layout switch stored just this byte.
    if(FILE* legacy=fopen((directory+"launcher-layout").c_str(),"rb")){
      config.launcherTiles=fgetc(legacy)=='1';fclose(legacy);
    }
  }else{
    perror(path.c_str());
    return false;
  }
  settingsFile=path;
  return true;
}
