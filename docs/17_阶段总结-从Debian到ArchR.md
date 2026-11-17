# 阶段总结：从自研 Debian 到 Arch R 基座（2026-11-16）

> 这不是失败记录，是一次方向修正。把所有代价和判断写清楚，避免下一个人（或下一个我）再走一遍。

## 一、起点

目标从来不是"做一个 Linux 发行版"，而是：

> **在 R36S 掌机上玩 4399 的 Flash 小游戏。**

原系统是 Ubuntu 19.10 + 厂商 4.4 内核（2016），glibc 2.30，Ruffle/新浏览器都装不上。所以决定换底座：Debian 13 + 主线 6.12。

## 二、这一周真正完成的

| 层 | 成果 |
|---|---|
| 内核 | 自编译 6.12，含面板、ADC 摇杆、rk817 声卡/充电、zram、uinput |
| 显示 | 自己写的 DSI 面板驱动（151 条初始化命令从厂商 dtb 提取） |
| 用户态 | Debian 13（debootstrap） |
| GPU | panfrost + Mesa 25，GL ES 3.1 |
| 菜单 | EmulationStation 中文、手柄免配置、主题 |
| 音频 | 外放/耳机/自动切换（DAPM 路由修复） |
| 系统 | 电量显示（内核 + 桥接服务）、WiFi（nmcli 包装）、21 张开机图 |
| 镜像 | 16 GiB，刷空卡可开机进中文菜单 |
| 资产 | 589 个存档备份、42G 去重 ROM 库、17G ports、169 个 Flash 测试游戏 |
| 文档 | 内核踩坑 33 条、缺陷清单 D1–D17、三轮外部评审、竞品分析 |

这些都是真的，**不是白做**。

## 三、卡住的地方

### 1. 输入被拆成两个设备（最致命）

R36S 的摇杆是 `adc-joystick`（只有轴），按键是 `gpio-keys`（只有键）。SDL/RetroArch/ES 都不会合并它们。

我们的对策是用户态虚拟键盘/鼠标桥（padmerge、stickmouse）。结果：
- ES 主菜单能用了，但 WiFi 密码键盘页摇杆左右反向，改源码也无效
- RetroArch 需要键盘映射 hack
- Ruffle 鼠标仍然不动
- 两个桥常驻还会抢输入

### 2. ES 源码改造代价远超预期

自编译后丢失原厂品牌/翻译；改 D16 补丁上机无效；N10 菜单结构、N3 主题预览没做完。根因：ES 的输入/页面路径不统一，靠读源码猜行为效率极低。

### 3. 电量状态不可信

插着充电器 `online=0`、容量跳变。硬件在预充（3.46V 低电压），但系统显示"未充电 7%"。

### 4. 存档曾写空

128KB 存档变成全 0xFF，根因未证明，只做了缓解。

### 5. 迭代成本极高

16 GiB 镜像写入一张 2.6MB/s 的卡要 ~2 小时。改一行、刷一次，时间全耗在刷写上。

## 四、转折点：看别人的源码

查 Arch R 的 R36S 设备树发现：

```
joypad: odroidgo3-joypad {
    compatible = "odroidgo3-joypad";
    io-channel-names = "amux_adc";   /* ADC 摇杆 */
    pinctrl-0 = <&btn_pins>;          /* GPIO 按键 */
};
```

**它在内核驱动层把摇杆和按键合并成一个手柄设备。** 我们折腾两周的用户态桥，在别人那里根本不存在这个问题。

同一份仓库里还有：
- `010-rk817-battery-calibration.patch`（电量校准）
- U-Boot 读 SARADC 自动识别 43 种面板

## 五、GPT-6.1 的复盘结论

> 停止把 TreeOS 当新发行版做，改成成熟基座上的 Flash 游戏方案。
> 你们缺的是目标游戏验证，不是更多底层任务。

它同时纠正了一个错误推论：**合并手柄 ≠ Ruffle 能用**，Flash 游戏多只认键鼠，仍需逐游戏映射。

