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
    static char buffer[16];
    for (int i = 0; i < WEATHER_CODE_COUNT; i++) {
        WeatherCodeEntry entry;
        memcpy_P(&entry, &WEATHER_CODES[i], sizeof(WeatherCodeEntry));
        if (entry.code == code) {
            strcpy_P(buffer, entry.label);
            return buffer;
        }
    }
    strcpy_P(buffer, PSTR("❓ 未知"));
    return buffer;
}

inline const char* getWindDirLabel(uint16_t deg) {
    static const char W_N[] PROGMEM = "北";
    static const char W_NE[] PROGMEM = "东北";
    static const char W_E[] PROGMEM = "东";
    static const char W_SE[] PROGMEM = "东南";
    static const char W_S[] PROGMEM = "南";
    static const char W_SW[] PROGMEM = "西南";
    static const char W_W[] PROGMEM = "西";
    static const char W_NW[] PROGMEM = "西北";

    static char buffer[7];
    const char* selected;

    if (deg < 22)         selected = W_N;
    else if (deg < 67)    selected = W_NE;
    else if (deg < 112)   selected = W_E;
    else if (deg < 157)   selected = W_SE;
    else if (deg < 202)   selected = W_S;
    else if (deg < 247)   selected = W_SW;
    else if (deg < 292)   selected = W_W;
    else if (deg < 337)   selected = W_NW;
    else                  selected = W_N;

    strcpy_P(buffer, selected);
    return buffer;
}
