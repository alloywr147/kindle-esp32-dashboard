# ESP32：首次安装与配网

以下命令在仓库根目录运行，示例为 Windows PowerShell。把 `COM4` 换成你自己的串口。
串口只用于首次安装或更新，不需要为日常运行保留电脑。

## 1. 工具与原固件备份

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python -m serial.tools.list_ports
python tools/esp_flash.py backup --port COM4 --backup private/esp-original
```

备份程序检查 ESP32-C3、4 MiB Flash、未启用 Secure Boot/Flash Encryption，
连续读取两份完整 Flash，确认一致后保存 MAC 和 SHA256。
`private/` 会被 Git 忽略；备份可能含原程序及网络信息，不要上传。
程序拒绝重复覆盖已有备份目录。新换一块板子必须单独备份。

若无串口：换支持数据的线，直接接电脑；必要时按住 BOOT 插入 USB，再松开。
不同 C3 Mini 的 USB 实现可能不同，本项目没有为所有板型配置驱动。
不需要接 GPIO，也不需要把 ESP 的 USB 接到 Kindle。

## 2. 构建

```powershell
python esp32/tools/embed_ui.py
python -m platformio run --project-dir esp32
python -m platformio run --project-dir esp32 --target buildfs
```

公共 `calendar.dat`、`calendar.idx` 已随源码提供，可直接构建。
需要重建2026—2035黄历时：`python esp32/tools/generate_calendar.py`。
公共证书 `weather-root.pem` 用于 HTTPS 校验，不要替换成设备私钥。

## 3. 首次写入

```powershell
python tools/esp_flash.py install --port COM4 --backup private/esp-original --first-install
```

这会写入 bootloader、分区表、应用与全新 LittleFS，**只用于初次安装**。
在已经有待办的设备上不要重复此步骤；升级见 OPERATIONS.md。
程序不执行整片擦除，会核对同一块板子的备份和 MAC；写入由 esptool 校验。
刷写期间关闭占用串口的监视器或桥接程序。

## 4. 取得热点密码并配网

```powershell
python esp32/tools/serial_device.py setup --port COM4
```

仅在你自己的电脑上查看设备的随机配网热点名和密码。没有家庭 Wi-Fi 配置时，
ESP 会提供 `KT2-Setup-xxxx` 热点；连接失败超过约30秒也会开启热点。

1. 手机连接该热点，用浏览器访问 `http://192.168.4.1/`，不要使用 HTTPS。
2. 展开“家庭 Wi-Fi 配置”，输入2.4 GHz Wi-Fi 名称、密码，点“保存并连接”。
3. 密码输入框会清空，ESP 重启；这是隐藏已保存内容，不是连接失败证明。
4. 手机切回家里的 Wi-Fi，运行下面命令查看 ESP 实际 IP：

```powershell
python esp32/tools/serial_device.py status --port COM4
```

`wifi_connected: true` 且浏览器能打开 `http://<ESP_IP>/` 才算成功。
Wi-Fi 密码目前接受8—63字节；空密码和开放网络不支持。
DHCP 地址可能变化；需修改 Kindle 配置为新地址，或自行配置稳定的地址分配。
本项目的安装工具不会修改路由器。

## 5. 离开电脑

先确认手机能新增待办，再把 ESP 接独立充电器，等待联网后复测。
Kindle 安装见 KINDLE_SETUP.md。ESP 设置了较保守的8.5 dBm发射功率且关闭无线省电，
这是原板稳定性调试后的取值，不保证每块不同天线/电源的板子都适合。

