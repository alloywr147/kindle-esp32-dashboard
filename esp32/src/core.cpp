#include "app.h"
#include <math.h>

DynamicJsonDocument state(32768);
Preferences preferences;
String setupPassword, setupSsid;
bool fsReady = false, apActive = false, weatherRequested = false;
unsigned long restartAt = 0;
volatile int wifiDisconnectReason=0;
static String storagePrefix = "";

bool clockValid() { return time(nullptr) >= 1767196800LL; }
String localDate(time_t at) {
  if (!at) at = time(nullptr);
  struct tm info; localtime_r(&at, &info);
  char output[24]; strftime(output, sizeof(output), "%Y-%m-%d", &info); return output;
}
String localStamp(time_t at) {
  if (!at) at = time(nullptr);
  struct tm info; localtime_r(&at, &info);
  char output[32]; strftime(output, sizeof(output), "%Y-%m-%d %H:%M", &info); return output;
}
int unicodeLength(const String& text) {
  int count = 0;
  const auto* p = reinterpret_cast<const uint8_t*>(text.c_str());
  for (size_t i=0; i<text.length();) {
    uint8_t c=p[i]; int n=0; uint32_t cp=0;
    if (c<0x80) { n=1; cp=c; }
    else if (c>=0xc2 && c<=0xdf) {n=2;cp=c&31;}
    else if (c>=0xe0 && c<=0xef) {n=3;cp=c&15;}
    else if (c>=0xf0 && c<=0xf4) {n=4;cp=c&7;}
    else return -1;
    if (i+n>text.length()) return -1;
    for(int j=1;j<n;j++) {if((p[i+j]&0xc0)!=0x80)return -1;cp=(cp<<6)|(p[i+j]&63);}
    if ((n==2&&cp<128)||(n==3&&cp<2048)||(n==4&&cp<65536)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||cp<32||cp==127) return -1;
    count++;i+=n;
  }
  return count;
}
String escapeHtml(const String& value) {
  String out; out.reserve(value.length()+24);
  for(size_t i=0;i<value.length();i++) switch(value[i]) {
    case '&':out+="&amp;";break; case '<':out+="&lt;";break;case '>':out+="&gt;";break;
    case '"':out+="&quot;";break;case '\'':out+="&#39;";break;default:out+=value[i];
  }
  return out;
}
String jsonText(JsonDocument& doc) {String out;serializeJson(doc,out);return out;}
Reply errorReply(int code,const char* message) {DynamicJsonDocument d(512);d["detail"]=message;return {code,"application/json; charset=utf-8",jsonText(d)};}
static void defaults() {
  state.clear();state["schema"]=1;state["revision"]=0;state["next_id"]=1;
  state["settings"]["mode"]="today";state["settings"]["city"]="苏州";state.createNestedArray("tasks");
}
static bool stateValid() {
  if (state.overflowed()||state["schema"]!=1||!state["tasks"].is<JsonArray>()||state["tasks"].size()>MAX_TASKS)return false;
  String mode=state["settings"]["mode"]|"";
  if(mode!="today"&&mode!="almanac"&&mode!="weather")return false;
  if(!state["revision"].is<uint32_t>()||!state["next_id"].is<uint32_t>())return false;
  uint32_t maxId=0;
  for(JsonObject task:state["tasks"].as<JsonArray>()) {
    if(!task["id"].is<uint32_t>()||!task["text"].is<const char*>()||!task["done"].is<bool>())return false;
    uint32_t id=task["id"]; if(id==0)return false;
    for(JsonObject other:state["tasks"].as<JsonArray>())if(other!=task&&other["id"].as<uint32_t>()==id)return false;
    int length=unicodeLength(task["text"].as<String>());if(length<1||length>MAX_TEXT_CHARS)return false;
    if(id>maxId)maxId=id;
  }
  return state["next_id"].as<uint32_t>()>maxId;
}
bool loadState() {
  for(const char* suffix:{"/state.json","/state.bak"}) {
    File f=LittleFS.open(storagePrefix+suffix,"r");
    if(!f)continue;
    auto err=deserializeJson(state,f);f.close();if(!err&&stateValid()) {
      if(String(suffix)=="/state.bak") {
        LittleFS.remove(storagePrefix+"/state.json");
        LittleFS.rename(storagePrefix+"/state.bak",storagePrefix+"/state.json");
      }
      return true;
    }
  }
  defaults();return false;
}
bool saveState() {
  if(!fsReady||!stateValid())return false;
  String base=storagePrefix+"/state";
  File f=LittleFS.open(base+".tmp","w");if(!f)return false;
  size_t expected=measureJson(state),written=serializeJson(state,f);f.flush();f.close();
  if(written!=expected||expected>55000){LittleFS.remove(base+".tmp");return false;}
  if(LittleFS.exists(base+".json")) {
    LittleFS.remove(base+".bak");
    if(!LittleFS.rename(base+".json",base+".bak"))return false;
  }
  if(!LittleFS.rename(base+".tmp",base+".json")) {
    if(LittleFS.exists(base+".bak"))LittleFS.rename(base+".bak",base+".json");
    return false;
  }
  return true;
}
String almanacJson() {
  DynamicJsonDocument error(256);
  if(!clockValid()){error["available"]=false;error["message"]="尚未校时，联网后自动恢复";return jsonText(error);}
  struct tm start={};start.tm_year=2026-1900;start.tm_mon=0;start.tm_mday=1;start.tm_isdst=0;
  time_t first=mktime(&start),now=time(nullptr);
  int day=(now-first)/86400;
  File idx=LittleFS.open("/calendar.idx","r"),dat=LittleFS.open("/calendar.dat","r");
  uint32_t offsets[2]={0,0};
  if(!idx||!dat||day<0||size_t(day+2)*4>idx.size()||!idx.seek(day*4)||idx.read((uint8_t*)offsets,8)!=8||offsets[1]<=offsets[0]||offsets[1]>dat.size()||offsets[1]-offsets[0]>=2048) {
    error["available"]=false;error["message"]="黄历数据需更新（支持2026—2035年）";return jsonText(error);
  }
  dat.seek(offsets[0]);String raw;raw.reserve(offsets[1]-offsets[0]);
  for(uint32_t i=offsets[0];i<offsets[1];i++)raw+=(char)dat.read();
  DynamicJsonDocument value(3072);if(deserializeJson(value,raw)||value["date"].as<String>()!=localDate()) {
    error["available"]=false;error["message"]="黄历数据校验失败";return jsonText(error);
  }
  value["available"]=true;return jsonText(value);
}
static Reply jsonReply(JsonDocument& doc,int code=200){return {code,"application/json; charset=utf-8",jsonText(doc)};}
Reply dispatch(const String& path,const String& method,const String& body,bool authorized) {
  if(path=="/health"&&method=="GET") {
    DynamicJsonDocument d(1024);d["device"]="ESP32-C3";d["access_key_required"]=false;d["firmware"]="KT2-C3-0.1.7";
    d["fs_ready"]=fsReady;d["clock_valid"]=clockValid();d["wifi_connected"]=WiFi.status()==WL_CONNECTED;
    d["ip"]=WiFi.localIP().toString();d["setup_ap"]=apActive;d["free_heap"]=ESP.getFreeHeap();
    d["wifi_configured"]=preferences.isKey("wifi_cfg")||preferences.getString("ssid","").length()>0;
    d["wifi_state"]=int(WiFi.status());d["last_wifi_reason"]=int(wifiDisconnectReason);
    d["wifi_power_save"]=int(WiFi.getSleep());d["wifi_rssi"]=WiFi.RSSI();
    d["wifi_tx_power_qdbm"]=int(WiFi.getTxPower());
    d["gateway"]=WiFi.gatewayIP().toString();d["subnet"]=WiFi.subnetMask().toString();
    d["uptime_seconds"]=millis()/1000;return jsonReply(d);
  }
  // Keyless access is intentional; retain the argument for existing USB callers.
  (void)authorized;
  if(path=="/api/version"&&method=="GET") {
    DynamicJsonDocument d(512);d["version"]=String(state["revision"].as<uint32_t>())+":"+String((uint32_t)(time(nullptr)/300))+":"+String(weatherRequested);return jsonReply(d);
  }
  if(path=="/api/state"&&method=="GET") {
    // Serialize the persistent document directly, without a second full JSON
    // tree. Keep even the 50 x 120 four-byte-character case within C3 RAM.
    String almanac=almanacJson(),weather=weatherJson(),out;
    size_t needed=measureJson(state["settings"])+measureJson(state["tasks"])+almanac.length()+weather.length()+512;
    if(!out.reserve(needed))return errorReply(503,"内存不足，请稍后重试");
    out="{\"schema\":1,\"revision\":"+String(state["revision"].as<uint32_t>())+",\"settings\":";
    serializeJson(state["settings"],out);out+=",\"tasks\":";serializeJson(state["tasks"],out);
    bool valid=clockValid();
    out+=",\"date\":\""+(valid?localDate():String(""))+"\",\"time\":\""+(valid?localStamp():String(""))+"\",\"clock_valid\":"+(valid?"true":"false");
    out+=",\"source\":\"ESP32-C3\",\"almanac\":";out+=almanac;out+=",\"weather\":";out+=weather;out+='}';
    return {200,"application/json; charset=utf-8",out};
  }
  if(body.length()>2048)return errorReply(413,"请求内容过长");
  DynamicJsonDocument input(4096);
  if((method=="POST"||method=="PUT"||method=="PATCH")&&(deserializeJson(input,body)||!input.is<JsonObject>()))return errorReply(400,"请求格式不正确");
  if(path=="/api/wifi"&&method=="POST") {
    if(!input["ssid"].is<const char*>()||!input["password"].is<const char*>())return errorReply(400,"请填写Wi-Fi名称和密码");
    String ssid=input["ssid"],password=input["password"];
    // Never let a second click after the password field was cleared replace a
    // protected home network's credentials with an empty password.
    if(ssid.isEmpty()||ssid.length()>32||password.length()<8||password.length()>63)return errorReply(400,"请填写Wi-Fi名称及8至63字节密码，空白不会覆盖已保存配置");
    DynamicJsonDocument network(512);network["ssid"]=ssid;network["password"]=password;
    String config=jsonText(network);
    if(preferences.putString("wifi_cfg",config)!=config.length())return errorReply(507,"Wi-Fi配置保存失败");
    restartAt=millis()+1500;DynamicJsonDocument d(256);d["saved"]=true;d["message"]="已保存，设备将重启连接家庭Wi-Fi";return jsonReply(d);
  }
  if(!fsReady)return errorReply(503,"存储尚不可用，未保存更改");
  if(path=="/api/weather/refresh"&&method=="POST") {
    weatherRequested=true;DynamicJsonDocument d(128);d["queued"]=true;return jsonReply(d,202);
  }
  if(path=="/api/settings"&&method=="PUT") {
    if(!input["mode"].is<const char*>())return errorReply(400,"请选择显示页面");
    String mode=input["mode"];if(mode!="today"&&mode!="almanac"&&mode!="weather")return errorReply(400,"未知显示页面");
    state["settings"]["mode"]=mode;
  } else if(path=="/api/tasks"&&method=="POST") {
    if(state["tasks"].size()>=MAX_TASKS)return errorReply(409,"最多保存50条待办，请先整理已完成事项");
    if(!input["text"].is<const char*>())return errorReply(400,"待办内容格式不正确");
    String text=input["text"];text.trim();int n=unicodeLength(text);if(n<1||n>MAX_TEXT_CHARS)return errorReply(400,"待办须为1至120个字符，不能包含换行或控制字符");
    uint32_t id=state["next_id"];if(id==UINT32_MAX)return errorReply(409,"待办编号已用尽，请导出数据");
    JsonObject task=state["tasks"].as<JsonArray>().createNestedObject();task["id"]=id;task["text"]=text;task["done"]=false;state["next_id"]=id+1;
  } else if(path.startsWith("/api/tasks/")&&(method=="PATCH"||method=="DELETE")) {
    String suffix=path.substring(11);if(suffix.isEmpty())return errorReply(404,"未找到待办");
    for(size_t i=0;i<suffix.length();i++)if(!isDigit(suffix[i]))return errorReply(404,"未找到待办");
    uint64_t large=strtoull(suffix.c_str(),nullptr,10);if(large==0||large>UINT32_MAX)return errorReply(404,"未找到待办");
    JsonArray tasks=state["tasks"];int found=-1;for(size_t i=0;i<tasks.size();i++)if(tasks[i]["id"]==uint32_t(large)){found=i;break;}
    if(found<0)return errorReply(404,"未找到待办");
    if(method=="DELETE")tasks.remove(found);
    else {
      if(!input.containsKey("text")&&!input.containsKey("done"))return errorReply(400,"没有可更新的内容");
      String text;
      if(input.containsKey("text")) {
        if(!input["text"].is<const char*>())return errorReply(400,"待办内容格式不正确");
        text=input["text"].as<String>();text.trim();int n=unicodeLength(text);if(n<1||n>MAX_TEXT_CHARS)return errorReply(400,"待办须为1至120个字符");
      }
      if(input.containsKey("done")&&!input["done"].is<bool>())return errorReply(400,"完成状态格式不正确");
      if(input.containsKey("text"))tasks[found]["text"]=text;
      if(input.containsKey("done"))tasks[found]["done"]=input["done"].as<bool>();
    }
  } else return errorReply(404,"接口不存在");
  state["revision"]=state["revision"].as<uint32_t>()+1;
  if(!saveState()){loadState();return errorReply(507,"保存失败，请重试，未确认的更改已撤回");}
  // Reparse the committed file to reclaim ArduinoJson's monotonic string pool.
  loadState();
  DynamicJsonDocument d(256);d["saved"]=true;d["revision"]=state["revision"];return jsonReply(d);
}

