# Lessons Learned: 方案 A 联调测试

> 记录 ESP8266 传感器 + Node.js 后端 + Vue 3 前端联调过程中遇到的问题

**日期**：2026-07-10
**分支**：`feature/weather-station-web`

---

## 问题 1：`STRINGIFY(BACKEND_PORT)` 宏不展开

### 现象

```
后端地址: http://192.168.3.11:BACKEND_PORT
[REG] HTTP -1
```

### 根因

与方案 C 中 HTTP 400 完全相同的 C 预处理器陷阱 —— `#` 操作符不展开宏参数。`STRINGIFY(BACKEND_PORT)` 生成 `"BACKEND_PORT"` 字面量而非 `"3001"`。

### 修复

```cpp
// 错误
STRINGIFY(BACKEND_PORT)

// 正确 —— 使用双层宏 STR(x)
STR(BACKEND_PORT)
```

### 教训

**同一仓库内不同固件共享同样的坑**。方案 C 的 `artifacts/weather-station-debug-lessons.md` 已记录此问题，但方案 A 创建 ESP8266 固件时重复了同样错误。应该在 plan 阶段就把已知问题清单传给 implementer。

---

## 问题 2：HTTP -1 连接失败 —— 跨 SSID 隔离

### 现象

```
[REG] HTTP -1
[REG] 注册失败
```

- ESP8266 `ping` PC 通（ICMP 可达）
- NTP 时间同步正常（UDP 可达外网）
- PC 上 `curl localhost:3001` 正常
- Python 从 PC 访问 `192.168.3.11:3001` 正常
- 但 ESP8266 HTTP POST 到 `192.168.3.11:3001` 失败

### 根因

PC 连接 `BarryBlue` WiFi，ESP8266 连接 `SNOWSKY` WiFi —— **两个不同的 SSID**。

路由器对跨 SSID 流量做了 **客户端隔离（Client Isolation）**：
- ICMP (ping) 走路由器转发，可达
- TCP 直连被阻断，不可达

### 排查过程

1. `ping 192.168.3.37` → 通 → 排除物理断连
2. `netstat -ano | findstr 3001` → `0.0.0.0:3001 LISTENING` → 排除后端只监听 localhost
3. `curl` 从 PC 自身访问 → 200 OK → 排除后端代码问题
4. Python 从 PC 访问 `192.168.3.11:3001` → 200 OK → 排除防火墙（本地回环不经过防火墙）
5. 添加 `netsh advfirewall` 规则 → 仍 -1 → 排除 Windows 防火墙
6. 排查 SSID —— PC 连 `BarryBlue`，ESP8266 连 `SNOWSKY` → **锁定根因**

### 修复

让 ESP8266 连接与 PC 相同的 WiFi (`BarryBlue`)。

### 教训

1. **`ping` 通 ≠ TCP 通**。ICMP 和 TCP 的路由策略不同，尤其在企业/多 SSID 路由器上
2. **先验证同网络**：联调前确保所有设备连接同一个 SSID
3. **分层排查**：物理层 (ping) → 传输层 (netstat) → 应用层 (curl) → 防火墙 → 网络拓扑
4. **HTTP -1 (ESP8266)** 通常意味着 TCP 连接被拒绝或超时，不是 HTTP 层错误

---

## 问题 3：Windows Public 网络防火墙

### 现象

添加 `netsh advfirewall firewall add rule` 后仍然 HTTP -1

### 根因

WiFi 网络被 Windows 归类为 **Public（公用）** 而非 Private（专用），防火墙策略更严格。`netsh` 规则虽然添加了，但当时 ESP8266 还在错误的 SSID 上，防火墙并不是真正的原因。

### 教训

1. `Get-NetConnectionProfile` 查看网络类别
2. Public 网络下入站连接被更严格限制
3. 如果有多个问题叠加，先解决最底层（网络拓扑）再查上层

---

## 问题 4：`test.beforeEach` 跨文件副作用

### 现象

`node --test tests/db.test.js tests/sensor.test.js tests/weather.test.js tests/source.test.js` 运行时部分测试 `SQLITE_ERROR`

### 根因

所有测试文件共享同一个 `schema.js` 模块的全局 `db` 变量。一个测试文件的 `beforeEach` 会覆盖另一个测试刚设置的 in-memory DB。

### 修复

- 每个测试函数内部自建 `setupDb()` + 独立 DB 实例
- 避免使用全局 `beforeEach` 钩子
- 分开运行各测试文件避免模块状态污染

### 教训

1. `node --test` 不支持测试文件间隔离 —— 共享进程和模块缓存
2. 全局单例（如 `let db` in schema.js）需要 `setDb()` 接口支持测试注入
3. 集成测试每个用例应自包含 DB 实例

---

## 关键数据

| 项目 | 值 |
|------|-----|
| 固件 Flash | 486KB（比方案 C 少 ~50KB） |
| 固件 RAM | 39.6% |
| 后端 API 端点 | 8 个 |
| 后端测试 | 22/22 通过 |
| 前端组件 | 10 个（3 views + 4 components + router + store） |
| 修复的 bug 数 | 3 个（STRINGIFY、SSID 隔离、test 隔离） |
