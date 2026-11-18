# TreeOS Status

最后验证：2026-11-18（R36S 真机）
状态只描述**所列设备与版本**；未列出的设备/能力一律视为未验证。

## 支持分档

| 档位 | 说明 |
|---|---|
| **基线（1GB）** | 原机内存，不要求改装；冻结 Ruffle 核心 + 逐款验证的 Flash 2.6 |
| **可选（2GB）** | 用户自行改装；不排期、不作为发布门槛或卖点；新运行时（Weston + wgpu）+ 逐款验证的 Flash 3.2 |
| 实验 | 未过验收门槛，仅在 labs 记录 |
| 不支持 | 明确失败或无实现 |

> 2GB 不是使用 TreeOS 的前提；不改机的 1GB 设备继续受支持。

## 硬件事实

- R36S 硬件表、2GB 改装、风扇 UART、RTL8723DU 适配：`docs/hardware/R36S-hardware.md`

## 已验证（RK3326 / R36S，1GB）

- Debian 13 + 主线 Linux 6.12，16GB 可刷写镜像，冷启动约 11 秒进入中文 EmulationStation
- 输入：内核 `odroidgo3-joypad`（GPL-2.0），`r36s_Gamepad` 单设备，ES / RetroArch 直接可用
- 音频：外放 / 耳机 / 自动切换
- Flash：Ruffle native（DRM 直出）；20 个样本启动存活 20 秒；《死神 vs 火影 2.6》进入对战并可操作
- 存储：exFAT 游戏分区；zram 默认（Flash 3.x 需特殊 swap 配置，见 docs/18）

## 未验证 / 实验

- 6.15 候选内核（v5）：6.15.6 + ROCKNIX 输入驱动 + 我们 DTS/面板 + codec 补丁 + 6.12 rk817 电量驱动 backport；
  启动、面板、WiFi、合并手柄、声卡、电量（66%、充电中）已实测；尚未设为默认
- 已知：部分 Flash 游戏（如 earn-to-die）在 Panfrost 下 GPU hang，6.12/6.15 均复现，与内核版本无关
- 新 Flash 运行时（Weston 14 kiosk + wgpu，fontconfig）：RK3326 1GB 上 wgpu/中文/右摇杆光标可用；
  **鼠标左键尚未到达 Flash 内容**（只能点开 Ruffle 自身菜单）；3.2 在 1GB + Weston 下会 OOM/重启，
  不作为 1GB 的日常目标；FN 联动、存档、30 分钟压测未验收
- ROCKNIX 补丁栈替换后的内核等价性（迁移中）
- RK3566 / RK3562：已采购、物流中；到货前不写“已支持”
- H700：研究记录，无实机
- 新运行时（Weston + wgpu）的 FN、退出、存档、30 分钟稳定性
- Flash 长时间稳定性、完整通关率、跨设备表现
- ports 兼容层（port-runner）
- 开发终端 / 中文输入 / coding agent / 本地小模型

## 不承诺

- 「所有 Flash 游戏都能玩」；只承诺逐款验证清单中的版本
- Xbox / PS2 大作在 RK3326 上可玩；x86 转译 / Wine / DXVK
- 未列出的 SoC 或设备