## 六、方向修正：底座仍是 Debian，只移植 Arch R 的 GPL 驱动

最初写过「内核/输入/面板交给 Arch R」，但核对其 LICENSE 后推翻：
Arch R 的品牌与镜像素材是 **CC BY-NC-SA 4.0（禁止商用）**，不能作为卖出去的机器的底座。
最终方案：

| 层 | 谁来做 |
|---|---|
| 内核 + 面板 + 启动 | **TreeOS（Debian 13 + 主线 6.12）** |
| 输入合并 | **移植 Arch R v3.0.3 的 `odroidgo3-joypad`（GPL-2.0）** 到我们的内核 |
| 中文界面/文档 | TreeOS |
| Ruffle 运行时 | TreeOS |
| 逐游戏键鼠映射 | TreeOS（`/roms/flash_profiles/...`） |
| 主题/开机图/品牌 | TreeOS |
| 产品与售后 | 拙木 |

Arch R 只作**内部对照与 GPL 代码来源**：只取内核驱动/DTS 写法，不打包其脚本、主题、logo、开机图，
对外不宣传为「Arch R 定制版」。

## 七、教训清单

1. **先看别人怎么解决，再决定自己写。** 输入问题在 Arch R 源码里 10 分钟就能看到，我们却自己试了两周。
2. **不要用系统工程掩盖产品目标。** 系统做得再完整，Flash 没跑起来就是没达成。
3. **验证优先于完善。** 两天验证目标游戏，比两周打磨菜单更有价值。
4. **镜像迭代要低成本。** 日常只同步文件，完整镜像低频发布。
5. **诚实记录失败。** 这份文档就是给未来的自己看的。

## 八、已落地的内核改动（2026-11-16 夜）

方向定下后，当天就完成了「Debian + 合并手柄驱动」的第一步，不再等 Arch R 整机：

| 项 | 内容 |
|---|---|
| 驱动 | `archr-004-input-drivers.patch`（GPL-2.0，Arch R v3.0.3）应用到本地 6.12 树，8 个文件，含 `odroidgo3-joypad.c` |
| 兼容移植 | 6.12 已删掉 `input-polldev` 和 `of_gpio_legacy`：改成 `input_setup_polling()` / `input_set_abs_params()` 路线；GPIO 说明符改用 `of_parse_phandle_with_args()` 自读 flags；`open`/`close` 回调签名按新 input 核心改 |
| 配置 | `CONFIG_JOYSTICK_ODROIDGO3=y`（`CONFIG_JOYSTICK_ADC` 保留作回退） |
| DTS | `&builtin_gamepad` 的 compatible 改为 `odroidgo3-joypad`，补 amux 引脚（gpio3 PB3/PB0 选择、PB5 使能）、`saradc 1`、厂商标定参数（scale=2 deadzone=64 fuzz=32 flat=32 tuning=200）、invert-absx/absy；**按键沿用我们厂商 dtb 验证过的引脚和键码**，没有照抄 Arch R 的按键表 |
| 删除 | 旧 `mux-controller` / `io-channel-mux` / `adc-joystick` 三个节点，避免重复上报 |
| 产物 | `Image` 48.4 MB、`rk3326-r36s.dtb` 49 KB，已编译通过，`dtc` 反编译确认手柄节点 26 个 GPIO 子节点齐全 |

### 设备树来源与合规

- Arch R 镜像（v3.0.3）的 boot 分区里直接反编译出 `rk3326-r36s` dtb，作为 amux 引脚与按键表的对照，**只取 GPL-2.0 的内核驱动/DTS 写法**；
- Arch R 的脚本、主题、logo、开机图属 CC BY-NC-SA 4.0，**不碰、不打包、不宣传为「Arch R 定制版」**；
- 驱动文件头保留原版权声明（Hardkernel / ROCKNIX 系），来源与版本记在这里；
- 对外分发内核二进制时，同时公开「Debian 内核源码 + 本仓库 patch 集 + .config + DTS」。

