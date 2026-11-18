# R36S / TreeOS 硬件事实（公开版）

> 2026-11-18 整理。**可直接公开**：不含个人信息、价格、聊天截图、串号、GPS。
> 原则：只写能拿出证据的；未验证的明确标注“未验证/未取得实机”。

---

## 一、R36S 硬件表（RK3326）

| 项目 | 事实 | 证据 | 状态 |
|---|---|---|---|
| SoC | **Rockchip RK3326**，4×Cortex-A35，Mali-G31 MP1 | `dts/rk3326-r36s.dts` compatible；`/proc/cpuinfo` | ✅ 已验证 |
| 内存 | **918 MB 可用**；2× **Micron D9PXV** = MT41K256M16HA-125:E，DDR3L 4Gb 256M×16，BGA96，单颗 512MB，共 1GB | 板照（内部存档，公开需打码）；`/proc/meminfo`；D9PXV 丝印 | ✅ 已验证 |
| PMIC | **RK817**（I2C 0x20）：电池、充电、音频 codec、RTC、电源键 | vendor dtb `pmic@20`；`aplay -l` card `rk817-codec` | ✅ 已验证 |
| 电量计 | RK817 内置；`sample_res = 10mΩ`；`design_capacity = 3380mAh`；`design_qmax = 3718mAh`；`ocv_table` 21 点 | vendor dtb `battery` 节点 | ✅ 已验证（配置值） |
| WiFi | **RTL8188EU**，USB `0bda:8179`，挂内部 hub `1a40:0101` | `lsusb`；`lsusb -t`；vendor dtb | ✅ 已验证 |
| 蓝牙 | **无硬件** | 无 BT 设备；`hciconfig` 无 | ✅ 已验证 |
| 屏幕 | 3.5" **640×480**，MIPI-DSI，ST7701S；厂商初始化序列 151 条 | `dts/rk3326-r36s.dts` panel 节点；`05_系统基线方案.md` §G.2 | ✅ 已验证 |
| 面板时序 | 58MHz 640×480 78Hz；29MHz 640×480 60Hz | vendor dtb `display-timings` | ✅ 已验证 |
| 背光 | `pwm-backlight`，PWM1，周期 62745ns（≈15.9kHz），默认 51/160 | vendor dtb `backlight`；`dts` | ✅ 已验证 |
| 充电 | 原厂 **4056 线性充电**（硬件）；PMIC `dc_det` = GPIO0_B3；PMIC 有 USB_VOL ADC（reg 0x7E/0x7F），驱动采样但未导出 | 板照/跑线（硬件合作方实测）；vendor dtb `charger`；`rk817_battery.c` | 🟡 硬件合作方实测 |
| 输入电压 | SARADC ch0 实测 ≈0.29V，电池 3.71V → 12.8:1，疑似电池电压采样；输入电压更可能走 PMIC USB_VOL | 2026-11-13 实测 `in_voltage0_raw=165`；`rk817_battery.c` | 🟡 待确认 |
| 指示灯 | **单脚双色 LED**（GPIO2_B5），红/蓝反并联；只能选红/蓝，**不能调亮度**；原厂可能无法关 | vendor dtb `arkos4clone-led`；硬件合作方口述 | 🟡 硬件合作方实测 |
| 风扇 | 原厂无风扇；未焊 flash 焊位 4 个空脚：**GPIO32/33/36/41**（GPIO1_A0/A1/A4/B1），可导出、可翻转 | `gpio` debug；export 测试 | ✅ 已验证 |
| 硬件 PWM | RK3326 只有 2 路：**M19=GPIO0_B7/PWM0/OTG_DRV**、**N21=GPIO0_C0/PWM1**；板上 M19 用于 `vcc-host`，N21 用于背光 PWM1 | RK3326 Datasheet V1.4 球位表；vendor dtb `vcc-host`/`backlight` | ✅ 已验证 |
| 电源轨 | 3.3V DCDC + 3.3V LDO；板上有 9 路 LDO + 5 路 DCDC；WiFi 模块用两个 0Ω 电阻选电源 | 硬件合作方实测/口述 | 🟡 硬件合作方实测 |
| OTG/主机 5V | `vcc-host` regulator-fixed，GPIO0_B7 使能，`regulator-always-on` | vendor dtb `vcc-host` | ✅ 已验证 |
| 内部 USB hub | 1 拖 4：WiFi + OTG 已用，剩 2 路 | `lsusb -t`；硬件合作方改造 | ✅ 已验证 |
| 音频 | RK817 codec；外放/耳机自动切换；耳机检测 GPIO2_C6（gpio-86） | `aplay -l`；`gpio` debug；vendor dtb `rk817-sound` | ✅ 已验证 |
| 按键 | `gpio-keys`；摇杆走 SARADC ch1 + GPIO3 复用 | vendor dtb；`dts` | ✅ 已验证 |
| 调试串口 | UART1，GPIO1_C0/C1（48/49），板上 GND/RX/TX/VCC | vendor dtb `serial@ff158000`；`px30.dtsi` | ✅ 已验证 |
| 温度 | 空闲 51–57°C；降频线 70/85/115°C；实测一次 74°C（环境/负载未记录） | `thermal_zone0`；trip points | ✅ 已验证 |
| 电池 | JLCL 804069P 3.7V 3000mAh 单节 | 物料/电芯标签 | 🟡 供应商信息 |

