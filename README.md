# ESP8266 Test Project

[![PlatformIO](https://img.shields.io/badge/PlatformIO-6.1+-orange)](https://platformio.org)
[![Framework](https://img.shields.io/badge/Arduino_Core-3.1.2-blue)](https://github.com/esp8266/Arduino)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)

ESP8266 物联网（IoT）开发项目。基于 PlatformIO + Arduino 框架，支持 NodeMCU、WeMos D1 Mini、ESP-01 等常见 ESP8266 开发板。

## 硬件需求

| 组件 | 说明 |
|------|------|
| MCU | ESP8266EX (Xtensa LX106 @ 80MHz) |
| RAM | 80 KB（约 50 KB 可用堆空间） |
| Flash | 4 MB SPI Flash |
| WiFi | 802.11 b/g/n |
| 接口 | USB-to-UART (CP2102 / CH340G) |

## 开发环境

- **构建系统**: [PlatformIO](https://platformio.org) 6.1.19
- **框架**: Arduino Core for ESP8266 3.1.2
- **交叉编译器**: xtensa-lx106-elf-gcc 10.3.0
- **烧录工具**: esptool.py 3.0.0

### 环境搭建

```bash
# 安装 PlatformIO
pip install platformio

# 验证安装
pio --version

# 检查支持的 ESP8266 开发板
pio boards esp8266
```

## 快速开始

### 编译

```bash
pio run
```

### 烧录固件

确保 ESP8266 已通过 USB 连接并识别到串口：

```bash
pio run --target upload
```

### 串口监视器

```bash
pio device monitor
```

### 清理

```bash
pio run --target clean
```

## 项目结构

```
├── src/               # 源代码
│   └── main.cpp       # 主程序入口
├── lib/               # 项目自定义库
├── test/              # 单元测试
├── Docs/              # 项目文档
├── .claude/           # Claude Code 配置
├── platformio.ini     # PlatformIO 板级配置
├── CLAUDE.md          # AI 助手指令
├── README.md          # 本文件
└── LICENSE            # 开源许可证
```

## 开发板配置

当前配置为 **NodeMCU 1.0 (ESP-12E)**，在 `platformio.ini` 中可切换：

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2       ; 改为 d1_mini / esp01_1m 等
framework = arduino
monitor_speed = 115200
upload_speed = 921600
```

> 查看所有支持板型：`pio boards esp8266`

## 验证测试

首次烧录后，固件会自动执行硬件自检：

- ✅ 系统信息（芯片 ID、频率、SDK 版本、Flash 大小）
- ✅ GPIO 输出（板载 LED 闪烁）
- ✅ 内存状态（堆使用、碎片率）
- ✅ WiFi 射频扫描

## 许可协议

本项目采用 MIT 许可证 — 详见 [LICENSE](LICENSE)。
