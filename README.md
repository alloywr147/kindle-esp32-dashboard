# Kindle + ESP32 家庭日程板

手机网页编辑待办，Kindle 直接勾选和取消；ESP32-C3 Mini 保存数据和提供天气，
Kindle 本地程序绘制中文全屏。日常运行不需要电脑、域名、服务器或微信小程序。

```text
手机浏览器 ──家庭 Wi-Fi── ESP32-C3 Mini（网页、待办、黄历、天气）
                                │
                         Kindle 本地插件
                    中文全屏 / 触控勾选 / 取消
```

目前针对 **Kindle KT2 / Basic 2014 / 第七代，固件 5.12.2.2，600×800 竖屏**。
其他型号、分辨率和固件没有实机验证。Kindle 需要已越狱并能运行 KOReader；
本项目不提供通用越狱工具或保证其他固件可越狱。

## 当前功能

- 手机增删改、完成和取消待办；删除直接生效，不弹二次确认。
- Kindle 整行点击勾选/取消，完整大方框和允许超框的大勾；每页4条。
- 首页：日期、农历、苏州天气、温度/降水、宜、忌、冲煞、待办。
- 单独的完整黄历页与七日预报页，底部切换和手动退出。
- 待办每5秒检查版本；未变不重绘。每小时定期重读数据，苏州零点更新日期/黄历。
- ESP 自动天气缓存约1小时有效；失败保留缓存并至少间隔5分钟重试。
- 最多50条待办，每条120个 Unicode 字符；网页可导出 JSON，尚无导入界面。
- 两台设备持续供电运行，取消两小时退出限制。手动退出恢复原 Kindle 界面和休眠设置。

**真正休眠后由手机修改触发 Kindle 唤醒尚未实现。** 当前路线让 Kindle 保持运行，
不能当作低功耗休眠方案。没有开机自启、签名 OTA、BLE 配网或设备绑定。
本版本按需求采用局域网免密管理，不适合直接开放到公网。

## 准备

- ESP32-C3 Mini，4 MiB Flash；不需要焊针、焊接或连接 Kindle 内部线路。
- 上述已越狱的 KT2；ESP 和 Kindle 各自用 USB 充电器供电。
- 家庭2.4 GHz Wi-Fi；天气需要 ESP 能访问互联网。
- 一台电脑和支持数据的 USB 线，用于首次备份、刷写和安装。日常运行可关机。
- Python 3.10+、Node.js 不是必需；构建工具见 `requirements.txt`。

## 复刻顺序

1. 阅读 [验证范围](docs/VERIFICATION.md) 和 [隐私说明](docs/PRIVACY.md)。
2. 按 [ESP 安装说明](docs/ESP32_SETUP.md) 备份原 Flash、构建、首次安装并配网。
3. 按 [Kindle 安装说明](docs/KINDLE_SETUP.md) 准备官方 KOReader 与本地安装目录。
4. 按 [日常使用与升级](docs/OPERATIONS.md) 测试手机编辑、Kindle 勾选以及退出。

源码结构：

```text
esp32/                 PlatformIO 工程、网页、公共黄历数据、TLS 根证书
  lib/WebServer/       带原许可和修改说明的项目内库副本
  tools/               黄历生成、网页嵌入、串口配网辅助
kindle/                KOReader 插件、通用启动/停止脚本、配置示例、Lua测试
tools/                 备份/刷写、生成本地Kindle安装包、发布检查
docs/                  从首次安装到使用、回退和已知限制
```

## 可配置与暂未通用化的部分

Kindle 的 ESP 地址由 `kt2-config.json` 的 `base_url` 配置，没有写死家庭 IP；
串口通过命令参数选择。安装目录统一为 `/mnt/us/kt2-dashboard/native`。
**城市目前仍固定苏州**（经纬度31.30、120.58），时区为 Asia/Shanghai；
改城市需要同时改 `esp32/src/weather.cpp`、相关“苏州”显示文字并重新构建。
不是在网页中直接切换城市。黄历数据范围2026—2035，超出后需要重新生成及调整索引基准。

## 发布前本地检查

```powershell
python tools/check_release.py
python -m unittest discover -s esp32/tests -p test_calendar.py
python esp32/tools/embed_ui.py
python -m platformio run --project-dir esp32
python -m platformio run --project-dir esp32 --target buildfs
```

`docs/VERIFICATION.md` 区分原部署的真机验证与通用发布副本的本地检查。
不要把“编译通过”当作已在另一台设备安装成功。

许可证按目录区分，见 [LICENSE.md](LICENSE.md) 和 [THIRD_PARTY.md](THIRD_PARTY.md)。

