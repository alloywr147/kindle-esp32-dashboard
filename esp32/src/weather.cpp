#include "app.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>

static String cache;
static bool loaded=false,attempted=false;
static unsigned long lastAttempt=0;
static unsigned long lastTick=0;
static const char* condition(int code) {
  switch(code) {
    case 0:return "晴";case 1:return "晴间多云";case 2:return "多云";case 3:return "阴";
    case 45:return "雾";case 48:return "雾凇";case 51:case 53:case 55:return "毛毛雨";
    case 56:case 57:case 66:case 67:return "冻雨";case 61:return "小雨";case 63:return "中雨";case 65:return "大雨";
    case 71:return "小雪";case 73:return "中雪";case 75:return "大雪";case 77:return "雪粒";
    case 80:return "阵雨";case 81:return "强阵雨";case 82:return "暴雨";case 85:case 86:return "阵雪";
    case 95:return "雷雨";case 96:case 99:return "雷雨冰雹";default:return "未知天气";
  }
}
static bool numeric(JsonVariantConst v) {return !v.is<bool>()&&(v.is<float>()||v.is<int>())&&isfinite(v.as<float>());}
static bool validCache(JsonDocument& d) {
  if(!d["fetched_epoch"].is<uint32_t>()||!d["days"].is<JsonArray>()||d["days"].size()!=7||!numeric(d["current"]["temperature"]))return false;
  for(JsonObject day:d["days"].as<JsonArray>()) {
    if(!day["date"].is<const char*>()||!day["condition"].is<const char*>()||!numeric(day["low"])||!numeric(day["high"])||!numeric(day["rain"]))return false;
    if(day["low"].as<float>()>day["high"].as<float>()||day["rain"].as<int>()<0||day["rain"].as<int>()>100)return false;
  }
  return true;
}
static void loadCache() {
  if(loaded)return;loaded=true;
  File file=LittleFS.open("/weather.json","r");if(!file||file.size()>12288)return;
  DynamicJsonDocument d(12288);if(!deserializeJson(d,file)&&validCache(d))cache=jsonText(d);
}
String weatherJson() {
  loadCache();DynamicJsonDocument d(12288);
  if(!cache.isEmpty())deserializeJson(d,cache);
  time_t now=time(nullptr);uint32_t fetched=d["fetched_epoch"]|0;
  if(!clockValid()||!validCache(d)||now<fetched||now-fetched>43200) {
    d.clear();d["status"]="unavailable";d["current"]=nullptr;d.createNestedArray("days");
  } else {
    bool sameDay=d["days"][0]["date"].as<String>()==localDate();
    d["status"]=(sameDay&&now-fetched<3600)?"live":"stale";
    if(!sameDay)d["current"]=nullptr;
    JsonArray days=d["days"];for(int i=int(days.size())-1;i>=0;i--)if(days[i]["date"].as<String>()<localDate())days.remove(i);
    d["fetched_at"]=localStamp(fetched);
  }
  d["city"]="苏州";d["source"]="Open-Meteo";return jsonText(d);
}
static bool fetchWeather() {
  File cert=LittleFS.open("/weather-root.pem","r");if(!cert||cert.size()>8192)return false;
  String root=cert.readString();cert.close();WiFiClientSecure tls;tls.setCACert(root.c_str());tls.setHandshakeTimeout(8);
  HTTPClient http;http.setConnectTimeout(6000);http.setTimeout(6000);http.useHTTP10(true);
  const char* url="https://api.open-meteo.com/v1/forecast?latitude=31.30&longitude=120.58&current=temperature_2m,weather_code&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=Asia%2FShanghai&forecast_days=7";
  if(!http.begin(tls,url))return false;
  int code=http.GET();if(code!=200||http.getSize()>24576){http.end();return false;}
  String raw=http.getString();http.end();if(raw.length()>24576)return false;
  DynamicJsonDocument input(16384),out(12288);if(deserializeJson(input,raw))return false;
  auto current=input["current"];
  if(!numeric(current["temperature_2m"])||!numeric(current["weather_code"]))return false;
  out["current"]["temperature"]=int(roundf(current["temperature_2m"].as<float>()));
  out["current"]["condition"]=condition(current["weather_code"].as<int>());
  out["current"]["time"]=current["time"];
  auto daily=input["daily"];if(daily["time"].size()<7)return false;JsonArray days=out.createNestedArray("days");
  for(int i=0;i<7;i++) {
    String expected=localDate(time(nullptr)+i*86400);
    if(daily["time"][i].as<String>()!=expected||!numeric(daily["temperature_2m_min"][i])||!numeric(daily["temperature_2m_max"][i])||!numeric(daily["precipitation_probability_max"][i])||!numeric(daily["weather_code"][i]))return false;
    JsonObject day=days.createNestedObject();day["date"]=expected;
    day["condition"]=condition(daily["weather_code"][i].as<int>());
    day["low"]=int(roundf(daily["temperature_2m_min"][i].as<float>()));
    day["high"]=int(roundf(daily["temperature_2m_max"][i].as<float>()));
    day["rain"]=int(roundf(daily["precipitation_probability_max"][i].as<float>()));
  }
  out["fetched_epoch"]=uint32_t(time(nullptr));if(out.overflowed()||!validCache(out))return false;
  File f=LittleFS.open("/weather.tmp","w");if(!f)return false;
  size_t expected=measureJson(out),written=serializeJson(out,f);f.flush();f.close();if(expected!=written)return false;
  // Cached weather can be downloaded again; the user todo state uses a separate two-generation store.
  if(!LittleFS.rename("/weather.tmp","/weather.json")){LittleFS.remove("/weather.json");if(!LittleFS.rename("/weather.tmp","/weather.json"))return false;}
  cache=jsonText(out);return true;
}
void weatherTick() {
  if(millis()-lastTick<1000UL)return;lastTick=millis();
  if(WiFi.status()!=WL_CONNECTED||!clockValid()||!fsReady)return;
  loadCache();DynamicJsonDocument d(12288);if(!cache.isEmpty())deserializeJson(d,cache);
  uint32_t fetched=d["fetched_epoch"]|0;time_t now=time(nullptr);
  bool fresh=validCache(d)&&now>=fetched&&now-fetched<3600&&d["days"][0]["date"].as<String>()==localDate();
  if(!weatherRequested&&fresh)return;
  if(attempted&&millis()-lastAttempt<300000UL)return;
  attempted=true;lastAttempt=millis();weatherRequested=false;
  Serial.println(fetchWeather()?"WEATHER_OK":"WEATHER_FAILED_KEEP_CACHE");
}
