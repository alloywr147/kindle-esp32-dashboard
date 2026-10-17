#include "app.h"

static String shortText(const String& value,int maxChars) {
  int count=0;size_t i=0;
  for(;i<value.length();i++){if((uint8_t(value[i])&0xc0)!=0x80){if(count==maxChars)break;count++;}}
  return value.substring(0,i)+(i<value.length()?"…":"");
}
static String joined(JsonVariantConst items) {
  String result;for(JsonVariantConst item:items.as<JsonArrayConst>()){if(result.length())result+=" · ";result+=item.as<String>();}return result;
}
String renderView() {
  DynamicJsonDocument a(4096),w(12288);deserializeJson(a,almanacJson());deserializeJson(w,weatherJson());
  String title=clockValid()?localDate():"等待联网校时";
  String page=F("<!doctype html><html lang='zh-CN'><head><meta charset='UTF-8'><meta name='viewport' content='width=600'><meta http-equiv='refresh' content='3600'><title>KT2 看板</title><style>html,body{margin:0;padding:0;background:white;color:black;font-family:sans-serif}body{width:560px;padding:18px 20px;font-size:24px;line-height:1.4}h1{font-size:28px;margin:0 0 8px}h2{font-size:28px;margin:12px 0}p{margin:8px 0}.small{font-size:20px}.rule{border-top:2px solid black;margin:14px 0}.task{margin:12px 0;font-size:26px;line-height:1.35;word-wrap:break-word}table{width:100%;border-collapse:collapse;font-size:23px}td,th{text-align:left;padding:10px 2px;border-bottom:1px solid #aaa}footer{font-size:16px;margin-top:18px}a{color:black}.day{font-size:74px}</style></head><body>");
  page+="<h1>"+escapeHtml(title)+" <span class='small'>"+escapeHtml(a["weekday"]|"")+"</span></h1>";
  page+="<p class='small'>"+escapeHtml(a["lunar_date"]|"")+"</p><div class='rule'></div>";
  String mode=state["settings"]["mode"]|"today";
  if(mode=="today") {
    page+="<h2>苏州</h2>";
    if(!w["current"].isNull())page+="<p style='font-size:36px'>"+String(w["current"]["temperature"].as<int>())+"℃ "+escapeHtml(w["current"]["condition"].as<String>())+"</p>";
    else page+="<p>天气暂不可用</p>";
    if(w["days"].size()>0&&w["days"][0]["date"].as<String>()==localDate())page+="<p class='small'>今日 "+String(w["days"][0]["low"].as<int>())+"～"+String(w["days"][0]["high"].as<int>())+"℃　降水 "+String(w["days"][0]["rain"].as<int>())+"%</p>";
    page+="<div class='rule'></div><h2>今日待办</h2>";
    int total=0,shown=0;
    for(JsonObject task:state["tasks"].as<JsonArray>())if(!task["done"].as<bool>()) {
      total++;if(shown<4){page+="<div class='task'>□ "+escapeHtml(shortText(task["text"].as<String>(),32))+"</div>";shown++;}
    }
    if(total==0)page+="<p>"+String(state["tasks"].size()?"今天的待办已全部完成":"还没有待办事项")+"</p><p class='small'>在手机网页中添加即可显示</p>";
    if(total>shown)page+="<p class='small'>另有 "+String(total-shown)+" 条，请在手机查看</p>";
  } else if(mode=="almanac") {
    page+="<h2>今日黄历</h2>";
    if(a["available"].as<bool>()) {
      page+="<p>"+escapeHtml(a["ganzhi"].as<String>())+"</p><p>"+escapeHtml(a["seasonal_note"].as<String>())+"</p><div class='rule'></div>";
      page+="<p><b>宜</b>　"+escapeHtml(joined(a["yi"]))+"</p><p><b>忌</b>　"+escapeHtml(joined(a["ji"]))+"</p><div class='rule'></div><p>"+escapeHtml(a["chong_sha"].as<String>())+"</p><p class='small'>传统民俗，仅供参考</p>";
    } else page+="<p>"+escapeHtml(a["message"].as<String>())+"</p>";
  } else {
    page+="<h2>苏州 · 七日预报</h2><table><tr><th>日期</th><th>天气</th><th>低/高温</th><th>降水</th></tr>";
    for(int i=0;i<7;i++) {
      String wanted=clockValid()?localDate(time(nullptr)+86400*i):"";JsonObject day;
      for(JsonObject candidate:w["days"].as<JsonArray>())if(candidate["date"].as<String>()==wanted){day=candidate;break;}
      page+="<tr><td>"+String(i==0?"今天":i==1?"明天":wanted.substring(5))+"</td><td>"+escapeHtml(day.isNull()?String("暂无数据"):day["condition"].as<String>())+"</td><td>";
      page+=day.isNull()?String("--"):String(day["low"].as<int>())+"/"+String(day["high"].as<int>())+"℃";
      page+="</td><td>"+String(day.isNull()?"--":String(day["rain"].as<int>())+"%")+"</td></tr>";
    }
    page+="</table>";
  }
  page+="<footer><div class='rule'></div>画面 "+escapeHtml(clockValid()?localStamp():String("尚未校时"))+"<br>";
  String status=w["status"]|"unavailable";page+=status=="live"?"天气已更新":status=="stale"?"天气延迟":"天气未连接";
  page+=" "+escapeHtml(w["fetched_at"]|"")+"<br>天气：Open-Meteo · ESP32-C3 独立看板</footer>";
  page+="</body></html>";
  return page;
}