### 上机结果（2026-11-16 23:50，已通过）

| 项 | 结果 |
|---|---|
| 内核 | `#17`，驱动内置，开机自动绑定（`/devices/platform/gamepad`） |
| 设备 | **一个** `r36s_Gamepad`（`event1`），`EV=b`，`ABS=X,Y,RX,RY`，轴范围 **-1800..1800**，fuzz 32 flat 32 deadzone 64 |
| ES | 识别为新设备，映射界面一次性走完，GUID `19003982010000008811000088010000` |
| 键位（SDL 序号） | A=1 B=0 X=2 Y=3；十字键 8/9/10/11；Start=13 Select=12；L1/R1=4/5；L2/R2=6/7；左/右摇杆按下=14/15；FN=16 |
| 左摇杆 | SDL axis0 左右、axis1 上下（上 = -1），ES 里 `leftanalog*` 已映射 |
| 配置留档 | `11_TreeOS/config/es_input.cfg`（含新设备条目） |

上机踩到的两个真问题（已修）：

1. **节点名不能叫 `gpio-keys`**：平台总线在驱动名相同时先按节点名匹配，`gpio-keys`（device_initcall）会抢在我们的驱动（late_initcall）之前绑定。把 dtsi 节点改名为 `gamepad` 后，开机由 compatible 正确匹配到 `odroidgo3_joypad`。
2. **sysfs 属性组没有 devm 托管**：重复 unbind/rebind 会 `-EEXIST` 失败。改成 `devm_device_add_group()`。

### 前端层结果（2026-11-17 00:20，全部通过）

**EmulationStation**：合并手柄直接被识别（日志 `Added known joystick r36s_Gamepad`），
菜单上下、左右、A、Start、音量条全部正常。映射文件在 `11_TreeOS/config/es_input.cfg`。

**RetroArch**：踩到两个与内核无关的坑，均已修：

1. **ArkOS 遗留的 `retroarch.cfg` 是罪魁**：
   - `input_player1_a_btn/b/x/y/start/select/上下左右` 全是 `nul` → 游戏里手柄按键一个都不生效；
   - `input_menu_toggle_btn = "2"` → 按钮 2 正好是 **X**，按 X 会弹菜单（“X 变成 Home 键”）；
   - `input_enable_hotkey_btn = "6"`、`exit_emulator_btn = "7"` 是按另一套手柄写的。
   修法：在 cfg 里按 **udev 序号**显式绑定（0=B 1=A 2=X 3=Y 4/5=L1/R1 6/7=L2/R2
   8=Select 9=Start 10=FN 11/12=L3/R3 13-16=十字键），菜单键改到 FN(10)，
   `analog_dpad_mode=1` 让摇杆也能当十字键。修复后的完整配置留档
   `11_TreeOS/config/retroarch-treeos.cfg`。
2. **GBA 默认核心 `gpsp` 在本机起不来**：ES 日志
   `launch terminated with nonzero exit code 256`，手工跑 `rc=1` 且无任何输出。
   默认核心改成 `mgba`，**必须同时改 `/etc/emulationstation/es_systems.cfg.sd1` 模板**，
   否则 ES 启动时用模板覆盖 `.cfg`。改后的模板留档
   `11_TreeOS/config/es_systems.cfg.sd1`。

实测结果：GBA《CT特种部队2》可进游戏，十字键/摇杆/ABXY/Start/Select/L/R 全部生效，
音量条正常。

### 后续待验证（设备联网后立刻做）

1. `dmesg | grep -i joypad`、`evtest /dev/input/eventX`：确认轴、按键、方向；
2. `sdl2-jstest`：确认 SDL 看到一个完整手柄、GUID；
3. 停用 `treeos-padmerge` / `stickmouse`，看 ES 主菜单能否直接用摇杆和按键；
4. ES 重新生成 `es_input.cfg`，RetroArch 去掉键盘映射 hack；
5. 若左右/上下反了：只调 `invert-absx` / `invert-absy` 或 amux 通道顺序，不再写用户态桥。