void selfTest() {
  String original=jsonText(state);storagePrefix="/selftest";LittleFS.mkdir(storagePrefix);defaults();
  int passed=0,failed=0;String failures;
  auto check=[&](bool ok,const char* name){if(ok)passed++;else{failed++;failures+=String(name)+";";}};
  check(unicodeLength("苏州🙂")==3,"unicode");check(unicodeLength(String("\xc0\xaf"))==-1,"bad_utf8");
  check(escapeHtml("<img>&\"'")=="&lt;img&gt;&amp;&quot;&#39;","html_escape");
  check(dispatch("/api/state","GET","",false).code==200,"keyless_access");
  check(dispatch("/api/tasks","POST","{bad",true).code==400,"invalid_json");
  check(dispatch("/api/tasks","POST","{\"text\":\"   \"}",true).code==400,"empty");
  check(dispatch("/api/tasks","POST","{\"text\":\"测试苏州🙂\"}",true).code==200,"create");
  check(dispatch("/api/tasks/1","PATCH","{\"done\":true}",true).code==200,"complete");
  loadState();check(state["tasks"][0]["done"]==true&&state["tasks"][0]["text"]=="测试苏州🙂","reload");
  check(dispatch("/api/tasks/1","PATCH","{\"text\":\"changed\",\"done\":\"true\"}",true).code==400&&state["tasks"][0]["text"]=="测试苏州🙂","atomic_validation");
  check(dispatch("/api/settings","PUT","{\"mode\":\"bad\"}",true).code==400,"bad_mode");
  check(dispatch("/api/settings","PUT","{\"mode\":\"almanac\"}",true).code==200,"mode");
  check(dispatch("/api/tasks/1","DELETE","",true).code==200&&state["tasks"].size()==0,"delete");
  String longText;for(int i=0;i<121;i++)longText+='a';
  check(dispatch("/api/tasks","POST",String("{\"text\":\"")+longText+"\"}",true).code==400,"length_limit");
  defaults();String lastText;
  for(int i=0;i<MAX_TASKS;i++) {
    String text;
    for(int j=0;j<MAX_TEXT_CHARS;j++) {
      uint32_t cp=0x1f300+i*MAX_TEXT_CHARS+j;
      text+=char(0xf0|(cp>>18));text+=char(0x80|((cp>>12)&63));text+=char(0x80|((cp>>6)&63));text+=char(0x80|(cp&63));
    }
    JsonObject task=state["tasks"].as<JsonArray>().createNestedObject();
    task["id"]=i+1;task["text"]=text;task["done"]=false;lastText=text;
  }
  state["next_id"]=MAX_TASKS+1;
  check(stateValid()&&saveState(),"capacity_max_four_byte_save");
  {
    Reply maximum=dispatch("/api/state","GET","",true);
    check(maximum.code==200&&maximum.body.length()>MAX_TASKS*MAX_TEXT_CHARS*4&&maximum.body.endsWith("}"),"capacity_state_response");
    check(maximum.body.indexOf(lastText)>=0&&maximum.body.indexOf("\"status\":\"")>=0,"capacity_last_task_and_weather");
  }
  LittleFS.remove("/selftest/state.json");LittleFS.remove("/selftest/state.bak");LittleFS.remove("/selftest/state.tmp");LittleFS.rmdir("/selftest");
  storagePrefix="";deserializeJson(state,original);
  DynamicJsonDocument report(512);report["passed"]=passed;report["failed"]=failed;report["failures"]=failures;report["free_heap"]=ESP.getFreeHeap();
  Serial.println("SELFTEST "+jsonText(report));
}
