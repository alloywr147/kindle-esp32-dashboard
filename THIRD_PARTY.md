# 来源与许可证

这是一套针对 ESP32-C3 Mini 与 Kindle KT2 的组合实现，不是上游项目的官方版本。

| 组件 | 使用位置、来源与许可 |
|---|---|
| KOReader v2026.07.1 | Kindle 原生绘图、输入和字体运行时。AGPL-3.0；[官方发布](https://github.com/koreader/koreader/releases/tag/v2026.07.1)。本仓库只提供插件，不打包运行时 |
| Arduino-ESP32 2.0.17 | ESP SDK，通过 PlatformIO espressif32 6.12.0 获取；[上游源码](https://github.com/espressif/arduino-esp32/tree/2.0.17)。组件许可随 SDK 提供 |
| WebServer | 项目内保留 Arduino-ESP32 2.0.17 的库副本，LGPL-2.1-or-later。`esp32/lib/WebServer/LOCAL_CHANGES.md` 记录请求体大小限制等修改，完整许可及原版权头均保留 |
| ArduinoJson 6.21.5 | JSON 编解码，MIT；[上游](https://github.com/bblanchon/ArduinoJson) |
| lunar-python 1.4.8 | 离线生成农历、宜忌等公开历法数据，MIT；[上游](https://github.com/6tail/lunar-python)。生成结果不是用户数据 |
| fridge-eink-display | 最初参考的信息屏路线；本地沿用的黄历适配器保留 Apache-2.0 与原 NOTICE。[原项目](https://github.com/dinggefei/fridge-eink-display)。其 ESP32-S3、四色屏、小程序、FastAPI 架构并未整套移植到本项目 |
| Open-Meteo | 苏州天气与七日预报数据；[API 文档](https://open-meteo.com/en/docs)、[服务条款](https://open-meteo.com/en/terms)。保留页面署名；自行确认使用规模符合服务条款 |
| ISRG Root X1 | `esp32/data/weather-root.pem` 是公开 CA 根证书，用于 TLS 验证，[证书来源](https://letsencrypt.org/certificates/)，不是设备私钥 |
| PlatformIO / esptool / pyserial | 电脑构建、备份和串口工具，分别遵守其 Apache-2.0 / GPL-2.0-or-later / BSD 许可；从依赖源安装，不打包工具二进制 |

`esp32/NOTICE` 中的旧目录引用是原项目通知的原文，不表示本仓库包含那个后端。
Kindle 系统字体由设备提供，不复制或发布字体文件。发布预编译固件之前，需同时满足
SDK 和项目内 LGPL 库对应的源码、重链接等分发要求；本次仅整理源码，不发布固件二进制。

