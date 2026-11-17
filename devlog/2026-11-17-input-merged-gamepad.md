# 2026-11-17：一个手柄，终于

前一篇写到，我们读了 Arch R 的设备树，发现摇杆和按键在别人那里是**一个**内核设备。
今天这件事在 TreeOS 上落地了：不是换发行版，而是把那段 GPL-2.0 驱动移植进我们的 Debian 内核。

![R36S 上运行 Flash 游戏](assets/merged-gamepad/001_掌机实拍_goldminer.jpg)

## 做了什么

- 应用 Arch R v3.0.3 的 `004-input-drivers` 补丁（8 个 joypad 驱动，GPL-2.0）
- 为 Linux 6.12 做兼容移植：`input-polldev` 已在主线删除，改成 `input_setup_polling()`；
  `of_gpio_legacy` / `of_get_named_gpio_flags()` 也已删除，改成 `of_parse_phandle_with_args()`
  自己读 flags；input 核心的 `open/close` 回调签名也按新 API 调整
- 设备树：`&builtin_gamepad` 的 compatible 改为 `odroidgo3-joypad`，补 amux 引脚、
  `saradc 1` 和厂商标定参数；**按键沿用我们自己厂商 dtb 验证过的引脚**，没有照抄别人的按键表
- 删掉旧的 `adc-joystick` / `mux-controller` / `io-channel-mux` 节点，避免重复上报

结果：开机就是一个 `r36s_Gamepad`，`ABS_X/Y` 范围 -1800..1800，17 个标准按键。
ES 和 RetroArch 都能直接使用，用户态 `padmerge` 桥正式退役。

```
$ evtest /dev/input/event1
Input device name: "r36s_Gamepad"
  Event code 304 (BTN_SOUTH)   ...  Event code 544 (BTN_DPAD_UP)
  Event code 0 (ABS_X): Min -1800  Max 1800
```

设置界面（按游戏保存键鼠映射）：

![设置界面](assets/merged-gamepad/001_设置ui_bvs26.jpg)

Flash 游戏加载：

![死神 vs 火影加载界面](assets/merged-gamepad/001_bvs_loading.jpg)

## 两个真正花时间的坑

**1. 节点名不能叫 `gpio-keys`。**
改完 compatible 后驱动没有 probe。原因是平台总线在驱动名与节点名相同时会先按名字匹配，
`gpio-keys`（device_initcall）抢在我们驱动（late_initcall）之前绑定了设备。
把节点改名为 `gamepad` 后，才由 compatible 正确匹配。

**2. ArkOS 的 `retroarch.cfg` 是不可见的坑。**
驱动明明正常，游戏里却一个键都没反应，按 X 还会弹菜单。查配置发现：
`input_player1_a_btn/b/x/y/start/select/上下左右` 全是 `nul`，而
`input_menu_toggle_btn = "2"`——按钮 2 正好是 X。按 udev 序号显式绑定后才全部正常。

## Flash 侧进展

《死神 vs 火影》已在真机走完一条完整路径：进入角色选择 → 开始对战，
血条、连击和命中反馈正常（120 秒实拍视频为证）。这里只记录「能进对战并操作」，
不承诺长时间稳定或所有游戏都能玩。

![死神 vs 火影对战](assets/merged-gamepad/005_bvs对战_掌机.png)

逐游戏 profile 与鼠标点击仍在验证。
这篇只记录输入：它从「两个设备 + 用户态桥」变成了「一个标准手柄」，
后面所有前端和模拟器都不用再为此写补丁。
