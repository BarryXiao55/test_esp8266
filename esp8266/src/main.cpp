// ESP8266 Weather Sensor (方案 A)
// 从 Open-Meteo 获取天气数据，POST 到 Node.js 后端
// 与方案 C 不同：不做 Web 服务器，不上传嵌入式 Dashboard

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>
#include <BearSSLHelpers.h>
#include <ArduinoJson.h>
#include "config.h"

// ---- 全局状态 ----
static String g_token = "";
static unsigned long lastWeatherFetch = 0;
static unsigned long lastNtpSync = 0;
static BearSSL::WiFiClientSecure sslClient;

// ---- 函数声明 ----
void syncNtp();
void doWeatherFetch();
bool fetchFromOpenMeteo(WeatherRecord* out);
String registerDevice();
bool uploadToBackend(const WeatherRecord& rec);

// ==========================================
//  WMO Weather Code → 中文标签
// ==========================================
static const char WEATHER_LABELS[][32] PROGMEM = {
    // index 0-9
    "\xe2\x98\x80\xef\xb8\x8f \xe6\x99\xb4",
    "\xf0\x9f\x8c\xa4\xef\xb8\x8f \xe5\xa4\xa7\xe9\x83\xa8\xe6\x99\xb4",
    "\xe2\x9b\x85 \xe5\xa4\x9a\xe4\xba\x91",
    "\xe2\x98\x81\xef\xb8\x8f \xe9\x98\xb4",
    "", "",
    // 45, 48: fog
    "\xf0\x9f\x8c\xab\xef\xb8\x8f \xe9\x9b\xbe",
    "", "\xf0\x9f\x8c\xab\xef\xb8\x8f \xe9\x9b\xbe\xe5\x87\x9b",
    "", "",
    // 51,53,55: drizzle
    "\xf0\x9f\x8c\xa7\xef\xb8\x8f \xe5\xb0\x8f\xe6\xaf\x9b\xe6\xaf\x9b\xe9\x9b\xa8",
    "", "\xf0\x9f\x8c\xa7\xef\xb8\x8f \xe4\xb8\xad\xe6\xaf\x9b\xe6\xaf\x9b\xe9\x9b\xa8",
    "", "\xf0\x9f\x8c\xa7\xef\xb8\x8f \xe5\xa4\xa7\xe6\xaf\x9b\xe6\xaf\x9b\xe9\x9b\xa8",
    "", "", "",
    // 61,63,65: rain
    "\xf0\x9f\x8c\xa7\xef\xb8\x8f \xe5\xb0\x8f\xe9\x9b\xa8",
    "", "\xf0\x9f\x8c\xa7\xef\xb8\x8f \xe4\xb8\xad\xe9\x9b\xa8",
    "", "\xf0\x9f\x8c\xa7\xef\xb8\x8f \xe5\xa4\xa7\xe9\x9b\xa8",
    "", "",
    // 71,73,75: snow
    "\xe2\x9d\x84\xef\xb8\x8f \xe5\xb0\x8f\xe9\x9b\xaa",
    "", "\xe2\x9d\x84\xef\xb8\x8f \xe4\xb8\xad\xe9\x9b\xaa",
    "", "\xe2\x9d\x84\xef\xb8\x8f \xe5\xa4\xa7\xe9\x9b\xaa",
    "", "",
    // 80,81,82: rain showers
    "\xf0\x9f\x8c\xa6\xef\xb8\x8f \xe5\xb0\x8f\xe9\x98\xb5\xe9\x9b\xa8",
    "", "\xf0\x9f\x8c\xa6\xef\xb8\x8f \xe4\xb8\xad\xe9\x98\xb5\xe9\x9b\xa8",
    "", "\xf0\x9f\x8c\xa6\xef\xb8\x8f \xe5\xa4\xa7\xe9\x98\xb5\xe9\x9b\xa8",
    "", "",
    // 95,96,99: thunderstorm
    "\xe2\x9b\x88\xef\xb8\x8f \xe9\x9b\xb7\xe6\x9a\xb4",
    "", "", "\xe2\x9b\x88\xef\xb8\x8f \xe5\x86\xb0\xe9\x9b\xb9\xe9\x9b\xb7\xe6\x9a\xb4",
    "", "\xe2\x9b\x88\xef\xb8\x8f \xe5\xa4\xa7\xe5\x86\xb0\xe9\x9b\xb9\xe9\x9b\xb7\xe6\x9a\xb4",
};

