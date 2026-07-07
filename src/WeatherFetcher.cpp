#include "WeatherFetcher.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <BearSSLHelpers.h>
#include <ArduinoJson.h>

static BearSSL::WiFiClientSecure client;

void weather_init() {
    client.setInsecure();
    client.setBufferSizes(1024, 512);  // MFLN: 接收缓冲从16KB→512B
}

bool weather_fetch(WeatherRecord* out) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);

    bool ok = false;
    http.begin(client, String("https://") + API_HOST + API_URL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        // 使用过滤，只解析需要的字段 —— 大幅减少内存占用
        StaticJsonDocument<96> filter;
        JsonObject filter_current = filter["current"].to<JsonObject>();
        filter_current["temperature_2m"] = true;
        filter_current["relative_humidity_2m"] = true;
        filter_current["apparent_temperature"] = true;
        filter_current["weather_code"] = true;
        filter_current["wind_speed_10m"] = true;
        filter_current["wind_direction_10m"] = true;

        // 使用 Stream 直接解析，避免 String 中转
        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, http.getStream(),
            DeserializationOption::Filter(filter));

        if (!err) {
            JsonObject current = doc["current"];
            out->temp       = current["temperature_2m"];
            out->feels_like = current["apparent_temperature"];
            out->humidity   = current["relative_humidity_2m"];
            out->wind_speed = current["wind_speed_10m"];
            out->wind_dir   = current["wind_direction_10m"];
            out->weather_code = current["weather_code"];
            out->timestamp  = time(nullptr);
            ok = true;
        }
    }

    http.end();
    return ok;
}
