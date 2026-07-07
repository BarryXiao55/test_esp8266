#include "WebServer.h"
#include "DashboardHtml.h"
#include "RingBuffer.h"
#include "WeatherCodes.h"
#include "config.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);

void webserver_init() {
    // 主页: Dashboard HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* resp = request->beginResponse_P(
            200, "text/html", (const uint8_t*)DASHBOARD_HTML, DASHBOARD_HTML_SIZE);
        request->send(resp);
    });

    // API: 天气数据 JSON
    server.on("/api/weather", HTTP_GET, [](AsyncWebServerRequest* request) {
        // 分配约 3KB 的动态 JSON（堆临时使用，请求完成后释放）
        const size_t capacity = JSON_OBJECT_SIZE(3)
            + JSON_OBJECT_SIZE(7)
            + JSON_ARRAY_SIZE(MAX_RECORDS)
            + MAX_RECORDS * JSON_OBJECT_SIZE(4)
            + 400;
        DynamicJsonDocument doc(capacity);

        JsonObject root = doc.to<JsonObject>();

        // 当前数据
        WeatherRecord latest;
        bool hasData = ring_latest(&latest);
        if (hasData) {
            JsonObject current = root["current"].to<JsonObject>();
            current["temp"]        = latest.temp;
            current["feels_like"]  = latest.feels_like;
            current["humidity"]    = latest.humidity;
            current["wind_speed"]  = latest.wind_speed;
            current["wind_dir"]    = latest.wind_dir;
            current["wind_label"]  = getWindDirLabel(latest.wind_dir);
            current["weather_code"] = latest.weather_code;
            current["weather_text"] = getWeatherLabel(latest.weather_code);
            current["timestamp"]   = (long)latest.timestamp;
        }

        // 历史数据
        JsonArray hist = root["history"].to<JsonArray>();
        WeatherRecord records[MAX_RECORDS];
        int n = ring_get_all(records, MAX_RECORDS);
        for (int i = 0; i < n; i++) {
            JsonObject item = hist.createNestedObject();
            item["time"]    = (long)records[i].timestamp;
            item["temp"]    = records[i].temp;
            item["humidity"] = records[i].humidity;
        }

        // 系统状态
        JsonObject sys = root["system"].to<JsonObject>();
        sys["uptime"]    = (long)(millis() / 1000);
        sys["free_heap"] = ESP.getFreeHeap();
        sys["rssi"]      = WiFi.RSSI();
        sys["records"]   = ring_count();
        sys["location"]  = STRINGIFY(WEATHER_LOCATION_NAME);

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // 重置配网
    server.on("/api/reset", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "OK - resetting WiFi config...");
        delay(100);
        WiFi.disconnect(true);
        ESP.restart();
    });

    // 默认 404
    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "Not Found");
    });

    server.begin();
}