const char* getWeatherLabel(int8_t code) {
    static char buf[32];
    if (code >= 0 && code < (int)(sizeof(WEATHER_LABELS) / sizeof(WEATHER_LABELS[0]))
        && strlen_P(WEATHER_LABELS[code]) > 0) {
        strcpy_P(buf, WEATHER_LABELS[code]);
        return buf;
    }
    snprintf(buf, sizeof(buf), "WMO %d", code);
    return buf;
}

// ==========================================
//  setup()
// ==========================================
void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("===================================");
    Serial.println("  ESP8266 Weather Sensor v2.0");
    Serial.println("  (方案 A — 数据上报后端)");
    Serial.println("===================================");

    system_update_cpu_freq(160);

    // 1. WiFiManager 配网
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    wm.setConnectTimeout(15);

    if (!wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD)) {
        Serial.println("[ERR] WiFi 配网失败，重启...");
        ESP.restart();
    }
    Serial.print("[OK] WiFi 已连接: ");
    Serial.println(WiFi.SSID());
    Serial.print("    IP: ");
    Serial.println(WiFi.localIP());

    // 2. SSL 客户端初始化
    sslClient.setInsecure();
    sslClient.setBufferSizes(2048, 1024);

    // 3. 设备注册
    g_token = registerDevice();
    if (g_token.length() > 0) {
        Serial.print("[REG] 设备已注册, token: ");
        Serial.println(g_token.substring(0, 8) + "...");
    } else {
        Serial.println("[REG] 注册失败，将在首次上报前重试");
    }

    // 4. NTP 时间同步
    syncNtp();

    // 5. 首次天气采集 + 上报
    if (g_token.length() > 0) {
        doWeatherFetch();
    }

    Serial.println("\n[READY] 传感器运行中...");
    Serial.print("       后端地址: http://");
    Serial.print(BACKEND_HOST);
    Serial.print(":");
    Serial.println(STR(BACKEND_PORT));
}

// ==========================================
//  loop()
// ==========================================
void loop() {
    unsigned long now = millis();

    // 串口命令
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "reset") {
            Serial.println("[CMD] 清除 WiFi 配置并重启...");
            WiFi.disconnect(true);
            delay(100);
            ESP.restart();
        } else if (cmd == "ip") {
            Serial.print("[CMD] IP: ");
            Serial.println(WiFi.localIP());
        } else if (cmd == "token") {
            Serial.print("[CMD] Token: ");
            Serial.println(g_token);
        } else if (cmd == "help") {
            Serial.println("[CMD] 可用命令:");
            Serial.println("  reset - 清除 WiFi 配置，重启进入配网模式");
            Serial.println("  ip    - 显示当前 IP 地址");
            Serial.println("  token - 显示当前注册 token");
            Serial.println("  help  - 显示此帮助");
        }
    }

    // 每60分钟同步NTP
    if (now - lastNtpSync >= NTP_INTERVAL_MS) {
        syncNtp();
        lastNtpSync = now;
    }

    // 每10分钟获取+上报天气
    if (now - lastWeatherFetch >= WEATHER_INTERVAL_MS) {
        doWeatherFetch();
        lastWeatherFetch = now;
    }

    yield();
    delay(10);
}

// ==========================================
//  NTP 时间同步
// ==========================================
void syncNtp() {
    if (WiFi.status() != WL_CONNECTED) return;

    configTime(TZ_OFFSET, 0, "ntp.aliyun.com", "ntp.ntsc.ac.cn", "time.nist.gov");
    Serial.print("[NTP] 同步时间...");

    time_t now = time(nullptr);
    int retries = 20;
    while (now < 100000 && retries-- > 0) {
        delay(500);
        now = time(nullptr);
    }

    if (now > 100000) {
        Serial.print(" OK: ");
        Serial.println(ctime(&now));
    } else {
        Serial.println(" 失败");
    }
}

