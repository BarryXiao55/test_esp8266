#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include "config.h"
#include "RingBuffer.h"
#include "WeatherFetcher.h"
#include "WebServer.h"
#include "WeatherCodes.h"

// ---- 调度状态 ----
static unsigned long lastWeatherFetch = 0;
static unsigned long lastNtpSync = 0;

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

    // 3. mDNS 注册（可通过 http://esp-weather.local 访问）
    if (MDNS.begin("esp-weather")) {
        Serial.println("[OK] mDNS 已启动: http://esp-weather.local");
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println("[WARN] mDNS 启动失败");
    }

    // 4. NTP 时间同步
    syncNtp();

    // 5. 首次数据获取
    doWeatherFetch();

    Serial.println("\n[READY] 打开浏览器访问:");
    Serial.println("       http://" + WiFi.localIP().toString());
    Serial.println("       http://esp-weather.local");
}

void loop() {
    unsigned long now = millis();

    MDNS.update();  // 保持 mDNS 响应

    // 串口命令处理
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
        } else if (cmd == "help") {
            Serial.println("[CMD] 可用命令:");
            Serial.println("  reset  - 清除 WiFi 配置，重启进入配网模式");
            Serial.println("  ip     - 显示当前 IP 地址");
            Serial.println("  help   - 显示此帮助");
        }
    }

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

    static uint8_t retryCount = 0;

    Serial.print("[WEATHER] 获取数据... ");
    WeatherRecord rec;
    bool ok = weather_fetch(&rec);

    if (ok) {
        retryCount = 0;
        ring_push(rec);
        Serial.print("OK: ");
        Serial.print(rec.temp);
        Serial.print("°C, ");
        Serial.print(rec.humidity);
        Serial.print("%, ");
        Serial.println(getWeatherLabel(rec.weather_code));
    } else {
        retryCount++;
        if (retryCount >= 3) {
            retryCount = 0;
            Serial.println("失败 (3次重试结束，等待下一周期)");
            // 不修改 lastWeatherFetch，自然等到下一周期
        } else {
            lastWeatherFetch = millis() - (WEATHER_INTERVAL_MS - RETRY_DELAY_MS);
            Serial.printf("失败 (30s后重试 %d/3)\n", retryCount);
        }
    }
}
