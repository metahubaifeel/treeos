# RTL8723DU 适配笔记（WiFi + BT 4.2）

> 2026-11-18。来源：群里讨论 + TreeOS 内核源码/配置核对。
> 结论：**蓝牙主线已支持；WiFi 驱动没开，要重编内核；固件压缩也没开。**
> **版本说明**：下面的配置核对基于 `kernel/config-6.12-r36s`；TreeOS 当前主线已到 **6.15.6**，6.12 保留回滚。6.15.6 的 `RTW88` / `FW_LOADER_COMPRESS` 配置要重新核对一遍。

---

## 一、这颗模块是什么

- **RTL8723DU**：USB 2.0 接口，802.11n 1×1 WiFi + **BT 4.2** 二合一。
- RTL8723BU 是 BT 4.0；DU 是 4.2（群里“一叶知秋”说的对）。
- USB ID（主线驱动）：
  - WiFi：`0bda:d723` → `rtw88_8723du`
  - 蓝牙：Realtek 接口，`btusb` + `btrtl` 识别为 8723D

---

## 二、驱动现状（TreeOS 6.12，已核对 `kernel/config-6.12-r36s`）

### 蓝牙：已支持 ✅

```
CONFIG_BT_RTL=m
CONFIG_BT_HCIBTUSB=m
CONFIG_BT_HCIBTUSB_RTL=y
```

- 主线 `btrtl.c` 有 `CHIP_ID_8723D`，固件：
  - `rtl_bt/rtl8723d_fw.bin`
  - `rtl_bt/rtl8723d_config.bin`
- 需要确认镜像里带了 `btusb.ko` / `btrtl.ko` 和固件。

### WiFi：没开 ❌

```
# CONFIG_RTW88 is not set
```

- 主线有 `rtw88_8723du`（`drivers/net/wireless/realtek/rtw88/rtw8723du.c`，USB ID `0bda:d723`）。
- 但当前内核 **`CONFIG_RTW88` 没开**，所以现在插上去 WiFi 不会工作。
- 要重编内核，打开：
  ```
  CONFIG_RTW88=m
  CONFIG_RTW88_USB=m
  CONFIG_RTW88_8723D=m
  CONFIG_RTW88_8723DU=m
  ```
  （`RTW88_USB` / `RTW88_8723D` 会被 `select`，但保险起见显式写上。）

### 固件压缩：没开 ❌

```
# CONFIG_FW_LOADER_COMPRESS is not set
```

- 系统里的固件是 `.zst`：
  - `/lib/firmware/rtw88/rtw8723d_fw.bin.zst`
  - `/lib/firmware/rtl_bt/rtl8723d_fw.bin.zst`
  - `/lib/firmware/rtl_bt/rtl8723d_config.bin.zst`
- 内核没开压缩支持，**加载不了**。
- 两个办法：
  1. 内核打开 `CONFIG_FW_LOADER_COMPRESS=y` + `CONFIG_FW_LOADER_COMPRESS_ZSTD=y`；
  2. 或把这三个 `.zst` 解压成 `.bin` 放进镜像（`zstd -d`）。

---

## 三、电源（群里 cao 佬说的重点）

- 模块供电有两路，用 **0Ω 电阻**选择：
  - 一路是 **LDO 3.3V**；
  - 一路是 **3.3V DCDC**。
- **大功率用 DCDC，不要用 LDO。**
- 两个 0Ω **不要同时贴**，否则两路电源并联，可能倒灌 LDO。
- 确认模块峰值电流（WiFi TX + BT 同时工作）和 DCDC 余量；R36S 的 3.3V DCDC 还要带屏、WiFi、音频，别把它压垮。
- 测：`OTG + WiFi + BT 同时工作`，看会不会掉电/重启/OTG 掉线。

---

## 四、USB / 天线 / 共存

- 模块是 USB；R36S 内置 hub 一拖四，已用 WiFi（8188EU）+ OTG，还剩两路。
- 8723DU 的 WiFi 和 BT 可能是**同一个 USB 设备的不同接口**；`lsusb` 看是一设备还是两设备。
- 天线：2.4GHz，WiFi/BT 共存；确认模块是 IPEX 还是 PCB 天线，别把天线贴在 SoC/PMIC 热源上。
- BT 音频和 WiFi 抢 2.4GHz；同时用会互相影响，属正常。

---

## 五、BT 4.2 能连什么

- **向下兼容**：BT 4.2 能连 BT 4.0/5.x 的手柄、耳机；设备会回落到 4.2。
- **没有 LE Audio / LC3**：BT 5.x 的新音频特性用不了。
- 音频走 **BlueZ + PipeWire/PulseAudio**，SBC/AAC 看 CPU；1GB RK3326 跑 SBC 没问题，AAC/aptX 要实测。
- 高版本手柄（Xbox/PS5）能不能连，取决于 BlueZ 的 HID 支持和手柄协议，不只看 BT 版本。

---

## 六、测试清单

```sh
# 1. 看设备
lsusb                     # 找 0bda:d723（WiFi）和 Realtek BT
dmesg | grep -iE "rtw88|btusb|btrtl|firmware"

# 2. 驱动/固件
modprobe rtw88_8723du
modprobe btusb
rfkill list
ip link

# 3. WiFi
nmcli dev wifi list
# 或 iw dev wlan0 scan

# 4. 蓝牙
bluetoothctl
  power on
  scan on
  devices
  pair <MAC>
  connect <MAC>

# 5. 压力
# OTG 插 U 盘 + WiFi 下载 + BT 耳机播放，同时跑 10 分钟
```

---

## 七、给群里的回复草稿

```text
8723DU 这颗我查了：

- 蓝牙：主线 6.12 的 btusb + btrtl 已经支持 8723D，固件 rtl8723d_fw/config 也在，配置里 BT_RTL 已开，不用自己写驱动。
- WiFi：主线有 rtw88_8723du，但我们现在内核 CONFIG_RTW88 没开，要重编内核，把 RTW88 / RTW88_USB / RTW88_8723D / RTW88_8723DU 打开。
- 固件是 .zst，内核现在没开 FW_LOADER_COMPRESS，要么开压缩支持，要么放解压后的固件。
- 供电按你说的走 3.3V DCDC，不要 LDO；两个 0Ω 不要同时贴。
- BT 4.2 连现在的 5.x 手柄/耳机没问题（向下兼容），但没有 LE Audio/LC3；音频走 BlueZ/PipeWire，SBC/AAC 看 CPU。

我先按这个改内核配置，模块到了直接测。
```

---

## 相关

- `kernel/config-6.12-r36s`
- `06_主线内核点亮记录.md`
- `07_硬件改造软件方案.md`
