# Changelog

里程碑只记录**已在真机验证**的能力；未验证项在 `STATUS.md` 的“未验证/实验”。

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
