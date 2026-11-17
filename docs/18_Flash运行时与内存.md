# Flash 运行时与内存：Ruffle 在 1GB 掌机上的实测

更新：2026-11-18。记录运行时结构、逐游戏映射，以及「为什么 BvS 3.2 必须换掉 zram」。

## 运行时结构

```
EmulationStation → /roms/ports/rufflehandheld/runtime/launch.sh
  ├─ 读取 /roms/flash_profiles/default.profile
  ├─ 按 SWF 文件名找 /roms/flash_profiles/custom/<name>.profile
  ├─ 启动 Ruffle native（DRM/KMS 直出，无 X）
  └─ gptokeyb2 注入鼠标/键盘映射（dpad=mouse_movement, a=mouse_left）
```

- 游戏文件按文件名归一化匹配 profile：小写、非字母数字转 `-`；纯中文名会被清空，建议用英文/数字文件名。
- 查找顺序：`custom/<name>.profile` → `<name>.profile` → `aliases=` 匹配 → `default.profile`。
- 每款游戏的左摇杆模式（鼠标 / WASD / 方向键）单独保存，由守护进程通过 uinput 翻译。

## 输入结论

- 内核层合并手柄（`odroidgo3-joypad`）后，Ruffle 只看到一个标准手柄；
- SDL 的按钮事件在这台机器的冻结核心上收不到，所以点击走 gptokeyb2 注入：
  `dpad = mouse_movement` + `a = mouse_left`；
- 代价是屏幕可能出现两个指针（Ruffle 自身光标 + 注入光标），不影响操作，属已知视觉问题。

## 内存：四轮实测

| 配置 | 峰值 | 结果 |
|---|---|---|
| 原厂 zram 478 MB | RSS 426 MB | OOM 被杀 |
| exFAT 上 1.5 GB swap | RSS 492 MB + swap 431 MB | OOM（exFAT 上 swap 实际没写入） |
| ext4 2 GB swap + zram | RSS 535 MB + swap 432 MB | OOM（zram 先写满） |
| **关 zram + ext4 2 GB swap + swappiness=100** | RSS 170 MB + swap 375 MB | ✅ 进入对战 |

结论：不是容量不够，是**回收速度**不够。A35 的 zram 压缩速度跟不上游戏要内存的速度。

当前采用的配置（已持久化）：

| 项 | 值 |
|---|---|
| `/swapfile` | 2 GB，ext4 根分区，写进 `/etc/fstab` |
| `vm.swappiness` | 100 |
| zram | 关闭 |
| WiFi 省电 | 关闭（重启后需重新设置，尚未做成服务） |

代价：切场景会读 SD 卡 swap，比纯内存慢；SD 卡有额外写入损耗。只玩较小的 2.x 版本时可以仍用 zram。

## 现状与边界

- 20 个测试 SWF：20/20 启动并存活 20 秒，无 OOM、无内核崩溃；
- 《死神 vs 火影 2.6》已实测进入角色选择并对战，血条/连击/命中反馈正常；
- 3.2 版本需要上述 swap 配置才能进游戏；
- 未验证：长时间稳定性、存档跨重启、全部 25 个样本的可玩性。

## 合规

本仓库不分发任何 SWF、音频、字体或汉化资源；运行时与映射工具开源，游戏内容由用户自备并自行确认授权。
