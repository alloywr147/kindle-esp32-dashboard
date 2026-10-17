# Kindle：本地全屏安装

## 先决条件

仅实测 KT2 / Basic 2014 / 第七代，固件5.12.2.2、600×800竖屏。
设备已完成适合该型号/固件的越狱及 Hotfix，并能运行自定义 shell/KOReader。
越狱入口会随固件不同而变化，不要照搬其他型号的升级包。
本项目不安装 SSH、不设置密码或公钥、不修改内核和路由器，也不提供系统自启。

已有能执行图书馆 `.sh` 文件的启动环境，可用下方图书馆入口。
如果你的启动环境只支持 KUAL，需自行在 KUAL 中添加执行
`sh /mnt/us/kt2-dashboard/native/start-native.sh` 的菜单项。
**普通未越狱 Kindle 不能把 `.sh` 文件当作可运行程序。**

## 1. 获取并核验运行时

下载 [官方 v2026.07.1 发布](https://github.com/koreader/koreader/releases/tag/v2026.07.1)
的 `koreader-kindlepw2-v2026.07.1.zip`，不要按 KT2 名称自行猜选另一种包。
该选择来自这一台 KT2 的实际测试，不代表所有老 Kindle 都适用。

SHA256：

```text
ea1f575c54492a2c679d128b7f3210fd7d6a87e5f5a1ff1f7a7fe2080ff68f86
```

生成器也会校验它；不符就停止。换 KOReader 版本需重新核验兼容性及更新元数据。
运行时、Kindle 系统字体不在本仓库中，不需要购买字体或上传自己的系统字体。

## 2. 在电脑上生成本地安装目录

把地址换成你自己的 ESP 地址，去掉末尾 `/`：

```powershell
python tools/prepare_kindle.py --koreader-zip private/downloads/koreader-kindlepw2-v2026.07.1.zip --esp-url http://192.168.1.50 --output private/kindle-usb
```

输出目录不能已经存在，以免覆盖旧配置。输出包括官方运行时、插件和你填写的本地地址，
属于本机安装产物，不要把 `private/kindle-usb` 上传 GitHub。

## 3. 复制到 Kindle

连接 Kindle USB，在资源管理器中把生成目录中的 **`kt2-dashboard` 和 `documents`
两个目录**复制到 Kindle 磁盘根目录。不要把整个 `kindle-usb` 外壳目录复制过去。
应得到：

```text
Kindle磁盘/
  documents/Start Native KT2 Board.sh
  documents/Stop Native KT2 Board.sh
  kt2-dashboard/native/start-native.sh
  kt2-dashboard/native/stop-native.sh
  kt2-dashboard/native/koreader/koreader.sh
  kt2-dashboard/native/koreader/kt2-config.json
  kt2-dashboard/native/koreader/plugins/kt2board.koplugin/...
```

这是独立安装目录，不替换已有 `/mnt/us/koreader`。
若目标 `kt2-dashboard/native` 已存在，先通过旧界面退出，备份旧目录再升级；不要直接混装。

安全弹出、拔掉数据线，让 Kindle 连接与 ESP 相同的家庭 Wi-Fi，改用充电器供电。
在自己的脚本启动器中打开 `Start Native KT2 Board.sh`；等待 KOReader 和看板加载。
如从 SSH 安装，文件权限需要允许执行 KOReader 的程序，见下一节。

## 4. 权限与配置

USB磁盘/文件系统权限表现依设备而异。若运行时报 permission denied，在已有 root shell 下：

```sh
cd /mnt/us/kt2-dashboard/native/koreader
chmod +x luajit koreader.sh fbink
```

配置位置：`/mnt/us/kt2-dashboard/native/koreader/kt2-config.json`。

- `base_url`：ESP 的 HTTP 地址，可用IPv4或可解析主机名，不含尾部斜杠。
- `poll_seconds`：默认5，最小5。
- `test_duration`：0表示无限期运行。
- `enable_test_commands`：默认false，不运行开发期每秒读测试命令文件的循环。

只读配置示例在 `kindle/config.example.json`；日常免密，不需要 access_key。
设置新地址后手动退出、重新启动。

## 5. 验收

1. 手机新增“验收测试”，确认 Kindle 出现。
2. Kindle 点击该行完成，手机刷新确认同步，再点一次取消。
3. 点击首页宜忌文字应无勾选操作；切换黄历和七日天气，检查中文。
4. 看实际屏幕判断残影、黑闪，截图不能替代肉眼验收。
5. 点底部“退出”，确认回到原 Kindle 界面，再用 Start 入口重开。
6. ESP 和 Kindle 均接独立电源后，关闭电脑，再复测手机修改和勾选。

启动期间会调用 KOReader 官方包装器暂时管理系统界面/服务，退出时恢复。
看板通过 preventScreenSaver 保持运行；守卫每30秒检查启动进程存活，不按时限退出。
重启 Kindle 后需要手动重新启动；不支持从真正深度休眠接收手机触发唤醒。

