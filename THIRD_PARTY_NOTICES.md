# 第三方代码与版权声明

TreeOS 仓库里包含来自第三方的代码。这里记录来源、许可证与版权头，便于再分发时合规。

## 1. odroidgo3-joypad 驱动（GPL-2.0）

- **文件**：`kernel/patches/0004-input-drivers-archr.patch`（原始补丁，含 8 个 joypad 驱动）、
  `kernel/patches/0005-odroidgo3-joypad-6.12-compat.patch`（我们为 Linux 6.12 做的兼容移植）
- **来源**：Arch R v3.0.3（Arch Linux ARM 的 R36S 系统）的 kernel patch 集，补丁编号 `004`
- **许可证**：GPL-2.0（补丁内保留原始版权头）
- **原始版权**：Hardkernel Co., Ltd. 及 ROCKNIX / Arch R 的贡献者；`odroidgo3-joypad.c` 文件头
  为 GPL-2.0 声明
- **我们改了什么**：适配主线 6.12 已删除的 `input-polldev`（改为 `input_setup_polling()`）、
  `of_gpio_legacy` / `of_get_named_gpio_flags()`（改为 `of_parse_phandle_with_args()` 自读 flags）、
  input 核心 `open/close` 回调签名、`sysfs_create_group` → `devm_device_add_group()`

> Arch R 的品牌、主题、logo、开机图和镜像素材是 **CC BY-NC-SA 4.0（禁止商用）**，
> 本项目**不使用、不打包、不再分发**这些素材，也不以「Arch R 定制版」名义对外宣传。
> 我们只取用了 GPL-2.0 的内核驱动/DTS 写法。

## 2. Linux 内核与设备树（GPL-2.0）

- `dts/rk3326-r36s.dts`、`dts/panel-r36s.c`、`kernel/patches/` 均派生自 Linux 内核
  （GPL-2.0 WITH Linux-syscall-note）。
- 基线：主线 Linux v6.12 `arch/arm64/boot/dts/rockchip/` 与 Debian 内核配置。

## 3. 用户态运行时（不在本仓库内分发）

- **Ruffle**（Flash 运行时）为 MIT / Apache-2.0，二进制与源码不随本仓库分发；
  使用方式见 `docs/` 与发布说明。
- EmulationStation、RetroArch 及 libretro 核心由各自的发行版包管理，遵循其原始许可证。

## 4. 游戏资源

本仓库**不包含**任何 ROM、BIOS、SWF 游戏文件、音频、字体或汉化包。
相关资源的获取与导入由用户自行负责，且必须遵守其原始授权。

## 5. stb_image / stb_truetype（public domain / MIT）

- **文件**：`tools/launcher/stb_image.h`、`tools/launcher/stb_truetype.h`
- **来源**：nothings/stb（Sean Barrett / RAD Game Tools）
- **许可证**：public domain，或 MIT（双许可，文件头保留原始声明）
- **用途**：设置界面的 PNG 图标解码与 TrueType 中文渲染

## 6. 设置界面图标（CC0）

- **文件**：`tools/launcher/icons/*.png`（ABXY、方向键、鼠标左右键）
- **来源**：Kenney *Input Prompts* 图标包（https://kenney.nl/assets/input-prompts）
- **许可证**：CC0 1.0（公共领域），原始许可文本见 `tools/launcher/icons/LICENSE.txt`
- **我们改了什么**：仅按实机配色对白色 ABXY 图标做运行时染色，未改动素材文件
