# TreeOS Status

最后验证：2026-11-20（R36S 真机；0.1.2 镜像的实卡冷启仍未完成）
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

## 当前发布（2026-11-20）

- **0.1.2 预览镜像**：16 GiB 原始镜像，压缩约 1.7 GB（1,695,814,382 字节），不含 ROM/SWF/BIOS/存档。
- SSH 默认关闭；`sudo treeos-ssh on` 打开 10 分钟配对窗口（随机密码）。root 远程登录关闭，
  用 `ssh tree@<ip>` 登录后 `sudo -i` 得 root。
- **0.1.3 仍是 Draft，未发布。** 下文标“0.1.3 草案”的能力不属于 0.1.2。

## 已验证（RK3326 / R36S，1GB）

- Debian 13 + 主线 Linux 6.15.6，16GB 可刷写镜像，冷启动约 11 秒进入中文 EmulationStation
- 输入：内核 `odroidgo3-joypad`（GPL-2.0），`r36s_Gamepad` 单设备，ES / RetroArch 直接可用
- 音频：外放 / 耳机 / 自动切换
- Flash：增强（Weston + Ruffle desktop）与经典（冻结 Ruffle 核心 + gptokeyb2）两条运行时；
  《黄金矿工》《死神 vs 火影 2.6》《造梦西游》《僵尸危机 3》已逐款验证可玩
- Flash 设置 UI：鼠标左右键、按住 2 秒绑定、一柄多键、profile 解析支持原始名/归一化/大小写/中文
- 游戏导入：`/roms/import/` 按扩展名分类，街机 zip 保持原样，开机前自动导入
- 存储：exFAT 游戏分区；zram 默认（Flash 3.x 需特殊 swap 配置，见 docs/18）
- **内核 6.15.6（v5，已部署）**：ROCKNIX 输入补丁 + 我们 DTS/面板 + codec 与 rk817 电量驱动 backport；
  面板、合并手柄、声卡（card 0 rk817）、电量（66%、充电）、WiFi 实测通过；Flash A/B 两条运行时均通过；
  6.12 保留为回滚备份（`/roms/backup/kernel-6.12-*`）

## 未验证 / 实验

- 已知：部分 Flash 游戏（如 earn-to-die）在 Panfrost 下 GPU hang，6.12/6.15 均复现，与内核版本无关
- 新 Flash 运行时（Weston 14 kiosk + wgpu，fontconfig）：RK3326 1GB 上 wgpu/中文/右摇杆光标可用；
  **鼠标左键尚未到达 Flash 内容**（只能点开 Ruffle 自身菜单）；3.2 在 1GB + Weston 下会 OOM/重启，
  不作为 1GB 的日常目标；FN 联动、存档、30 分钟压测未验收
- ROCKNIX 补丁栈替换后的内核等价性（迁移中）
- RK3566 / RK3562：已采购、物流中；到货前不写“已支持”
- H700：研究记录，无实机
- 新运行时（Weston + wgpu）的 FN、退出、存档、30 分钟稳定性
- Flash 长时间稳定性、完整通关率、跨设备表现
- **通用控制 v2**（`flash-template` → `session.bindings.json` → `padinput` / `flash-emit-gptk`）：
  源码已实现；经典后端在《黄金矿工》验证，增强后端绑定表正确但键盘事件尚未送达 Ruffle 内容。
  未进入 0.1.2，也未提交到本仓库
- **开机画面（0.1.3 草案）**：直接 DRM/KMS + fb0 交接 + ES early boot splash 已在测试机冷启验证；
  首帧 5.05s，交接只剩 modeset 约 0.2s 闪烁；0–5 秒 U-Boot/内核仍黑。未进入 0.1.2
- **0.1.3 草案**（假鼠标清理、ES 输入配置对齐、WiFi `nmcli`、SSH 默认 `tree/tree`、日志导出、
  开机画面、通用控制）：未发布；默认口令只适用于可信局域网
- **AURKNIX `sdl2test-rocknix` v0.9**：只完成二进制与启动链调研，未在 TreeOS 上集成或验收
- ports 兼容层（port-runner）
- 开发终端 / 中文输入 / coding agent / 本地小模型

## 已知问题

- 部分 Flash 游戏（earn-to-die 类）在 Panfrost 下 `gpu sched timeout` 黑屏；6.12/6.15 均复现，低画质仅降低概率
- 新运行时可能出现双光标（Weston/Ruffle 光标层），不影响操作；隐藏系统光标会让游戏无指针，未解
- 《死神 vs 火影 3.2》在 1GB 上换页卡顿，属 2GB 可选档，不作 1GB 承诺
- 0.1.2 发布镜像尚未做作者侧实卡冷启回归；热重启在 systemd 结束后可能停在 U-Boot，需要断电重开
- 0.1.2 仍有经典 Flash 退出后 gptokeyb2 残留假鼠标、部分机器每次开机弹手柄配置；
  修复在未发布的 0.1.3 草案中

## 不承诺

- 「所有 Flash 游戏都能玩」；只承诺逐款验证清单中的版本
- Xbox / PS2 大作在 RK3326 上可玩；x86 转译 / Wine / DXVK
- 未列出的 SoC 或设备
