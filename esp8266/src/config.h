#pragma once
#include <cstdint>
#include <time.h>

// 双层宏展开：先展开参数中的宏，再字符串化
#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)

// ---- WiFi ----
#define WIFI_AP_NAME        "ESP-Weather-Station"
#define WIFI_AP_PASSWORD    "weather123"

// ---- Open-Meteo API ----
#define API_HOST            "api.open-meteo.com"
#define API_URL             "/v1/forecast?latitude=" \
                            STR(WEATHER_LOCATION_LAT) \
                            "&longitude=" \
                            STR(WEATHER_LOCATION_LON) \
                            "&current=temperature_2m," \
                            "relative_humidity_2m," \
                            "apparent_temperature," \
                            "weather_code,wind_speed_10m," \
                            "wind_direction_10m&timezone=auto"

// ---- Backend Server (方案 A: ESP8266 上报数据到此服务器) ----
#ifndef BACKEND_HOST
#define BACKEND_HOST  "192.168.3.100"   // 修改为 PC 实际 IP
#endif
#ifndef BACKEND_PORT
#define BACKEND_PORT  3001
#endif
#define SENSOR_ID     "esp8266-01"
#define SENSOR_NAME   "客厅传感器"

// ---- Timing ----
#define WEATHER_INTERVAL_MS (10 * 60 * 1000UL)  // 10分钟
#define NTP_INTERVAL_MS     (60 * 60 * 1000UL)  // 1小时
#define RETRY_DELAY_MS      (30 * 1000UL)       // 失败重试延迟

// ---- Data Structures ----
struct WeatherRecord {
    time_t timestamp;
    float temp;            // °C
    float feels_like;      // °C
    uint8_t humidity;      // %
    float wind_speed;      // km/h
    uint16_t wind_dir;     // 度
    int8_t weather_code;   // WMO code
};
