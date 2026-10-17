#include "app.h"
#include <ESPmDNS.h>
#include <esp_system.h>
#include <sys/time.h>
#include "generated_ui.h"

WebServer server(80);
static String serialInput,hostname;
static bool loggedConnection=false;
static bool stationConfigured=false;
static unsigned long startedAt,lastStationRetry=0;

static String randomHex(int bytes) {String out;for(int i=0;i<bytes;i++){char part[3];snprintf(part,3,"%02x",uint8_t(esp_random()));out+=part;}return out;}
static String homePage(bool bootstrap) {
  String html=FPSTR(INDEX_HTML);html.replace("__DEVICE_NAME__",hostname);return html;
}
static Reply serve(const String& path,const String& method,const String& body,bool authorized,bool bootstrap=false) {
  if(path=="/"&&method=="GET")return {200,"text/html; charset=utf-8",homePage(bootstrap)};
  if(path=="/favicon.ico"&&method=="GET")return {204,"image/x-icon",""};
  if(path=="/view"&&method=="GET") {
    if(!fsReady)return errorReply(503,"看板数据存储尚不可用");
    return {200,"text/html; charset=utf-8",renderView()};
  }
  return dispatch(path,method,body,authorized);
}
static void httpRequest() {
  String method=server.method()==HTTP_GET?"GET":server.method()==HTTP_POST?"POST":server.method()==HTTP_PUT?"PUT":server.method()==HTTP_PATCH?"PATCH":server.method()==HTTP_DELETE?"DELETE":"OTHER";
  if(method!="GET"&&server.hasHeader("Origin")&&server.header("Origin")!="http://"+server.hostHeader()) {
    server.send(403,"application/json; charset=utf-8","{\"detail\":\"请求来源不匹配\"}");return;
  }
  if(method!="GET"&&method!="DELETE"&&!server.header("Content-Type").startsWith("application/json")) {
    server.send(415,"application/json; charset=utf-8","{\"detail\":\"请使用JSON请求\"}");return;
  }
  bool authorized=true; // Owner-selected LAN access without a key.
  bool bootstrap=apActive&&server.client().localIP()==WiFi.softAPIP();
  Reply reply=serve(server.uri(),method,server.arg("plain"),authorized,bootstrap);
  server.sendHeader("Cache-Control","no-store");server.sendHeader("Referrer-Policy","no-referrer");server.sendHeader("X-Content-Type-Options","nosniff");
  server.send(reply.code,reply.type,reply.body);
}
static void startAp() {
  if(apActive)return;
  WiFi.mode(WIFI_AP_STA);
  apActive=WiFi.softAP(setupSsid.c_str(),setupPassword.c_str());
  Serial.println(apActive?"SETUP_AP_READY":"SETUP_AP_FAILED");
}
static void serialCommand(const String& command) {
  if(command=="STATUS")Serial.println("STATUS "+dispatch("/health","GET","",false).body);
  else if(command=="NET_AWAKE") {
    Serial.println(WiFi.setSleep(false)?"NET_AWAKE_OK":"NET_AWAKE_FAILED");
  }
  else if(command=="CONFIG") {
    DynamicJsonDocument d(1024);d["setup_ssid"]=setupSsid;d["setup_password"]=setupPassword;
    d["setup_url"]="http://192.168.4.1/";d["hostname"]=hostname+".local";d["ip"]=WiFi.localIP().toString();
    d["ssid_configured"]=preferences.getString("ssid","").length()>0;
    Serial.println("CONFIG "+jsonText(d));
  } else if(command=="SELFTEST") {if(fsReady)selfTest();else Serial.println("SELFTEST {\"passed\":0,\"failed\":1,\"failures\":\"filesystem\"}");}
  else if(command.startsWith("TIME ")) {
    uint64_t epoch=strtoull(command.substring(5).c_str(),nullptr,10);
    if(epoch>=1767196800ULL&&epoch<2082729600ULL){struct timeval tv={time_t(epoch),0};settimeofday(&tv,nullptr);Serial.println("TIME_OK");}
    else Serial.println("TIME_INVALID");
  } else if(command.startsWith("RPC ")) {
    DynamicJsonDocument request(6144);if(deserializeJson(request,command.substring(4))){Serial.println("RPC {\"code\":400,\"body\":\"bad rpc\"}");return;}
    Reply result=serve(request["path"]|"",request["method"]|"GET",request["body"]|"",true,false);
    DynamicJsonDocument output(result.body.length()+2048);output["code"]=result.code;output["type"]=result.type;output["body"]=result.body;
    if(output.overflowed()){Serial.println("RPC {\"code\":503,\"body\":\"memory\"}");return;}
    Serial.print("RPC ");serializeJson(output,Serial);Serial.println();
  }
}
void setup() {
  Serial.setRxBufferSize(8192);Serial.setTxBufferSize(4096);Serial.begin(115200);delay(900);startedAt=millis();
  setenv("TZ","CST-8",1);tzset();
  fsReady=LittleFS.begin(false,"/littlefs",10,"littlefs");
  if(fsReady)loadState();
  else Serial.println("FILESYSTEM_MISSING_NO_AUTOFORMAT");
  preferences.begin("kt2web",false);
  // No access key: this build is intentionally a trusted-LAN service.
  setupPassword=preferences.getString("ap_pass","");if(setupPassword.length()!=16){setupPassword=randomHex(8);preferences.putString("ap_pass",setupPassword);}
  String suffix=String(uint32_t(ESP.getEfuseMac()>>32)&0xffff,HEX);while(suffix.length()<4)suffix="0"+suffix;
  hostname="kt2-"+suffix;setupSsid="KT2-Setup-"+suffix;
  WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setHostname(hostname.c_str());WiFi.setAutoReconnect(true);
  // This is a continuously USB-powered LAN server. On this C3/router pair,
  // modem sleep lost inbound ARP/TCP while STATUS still reported connected.
  WiFi.setSleep(false);
  // Some C3 Mini boards cannot transmit reliably at the SDK's default peak.
  // Use the conservative level validated for this continuously powered board.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.onEvent([](WiFiEvent_t event,WiFiEventInfo_t info){
    if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED)wifiDisconnectReason=info.wifi_sta_disconnected.reason;
    else if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP)wifiDisconnectReason=0;
  });
  String ssid,password;
  if(preferences.isKey("wifi_cfg")) {
    DynamicJsonDocument network(512);
    if(!deserializeJson(network,preferences.getString("wifi_cfg",""))) {
      ssid=network["ssid"].as<String>();password=network["password"].as<String>();
    }
  } else {
    // Read legacy settings once; new saves use one atomic NVS value.
    ssid=preferences.getString("ssid","");password=preferences.getString("wifi_pass","");
  }
  stationConfigured=ssid.length()>0;lastStationRetry=millis();
  if(stationConfigured)WiFi.begin(ssid.c_str(),password.c_str());else startAp();
  const char* headers[]={"Authorization","Origin","Content-Type"};server.collectHeaders(headers,3);
  server.onNotFound(httpRequest);server.begin();
  Serial.println("KT2_C3_READY");Serial.println("STATUS "+dispatch("/health","GET","",false).body);
}
void loop() {
  while(Serial.available()) {
    char c=Serial.read();if(c=='\n'){serialCommand(serialInput);serialInput="";}else if(c!='\r') {
      if(serialInput.length()<4096)serialInput+=c;else serialInput="";
    }
  }
  server.handleClient();
  if(restartAt&&int32_t(millis()-restartAt)>=0)ESP.restart();
  if(WiFi.status()==WL_CONNECTED) {
    lastStationRetry=millis();
    if(!loggedConnection) {
      loggedConnection=true;configTzTime("CST-8","ntp.aliyun.com","pool.ntp.org","time.cloudflare.com");MDNS.begin(hostname.c_str());
      Serial.println("WIFI_CONNECTED "+WiFi.localIP().toString());
      if(apActive){WiFi.softAPdisconnect(true);apActive=false;WiFi.mode(WIFI_STA);}
    }
  } else {
    loggedConnection=false;
    if(millis()-startedAt>30000UL)startAp();
    // Arduino's auto reconnect deliberately excludes AUTH_FAIL. A transient
    // authentication error must not permanently strand this unattended server.
    if(stationConfigured&&millis()-lastStationRetry>=60000UL) {
      lastStationRetry=millis();WiFi.reconnect();Serial.println("WIFI_RETRY");
    }
  }
  weatherTick();delay(2);
}
