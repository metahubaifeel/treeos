# 输入映射备忘（R36S 合并手柄）

内核驱动：`odroidgo3-joypad`（见 `kernel/patches/`），设备名 **`r36s_Gamepad`**。

## 设备能力

- 轴：`ABS_X` / `ABS_Y`（左摇杆），`ABS_RX` / `ABS_RY`（未接），范围 **-1800..1800**，
  fuzz 32，flat 32，deadzone 64
- 按键（evdev 标准码）：

| 物理键 | evdev | 码值 |
|---|---|---|
| B | `BTN_SOUTH` | 304 |
| A | `BTN_EAST` | 305 |
| X | `BTN_NORTH` | 307 |
| Y | `BTN_WEST` | 308 |
| L1 / R1 | `BTN_TL` / `BTN_TR` | 310 / 311 |
| L2 / R2 | `BTN_TL2` / `BTN_TR2` | 312 / 313 |
| Select / Start | `BTN_SELECT` / `BTN_START` | 314 / 315 |
| FN | `BTN_MODE` | 316 |
| 左/右摇杆按下 | `BTN_THUMBL` / `BTN_THUMBR` | 317 / 318 |
| 十字键 | `BTN_DPAD_*` | 544–547 |

> 早期厂商 dtb 用的是 `BTN_TRIGGER_HAPPY1..5`（704–708），SDL/RetroArch 不认这套码，
> 已改为标准码；换码后 ES 的按钮序号会变，需要重新映射。

## EmulationStation

- 配置文件：`config/es_input.cfg`（设备 GUID `19003982010000008811000088010000`）
- ES 使用 SDL 的**摇杆按钮序号**（按 evdev 码升序，0=B、1=A、2=X……13=上……16=右）

## RetroArch（`input_joypad_driver = "udev"`）

- udev 驱动的按钮序号与上表顺序一致：0=B、1=A、2=X、3=Y、4/5=L1/R1、6/7=L2/R2、
  8=Select、9=Start、10=FN、11/12=L3/R3、13=上、14=下、15=左、16=右
- 我们在 `retroarch.cfg` 里显式绑定 `input_player1_*_btn`，菜单键绑到 FN(10)，
  `input_player1_analog_dpad_mode = 1`
- **注意**：ArkOS 自带的 `retroarch.cfg` 把手柄按键全绑成 `nul`，并把
  `input_menu_toggle_btn` 绑在 `2`（正好是 X），会导致「游戏里手柄没反应、按 X 弹菜单」。

## 其他坑

- 设备树节点名**不能叫 `gpio-keys`**：平台总线在驱动名相同时先按节点名匹配，
  `gpio-keys`（device_initcall）会抢在 `odroidgo3-joypad`（late_initcall）之前绑定。
  节点改名为 `gamepad` 后由 compatible 正确匹配。
- 驱动的 sysfs 属性组必须用 `devm_device_add_group()`，否则重复 unbind/rebind 会 `-EEXIST`。
- GBA 默认核心改 `mgba` 时，**必须改 `/etc/emulationstation/es_systems.cfg.sd1` 模板**，
  改 `.cfg` 会被 ES 启动时覆盖。