---

## 二、2GB 内存改装

**现状**：2× Micron D9PXV（MT41K256M16HA-125:E），DDR3L 4Gb x16，BGA96，单颗 512MB。

**2GB 方案**：

- 物料：2× **8Gb x16 DDR3L**，例如：
  - Micron **MT41K512M16HA-125**（或同系列）
  - Samsung **K4B8G1646**（或同系列）
- 工艺：BGA96 重植/返修（拆原片、植球、对位、回流）。
- 软件：U-Boot DDR init 要改成 2GB 参数；内核 memory node / reserved-memory 可能要调。
- 风险：
  - BGA 返修虚焊、掉点、板子变形；
  - DDR 时序/训练不过，随机死机；
  - 良率未知，目前**没有实测数据**；
  - 改后不在原厂保修范围。
- 结论：**只作为“用户自行改装 / 可选服务”，不写进默认镜像、不排期、不作为发布门槛或卖点；
  1GB 基线必须完整支持，不能因为没有 2GB 就排除用户。**

---

## 三、RK3566 / RK3562 / H700 设备清单

| SoC | 设备 | 状态 |
|---|---|---|
| RK3566 | — | **已采购，物流中**；到货后补型号与验证，暂不写“已支持” |
| RK3562 | — | **已采购，物流中**；上游只有 SoC dtsi + EVB，无掌机 DTS |
| H700 | — | 无实机；只做研究记录 |

**声明**：在上述设备完成安装、启动、显示、输入、音频、WiFi、休眠/唤醒验证前，**不写“已支持”**。

---

## 四、风扇 UART 方案（计划中，未实现）

- 风扇板加 **8 脚 SO8 单片机**，硬件 PWM；R36S 通过 UART 发目标。
- 我们发：**频率 + PWM 百分比**，约 **300ms** 一次。
- 单片机回：**转速 + 5V 主电源电压**。
- 超时：UART 模式 3 秒没数据 → 0%；开机从没收到过 → 100%（兼容未改软件）。
- **SoC 侧 UART 是固定引脚**：四个空闲脚 32/33/36/41 都不是 UART；候选 UART2 M0（58/59）、UART4（60/61）、UART2 M1（76/78）、UART5（97/98），或 USB 转串口（推荐）。
- 建议失效安全：5V 在、串口断 → 100%；5V 不在 → 0%。
- 状态：**方案已定，等硬件合作方做板，未实现。**

---

## 五、RTL8723DU 适配（WiFi + BT 4.2）

- 模块：USB 2.0，802.11n 1×1 WiFi + BT 4.2。
- **WiFi**：主线 `rtw88_8723du`，USB ID `0bda:d723`；当前 TreeOS 配置 **`CONFIG_RTW88` 未开**，需重编内核，打开 `RTW88` / `RTW88_USB` / `RTW88_8723D` / `RTW88_8723DU`。
- **蓝牙**：主线 `btusb` + `btrtl` 支持 8723D；配置 `BT_RTL=m`、`BT_HCIBTUSB=m`、`BT_HCIBTUSB_RTL=y` 已开；固件 `rtl_bt/rtl8723d_fw/config`。
- **固件压缩**：当前 `CONFIG_FW_LOADER_COMPRESS` 未开，`.zst` 固件加载不了；要么开压缩支持，要么放解压后的 `.bin`。
- **电源**：3.3V DCDC，不走 LDO；两个 0Ω 不要同时贴，避免倒灌。
- **BT 版本**：4.2 向下兼容 4.0/5.x 设备；无 LE Audio/LC3；音频走 BlueZ/PipeWire。
- 状态：**驱动方案已核对，未在实机验证。**

---

## 六、证据索引

| 证据 | 位置 |
|---|---|
| 主线 dts | `dts/rk3326-r36s.dts` |
| TreeOS 内核配置 | `kernel/config-6.12-r36s` |
| 内核源码 | 上游 Linux 6.12（`rtw88`/`btrtl`/`rtl8xxxu`） |
| vendor dtb dump | 内部文件（不公开；引用节点名即可） |
| 主板/内存丝印照片 | 内部存档（公开需打码） |
| 风扇 BGA 球位图 | 内部存档 |
| RK3326 数据手册 | [RK3326 Datasheet V1.4](https://www.rockchip.fr/RK3326%20datasheet%20V1.4.pdf) |
| 实测读数 | 2026-11-13：电池 3.712V、-660mA、68%、SARADC ch0=165、thermal 74°C、`ac/usb online=0` |

---

## 七、公开边界

**可以公开**：本文件的技术事实、dts 节点、照片（打码后）、实测读数。

**不要公开**：
- 人名、群聊截图、供应商价格、进货渠道；
- 板子串号、GPS、个人目录路径；
- 未验证的 2GB 良率、3566/3562 “已支持”；
- 别人的私聊原文（转述可以）。

---

## 相关

- `docs/03_需求与缺陷清单.md`（硬件底子）
- 竞品与定位、风扇 UART 细节、RTL8723DU 适配：内部资料，未随本仓库公开
- 板照/丝印：内部存档，公开版需打码
