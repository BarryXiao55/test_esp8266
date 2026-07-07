#include "WeatherFetcher.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <BearSSLHelpers.h>
#include <ArduinoJson.h>

static BearSSL::WiFiClientSecure client;

void weather_init() {
    // setInsecure() 跳过证书验证（演示项目权衡）
    // 生产环境应使用 setFingerprint() 或 setTrustAnchors()
    client.setInsecure();
    client.setBufferSizes(2048, 1024);  // MFLN: TX 2KB, RX 1KB (hold full JSON response)
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
        // HTTPS 下 getStream() 可能不可靠，用 getString()
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
            Serial.print("[WEATHER] JSON解析失败: ");
            Serial.println(err.c_str());
            // 打印原始响应前 200 字符用于调试
            Serial.print("[WEATHER] 原始响应: ");
            Serial.println(payload.substring(0, 200));
        }
    } else {
        Serial.print("[WEATHER] HTTP ");
        Serial.print(httpCode);
        Serial.print(" (堆空闲:");
        Serial.print(ESP.getFreeHeap());
        Serial.println("B)");
    }

    http.end();
    return ok;
}
