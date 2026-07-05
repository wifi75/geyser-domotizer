#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

void eventLogBegin(AsyncWebServer& server);
void eventLogAdd(const char* type, const String& message);
void eventLogToJson(JsonArray out);

