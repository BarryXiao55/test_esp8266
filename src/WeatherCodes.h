#pragma once
#include <Arduino.h>
#include <pgmspace.h>

struct WeatherCodeEntry {
    int8_t code;
    const char* label;
};

// WMO Weather interpretation codes (WW)
// https://open-meteo.com/en/docs#weathervariables
static const WeatherCodeEntry WEATHER_CODES[] PROGMEM = {
    {0,   "☀️ 晴"},
    {1,   "🌤️ 大部晴朗"},
    {2,   "⛅ 多云"},
    {3,   "☁️ 阴"},
    {45,  "🌫️ 雾"},
    {48,  "🌫️ 雾凇"},
    {51,  "🌦️ 小毛毛雨"},
    {53,  "🌦️ 毛毛雨"},
    {55,  "🌧️ 大毛毛雨"},
    {56,  "🌦️ 冻毛毛雨"},
    {57,  "🌧️ 冻大毛毛雨"},
    {61,  "🌦️ 小雨"},
    {63,  "🌧️ 中雨"},
    {65,  "🌧️ 大雨"},
    {66,  "🌧️ 冻雨"},
    {67,  "🌧️ 冻大雨"},
    {71,  "❄️ 小雪"},
    {73,  "❄️ 中雪"},
    {75,  "❄️ 大雪"},
    {77,  "❄️ 雪粒"},
    {80,  "🌦️ 小阵雨"},
    {81,  "🌧️ 中阵雨"},
    {82,  "🌧️ 大阵雨"},
    {85,  "❄️ 小阵雪"},
    {86,  "❄️ 大阵雪"},
    {95,  "⛈️ 雷暴"},
    {96,  "⛈️ 雷暴+小冰雹"},
    {99,  "⛈️ 雷暴+大冰雹"},
};

static constexpr int WEATHER_CODE_COUNT = sizeof(WEATHER_CODES) / sizeof(WEATHER_CODES[0]);

inline const char* getWeatherLabel(int8_t code) {
    for (int i = 0; i < WEATHER_CODE_COUNT; i++) {
        WeatherCodeEntry entry;
        memcpy_P(&entry, &WEATHER_CODES[i], sizeof(WeatherCodeEntry));
        if (entry.code == code) return entry.label;
    }
    return "❓ 未知";
}

inline const char* getWindDirLabel(uint16_t deg) {
    if (deg < 22)  return "北";
    if (deg < 67)  return "东北";
    if (deg < 112) return "东";
    if (deg < 157) return "东南";
    if (deg < 202) return "南";
    if (deg < 247) return "西南";
    if (deg < 292) return "西";
    if (deg < 337) return "西北";
    return "北";
}
