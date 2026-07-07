#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include "config.h"
#include "RingBuffer.h"
#include "WeatherFetcher.h"
#include "WebServer.h"
#include "WeatherCodes.h"

// ---- 调度状态 ----
static unsigned long lastWeatherFetch = 0;
static unsigned long lastNtpSync = 0;
static bool ntpSynced = false;

// ---- 函数声明 ----
void syncNtp();
void doWeatherFetch();

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("===================================");
    Serial.println("  ESP8266 Weather Station v1.0");
    Serial.println("===================================");

    // 提升 CPU 频率加速 SSL
    system_update_cpu_freq(160);

    // 1. WiFiManager 配网
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    wm.setConnectTimeout(15);

    bool connected = wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);
    if (!connected) {
        Serial.println("[ERR] WiFi 配网失败，重启...");
        ESP.restart();
    }
    Serial.print("[OK] WiFi 已连接: ");
    Serial.println(WiFi.SSID());
    Serial.print("    IP 地址: ");
    Serial.println(WiFi.localIP());

    // 2. 初始化学组件
    ring_init();
    weather_init();
    webserver_init();

    // 3. NTP 时间同步
    syncNtp();

    // 4. 首次数据获取
    doWeatherFetch();

    Serial.println("\n[READY] 打开浏览器访问 http://" + WiFi.localIP().toString());
}

void loop() {
    unsigned long now = millis();

    // 每60分钟同步NTP
    if (now - lastNtpSync >= NTP_INTERVAL_MS) {
        syncNtp();
        lastNtpSync = now;
    }

    // 每10分钟获取天气数据
    if (now - lastWeatherFetch >= WEATHER_INTERVAL_MS) {
        doWeatherFetch();
        lastWeatherFetch = now;
    }

    // ESP8266 喂狗（硬件WDT自动运行，但保持loop不太忙）
    yield();
    delay(10);
}

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
        ntpSynced = true;
        Serial.print(" OK: ");
        Serial.println(ctime(&now));
    } else {
        Serial.println(" 失败 (使用 unix epoch)");
    }
}

void doWeatherFetch() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WARN] WiFi 未连接，跳过获取");
        return;
    }

    Serial.print("[WEATHER] 获取数据... ");
    WeatherRecord rec;
    bool ok = weather_fetch(&rec);

    if (ok) {
        ring_push(rec);
        Serial.print("OK: ");
        Serial.print(rec.temp);
        Serial.print("°C, ");
        Serial.print(rec.humidity);
        Serial.print("%, ");
        Serial.println(getWeatherLabel(rec.weather_code));
    } else {
        Serial.println("失败 (将在 30s 后重试)");
        // 设置提前重试
        lastWeatherFetch = millis() - (WEATHER_INTERVAL_MS - RETRY_DELAY_MS);
    }
}
