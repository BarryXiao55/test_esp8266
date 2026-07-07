# Lessons Learned: ESP8266 Weather Station - HTTPS API Debugging

> 记录在 ESP8266 上调试 Open-Meteo HTTPS 天气数据获取时遇到的三个问题及其根因分析。

**日期**：2026-07-07
**分支**：`feature/weather-station-esp`
**相关提交**：`790ac2f`

---

## 问题 1：HTTP 400 Bad Request — C 预处理器宏陷阱

### 现象

```
[WEATHER] HTTP 400 (堆空闲:30208B)
```

TLS 握手成功，但 Open-Meteo 返回 400 Bad Request。

### 根因

ESP8266 通过 PlatformIO build_flags 传入坐标参数：

```ini
build_flags =
    -DWEATHER_LOCATION_LAT=31.23
    -DWEATHER_LOCATION_LON=121.47
```

config.h 中用 `STRINGIFY` 宏构造 API URL：

```cpp
#define STRINGIFY(x) #x
#define API_URL "/v1/forecast?latitude=" STRINGIFY(WEATHER_LOCATION_LAT) ...
```

**C 预处理器规则**：`#` 操作符不会先展开宏参数。所以 `STRINGIFY(WEATHER_LOCATION_LAT)` 产生的是字面量 `"WEATHER_LOCATION_LAT"` 而非 `"31.23"`。

实际发出的 URL：
```
/v1/forecast?latitude=WEATHER_LOCATION_LAT&longitude=WEATHER_LOCATION_LON&...
```

### 修复

双层宏间接展开，先展开参数中的宏再字符串化：

```cpp
#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)   // 先展开 x 中的宏，再传给 STRINGIFY

#define API_URL "/v1/forecast?latitude=" STR(WEATHER_LOCATION_LAT) ...
```

### 教训

1. **`-D` 传入的值不是字符串**：`-DFOO=bar` 定义的是标识符 `bar`，不是字符串 `"bar"`
2. **`#` 操作符不展开宏**：需要一层中间宏来做 "double expansion"
3. **测试策略**：可以在编译后检查 `.i` 预处理文件验证宏展开结果：
   ```bash
   # 验证 URL 中的坐标是否正确展开
   pio run -t clean && pio run 2>&1 | grep -o 'latitude=[^&]*'
   ```
   或者直接加一条编译时 `#pragma message` 打印展开后的 URL

---

## 问题 2：JSON IncompleteInput — TLS 缓冲区太小

### 现象

切换到 `getString()` + 打印原始响应后：

```
[WEATHER] JSON解析失败: IncompleteInput
[WEATHER] 原始响应: {"latitude":31.247803,...
```

响应被截断在 ~200 字符处（JSON 对象未闭合）。

### 根因

`BearSSL::WiFiClientSecure::setBufferSizes(1024, 512)` 将 TLS 接收缓冲区从默认 16KB 压缩到 512B。MFLN（Maximum Fragment Length Negotiation）本应将 TLS 记录限制在 512B，但实际环境中：

- HTTP 响应头 + JSON body 总计 ~550-650 字节
- TLS 记录可能超过 512B（取决于服务器是否支持 MFLN）
- 超过缓冲区的数据被截断，HTTPClient 收到的响应不完整

### 修复

增大 TLS 缓冲区：

```cpp
client.setBufferSizes(2048, 1024);  // TX 2KB, RX 1KB
```

1KB RX 缓冲区足够容纳单次 API 响应的完整 TLS 记录。相比默认的 16KB 仍然节省了 15KB RAM。

### 教训

1. **MFLN 是协商特性，不可依赖**：服务器可能不支持或实现不同
2. **最小可行缓冲区**：需要实际测量响应大小来确定。用 `curl -s -w '%{size_download}' URL` 先测得 body 大小，再留出 HTTP 头 + TLS 开销
3. **512B 适合极简 API**（如返回 `{"t":33.3}` 的定制端点），不适合通用 REST API

---

## 问题 3：JSON 解析返回全 0 — Filtered Parsing 容量不足

### 现象

HTTP 200 成功，但解析结果都是默认值：

```
[WEATHER] 获取数据... OK: 0.00°C, 0%, ☀️ 晴
```

实际 API 返回的是 `33.3°C, 65%, ☁️`。

### 根因

使用 ArduinoJson v6 的 `DeserializationOption::Filter` 进行过滤解析：

```cpp
StaticJsonDocument<96> filter;   // ← 96 bytes 不够 6 个字段
JsonObject filter_current = filter["current"].to<JsonObject>();
filter_current["temperature_2m"] = true;
filter_current["relative_humidity_2m"] = true;
// ... 共 6 个字段
```

`StaticJsonDocument<96>` 的内存布局：
- 文档元数据：~16 bytes
- 每个 JSON 键值对：~16 bytes（键指针 + 值联合体）
- 6 个字段：6 × 16 = 96 bytes
- 总计需要：16 + 96 = 112 bytes → **超过 96 bytes 上限**

部分 filter key 被静默丢弃，对应的 JSON 字段不会被解析，`JsonObject` 的 `|` 运算符返回默认值 0。

### 修复

放弃过滤解析，改用全量解析。Open-Meteo API 响应仅 ~430 bytes，在 `StaticJsonDocument<768>` 内完全装得下：

```cpp
String payload = http.getString();
StaticJsonDocument<768> doc;
DeserializationError err = deserializeJson(doc, payload);
```

同时改用 `|` 运算符提供默认值防御：

```cpp
out->temp = current["temperature_2m"] | 0.0f;  // key 不存在时返回 0.0
```

### 教训

1. **ArduinoJson 不会报 filter 满**：键值对静默丢弃，不会触发 `NoMemory` 错误
2. **计算容量公式**（v6）：每个对象成员 ~16 bytes + 键名字符串（池化后 ~4 bytes）+ 文档开销 ~16 bytes
3. **过滤解析的取舍**：适合大 JSON 取少量字段的场景，但 filter 本身的容量必须够。对 ~400 字节的小 JSON，直接全量解析更简单可靠
4. **用 `|` 防御**：`json["key"] | default_value` 比 `json["key"].as<float>()` 更安全，避免 key 缺失时的未定义行为

---

## 调试方法论总结

1. **分层隔离**：先确认 HTTP 层（状态码），再检查 TLS 层（缓冲区），最后排查 JSON 层（解析）
2. **打印原始数据**：`IncompleteInput` 错误 → 打印 `payload` 内容 → 立刻发现截断位置
3. **已知良好对照**：用 `curl` 在 PC 端验证 API URL 和响应 → 排除服务端问题
4. **增量修复**：每个问题单独修、单独验证，不堆在一起改

## 关键数据

| 参数 | 值 |
|------|-----|
| API 响应 body 大小 | ~430 bytes |
| HTTP 响应总大小（含头） | ~600 bytes |
| 修复后 TLS RX buffer | 1024 bytes |
| 修复后 JSON doc | 768 bytes |
| 修复后剩余堆 | ~30KB |
| 编译固件大小 | ~535KB（压缩后 ~380KB） |
