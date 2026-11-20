# Changelog

里程碑只记录**已在真机验证**的能力；未验证项在 `STATUS.md` 的“未验证/实验”。

## 0.1.3（开发中，未发布）

- 开机画面：直接 DRM/KMS splash（首帧约 5.05s）+ 停止时写 fb0/恢复 fbcon 交接 +
  ES `Window::init` early boot splash；测试机冷启确认只剩 modeset 约 0.2s 闪烁
- 0–5 秒 U-Boot/内核仍黑，列 backlog
- 不进入 0.1.2；0.1.3 镜像待重建

## 2026-11-20 — 0.1.2 预览镜像与 Flash 启动器

- `0.1.2` 预览镜像发布：16 GiB，压缩 1,695,814,382 字节；不含 ROM/SWF/BIOS/存档
- SSH 默认关闭，`treeos-ssh on` 提供 10 分钟配对窗口；root 远程登录保持关闭
- Flash 启动器：设置 UI（鼠标左右键、按住 2 秒绑定、一柄多键）、profile 原始名/归一化/大小写/中文解析
- 游戏导入：`/roms/import/` 按扩展名分类，街机 zip 保持原样
- Flash 逐款验证：黄金矿工、死神 vs 火影 2.6、造梦西游、僵尸危机 3
- 不在本节：0.1.3 草案与通用控制 v2 尚未发布，见 `STATUS.md` 的“未验证/实验”

## 2026-11-18 — 6.15 迁移与音频/电量修复

- 内核：6.12 → **6.15.6**（ROCKNIX 输入补丁 + 我们 DTS/面板 + codec 与 rk817 电量驱动 backport）
- 输入：`r36s_Gamepad` 合并手柄在 6.15 下键码/轴与 6.12 一致
- 声音：`card 0 [rockchiprk817co]` 恢复，Master 60/255
- 电量：6.15 驱动回归修复，`capacity` 恢复（66%、充电中）
- Flash：新旧两套运行时在 6.15 下均通过；6.12 保留回滚
- 已知：部分 Flash 游戏（earn-to-die 类）在 Panfrost 下 GPU hang，6.12/6.15 均复现，与内核版本无关

## 2026-11-17 — 合并手柄（内核层）

- 移植 Arch R v3.0.3 的 GPL-2.0 `odroidgo3-joypad` 到 Debian 6.12
- ES / RetroArch 直接识别一个标准手柄；用户态 padmerge/stickmouse 退役
- 修复：节点名不能叫 `gpio-keys`；sysfs 属性改 devm

## 2026-11-15 — 首个可刷写镜像

- 16 GiB 镜像，冷启动约 11 秒进入中文 EmulationStation