// ==========================================
//  天气获取 + 上报
// ==========================================
void doWeatherFetch() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WARN] WiFi 未连接，跳过");
        return;
    }

    // 如果没有 token，先注册
    if (g_token.length() == 0) {
        g_token = registerDevice();
        if (g_token.length() == 0) {
            Serial.println("[ERR] 设备未注册，跳过上报");
            return;
        }
    }

    static uint8_t retryCount = 0;

    Serial.print("[FETCH] 获取天气... ");
    WeatherRecord rec;
    bool ok = fetchFromOpenMeteo(&rec);

    if (!ok) {
        retryCount++;
        if (retryCount >= 3) {
            retryCount = 0;
            Serial.println("失败 (3次重试结束，等待下一周期)");
        } else {
            lastWeatherFetch = millis() - (WEATHER_INTERVAL_MS - RETRY_DELAY_MS);
            Serial.printf("失败 (30s后重试 %d/3)\n", retryCount);
        }
        return;
    }

    retryCount = 0;
    Serial.print("OK: ");
    Serial.print(rec.temp);
    Serial.print("°C, ");
    Serial.print(rec.humidity);
    Serial.print("%, ");
    Serial.println(getWeatherLabel(rec.weather_code));

    // 上报到后端
    Serial.print("[UPLOAD] 上报数据... ");
    if (uploadToBackend(rec)) {
        Serial.println("OK");
    } else {
        Serial.println("失败");
    }
}

// ==========================================
//  Open-Meteo HTTPS 数据获取
// ==========================================
bool fetchFromOpenMeteo(WeatherRecord* out) {
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);

    bool ok = false;
    http.begin(sslClient, String("https://") + API_HOST + API_URL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<768> doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err) {
            JsonObject current = doc["current"];
            out->temp         = current["temperature_2m"] | 0.0f;
            out->feels_like   = current["apparent_temperature"] | 0.0f;
            out->humidity     = current["relative_humidity_2m"] | 0;
            out->wind_speed   = current["wind_speed_10m"] | 0.0f;
            out->wind_dir     = current["wind_direction_10m"] | 0;
            out->weather_code = current["weather_code"] | 0;
            out->timestamp    = time(nullptr);
            ok = true;
        } else {
            Serial.print("JSON解析失败: ");
            Serial.println(err.c_str());
        }
    } else {
        Serial.print("HTTP ");
        Serial.print(httpCode);
        Serial.print(" (堆:");
        Serial.print(ESP.getFreeHeap());
        Serial.println("B)");
    }

    http.end();
    return ok;
}

// ==========================================
//  设备注册 — POST /api/sensor/register
// ==========================================
String registerDevice() {
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(5000);

    String url = String("http://") + BACKEND_HOST + ":" + STR(BACKEND_PORT) + "/api/sensor/register";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    String body = "{\"id\":\"" SENSOR_ID "\",\"name\":\"" SENSOR_NAME "\"}";
    int code = http.POST(body);

    String token = "";
    if (code == 200) {
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, http.getString());
        if (!err) {
            token = doc["token"].as<String>();
        }
    } else {
        Serial.print("[REG] HTTP ");
        Serial.println(code);
    }

    http.end();
    return token;
}

// ==========================================
//  数据上报 — POST /api/sensor/data
// ==========================================
bool uploadToBackend(const WeatherRecord& rec) {
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(5000);

    String url = String("http://") + BACKEND_HOST + ":" + STR(BACKEND_PORT) + "/api/sensor/data";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + g_token);
    http.addHeader("X-Device-Id", SENSOR_ID);

    StaticJsonDocument<256> doc;
    doc["temp"] = rec.temp;
    doc["feels_like"] = rec.feels_like;
    doc["humidity"] = rec.humidity;
    doc["wind_speed"] = rec.wind_speed;
    doc["wind_dir"] = rec.wind_dir;
    doc["weather_code"] = rec.weather_code;

    String body;
    serializeJson(doc, body);
    int code = http.POST(body);
    http.end();

    return code == 200;
}
