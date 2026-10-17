#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

struct Reply { int code; String type; String body; };
extern DynamicJsonDocument state;
extern Preferences preferences;
extern String setupPassword, setupSsid;
extern bool fsReady, apActive, weatherRequested;
extern unsigned long restartAt;
extern volatile int wifiDisconnectReason;
extern WebServer server;
constexpr int MAX_TASKS = 50;
constexpr int MAX_TEXT_CHARS = 120;
bool clockValid();
String localDate(time_t at = 0);
String localStamp(time_t at = 0);
int unicodeLength(const String& text);
String escapeHtml(const String& value);
String jsonText(JsonDocument& doc);
Reply errorReply(int code, const char* message);
bool loadState();
bool saveState();
String almanacJson();
String weatherJson();
void weatherTick();
Reply dispatch(const String& path, const String& method, const String& body, bool authorized);
String renderView();
void selfTest();
