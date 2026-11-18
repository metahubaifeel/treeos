# TreeOS Status

最后验证：2026-11-18（R36S 真机）
状态只描述**所列设备与版本**；未列出的设备/能力一律视为未验证。

## 支持分档

| 档位 | 说明 |
|---|---|
| **基线（1GB）** | 原机内存，不要求改装；冻结 Ruffle 核心 + 逐款验证的 Flash 2.6 |
| **可选（2GB）** | 用户自行改装；新运行时（Weston + wgpu）+ 逐款验证的 Flash 3.2 |
| 实验 | 未过验收门槛，仅在 labs 记录 |
| 不支持 | 明确失败或无实现 |

> 2GB 不是使用 TreeOS 的前提；不改机的 1GB 设备继续受支持。

## 已验证（RK3326 / R36S，1GB）

- Debian 13 + 主线 Linux 6.12，16GB 可刷写镜像，冷启动约 11 秒进入中文 EmulationStation
- 输入：内核 `odroidgo3-joypad`（GPL-2.0），`r36s_Gamepad` 单设备，ES / RetroArch 直接可用
- 音频：外放 / 耳机 / 自动切换
- Flash：Ruffle native（DRM 直出）；20 个样本启动存活 20 秒；《死神 vs 火影 2.6》进入对战并可操作
- 存储：exFAT 游戏分区；zram 默认（Flash 3.x 需特殊 swap 配置，见 docs/18）

## 未验证 / 实验

- ROCKNIX 补丁栈替换后的内核等价性（迁移中）
- RK3566 / H700 / RK3562 任一实机
- 新运行时（Weston + wgpu）的 FN、退出、存档、30 分钟稳定性
- Flash 长时间稳定性、完整通关率、跨设备表现
- ports 兼容层（port-runner）
- 开发终端 / 中文输入 / coding agent / 本地小模型

## 不承诺

- 「所有 Flash 游戏都能玩」；只承诺逐款验证清单中的版本
- Xbox / PS2 大作在 RK3326 上可玩；x86 转译 / Wine / DXVK
- 未列出的 SoC 或设备
