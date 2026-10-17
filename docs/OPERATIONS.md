# 使用、升级和回退

## 日常

手机与两台设备在同一家庭 Wi-Fi；浏览器直接打开 ESP 地址，不需要密钥。
网页删除直接提交，没有二次确认。重要待办可先点“导出待办备份”；目前没有导入界面。
日期/农历由 ESP 的北京时间提供，联网 NTP 校时前会提示不可用。
黄历数据目前2026—2035年，宜忌是传统民俗信息。

Kindle 每5秒检查小版本；待办变化触发读取。天气由ESP约每小时获取，Kindle每整点定期重读，
二者不保证在同一秒执行，所以画面可能稍晚反映最新天气。网络失败会保留缓存并退避重试。
“已同步”时间是最近一次读入数据的时间，不是联网心跳。

Kindle 重启或手动退出后，用 `Start Native KT2 Board` 重开。
没有开机自启。屏幕保留画面不代表程序还运行，判断方法是手机改一条测试待办。

## ESP 应用升级：保留待办

先网页导出待办，再只构建应用；不要上传新文件系统：

```powershell
python esp32/tools/embed_ui.py
python -m platformio run --project-dir esp32
python tools/esp_flash.py update --port COM4 --backup private/esp-original
```

`update` 只写应用区0x10000，不写NVS和LittleFS，因此保留家庭Wi-Fi、黄历和待办。
工具核对原板MAC和两份原始备份；它只支持本项目既定4MiB分区布局，不支持从任意第三方固件
直接按此模式迁移，也不意味着以后改变数据格式的升级仍自动兼容。
更新后看 `/health` 与原待办，等 Kindle 自动重连。

## Kindle 插件升级

先点底部“退出”，在电脑上保存本地配置和旧插件。将发布版
`kindle/plugin/kt2board.koplugin` 覆盖到原生目录中同名插件目录，保留 `kt2-config.json`，
重新启动。启动脚本升级也应先退出。更换 KOReader 版本需单独兼容性验证。

## 回退

- Kindle：退出后复制回自己的旧插件和启动脚本，再启动。不需要重置 Kindle。
- ESP：可用自己保存的兼容旧 `firmware.bin` 仅写回0x10000。完整原Flash恢复会同时覆盖当前
  待办和Wi-Fi，必须先导出/备份。确认板子MAC属于该完整备份后才执行：

```powershell
python -m esptool --chip esp32c3 --port COM4 write-flash 0x0 private/esp-original/original-1.bin
```

不要把另一个板子的完整镜像刷到当前板子，不要先执行 erase-flash。

## 常见情况

| 现象 | 检查 |
|---|---|
| 网页打不开 | 手机是否同一Wi-Fi、ESP是否供电、DHCP是否换地址 |
| 电脑localhost地址打不开 | 正常入口是ESP地址；不依赖旧的电脑桥接页 |
| 保存Wi-Fi后密码框清空 | 正常隐藏；以实际联网状态和IP为准 |
| Kindle不更新 | 是否已运行原生看板、ESP地址是否正确、Wi-Fi是否联网 |
| 网络失败后画面没变 | 缓存保留，不代表写入成功；等待同步，避免重复提交 |
| 没有七日预报/中文黄历 | 检查网络、NTP时钟、公开证书和黄历文件；不要关闭TLS校验来绕过错误 |
| 中文显示异常 | 核对官方KOReader包、设备系统字体和型号；不分发商业字体 |
| 点电源后不再同步 | 没有实现真正休眠网络唤醒；先唤醒并重新打开原生看板 |

