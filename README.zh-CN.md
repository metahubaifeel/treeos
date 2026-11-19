# TreeOS

**给 R36S 掌机的主线 Linux 系统。** Linux 6.15.6（已上机，6.12 可回滚）+ Debian 13 + 开源 panfrost GPU 驱动 + 自己写的 DSI 面板驱动——目标是让这台机器能跟着上游走，而不是被锁在 2016 年的厂商内核上。

[English](README.md) | 简体中文

![运行 TreeOS 的 R36S](assets/r36s-black-front-display.webp)

## 为什么做这个

R36S 出厂是 Linux 4.4 厂商内核（2016 年）+ Ubuntu 19.10，2020 年就停止安全更新。它的 glibc 卡在 2.30，Ruffle、浏览器、现代工具链全都装不上。硬件没问题，是地基老了。

TreeOS 换掉地基：

| 层 | 厂商系统 | TreeOS |
|---|---|---|
| 内核 | 4.4.189（2016 BSP） | **主线 6.15.6**（6.12 可回滚） |
| 用户态 | Ubuntu 19.10，glibc 2.30 | **Debian 13** |
| GPU | 闭源 Mali blob | **panfrost / Mesa 25**（GL ES 3.1） |
| 显示 | 厂商面板驱动 | **自己写的 DSI 面板驱动** |
| 前端 | EmulationStation | EmulationStation（保留） |
| 模拟器 | 厂商 RetroArch（依赖 RGA） | **Debian 官方 RetroArch 1.20** |

## 先行版镜像（0.1.1）

**[下载 TreeOS 0.1.1（R36S）](https://github.com/metahubaifeel/treeos/releases/tag/v0.1.1)**
— 16 GiB 镜像，压缩约 1.6 GB；仅 R36S（RK3326，1GB）。

- **预览版：尚未完成作者侧实卡冷启回归**
- 不含任何 ROM/SWF/BIOS/存档；游戏放 `EASYROMS/import/` 或对应平台目录
- **安全**：SSH 默认关闭、禁止 root 登录、root 已锁定；系统用户 `tree`（ES 自动登录）。
  如需 SSH：先 `passwd tree` 设强密码，再 `sudo systemctl enable --now ssh`；
  不要把未改密的机器暴露在公共网络。详见 Release 的 `SECURITY.txt`

## 已验证（真机）

- 开机进入中文 EmulationStation 菜单
- 声音：外放、耳机、自动切换
- **输入：内核层合并手柄**——移植 Arch R v3.0.3 的 `odroidgo3-joypad`（GPL-2.0），
  ADC 摇杆 + GPIO 按键合并成一个 `r36s_Gamepad`（ABS ±1800、17 个标准键），
  ES / RetroArch 直接可用，不再需要用户态 `padmerge` 桥
- RetroArch 1.20：GBA 默认 `mgba`（本机 `gpsp` 核心启动即退出，已记录）
- Flash：增强（Weston + Ruffle desktop）与经典（冻结核心）两条线；
  黄金矿工、BvS 2.6、造梦西游、僵尸危机 3 已实测；
  新设置界面支持鼠标左右键、按住 2 秒绑定、一柄多键（如 `south=74,32`）
- 游戏导入器：`/roms/import/` 开机前按扩展名归档；街机 zip 保持原样
- zram、WiFi、21 张开机轮播图
- 0.1.1 镜像已发布；实卡冷启回归待补

尚未验证：0.1.1 的实卡冷启、N64/PSP 性能、反复开关机后的存档可靠性、
RK3566/RK3562 板型、OTA、Flash 全量可玩性（逐游戏验证仍在进行）。

## 实拍与视频

- 《死神 vs 火影》掌机实玩（114 秒，已裁掉片尾水印）：
  [assets/video/treeos-bvs-handheld.mp4](assets/video/treeos-bvs-handheld.mp4)
- 图片：[assets/merged-gamepad/](assets/merged-gamepad/)

- Flash 运行时与内存实测：`docs/18_Flash运行时与内存.md`
- Ruffle 重编译（暂停/菜单）：`docs/19_Ruffle重编译项目.md`
- 多 SoC 与 ports 兼容评审：`docs/08_外部评审-OldBits与多SoC-gpt6luna.md`

- Flash 工程总览与边界：`docs/20_Flash工程总览.md`
- 经典页游离线分类：`docs/21_经典页游分类.md`
- 工具与映射示例：`tools/`

- 硬件事实（R36S / 2GB 改装 / 风扇 / WiFi 模块 / 设备清单）：`docs/hardware/`

## 仓库结构

```
docs/     设计文档、踩坑记录、路线图（中文）
kernel/   内核配置、补丁（含 Arch R GPL 驱动移植）、构建脚本、校验值
dts/      设备树 + DSI 面板驱动
scripts/  镜像构建、备份、刷机脚本
config/   ES/RetroArch 输入映射与系统配置片段
tools/    启动器、设置 UI、游戏导入器源码（tools/launcher）
devlog/   按日期记录的开发日志
assets/   真机实拍图
```

## 构建

内核在 Linux 主机上交叉编译：

```bash
# kernel/config-6.12-r36s 是 6.12 基线；当前真机为 6.15.6 v5
# 工具链布局见 kernel/build.sh
export TREEOS_ROOT=$PWD
SRC=path/to/linux-6.15.6 bash kernel/build.sh
```

补丁应用顺序：`kernel/patches/0001-*` → `0004-input-drivers-archr.patch` →
`0005-odroidgo3-joypad-6.12-compat.patch`。

镜像从运行中的卡的 rootfs 组装：

```bash
sudo bash scripts/build-image-v3.sh
```

备份与恢复流程见 `docs/12_备份与恢复.md`。

## 踩过的坑（都有记录）

- **所有核心都黑屏**：厂商 RetroArch 链接 `librga.so.2`，用 Rockchip 私有 2D 加速器拷帧；主线没有 RGA 驱动。换的是模拟器，不是核心。
- **两个设备，一个玩家**：摇杆（`adc-joystick`）和按键（`gpio-keys`）原本是两个独立输入设备，SDL/RetroArch 不会合并。先用用户态 `padmerge.py` 桥接，最后在**内核层**移植 Arch R 的 `odroidgo3-joypad`（GPL-2.0）彻底解决，用户态桥已退役。
- **存档被写成空数据**：128 KiB 存档变成全 `0xFF`。根因未证明，缓解措施和完整记录在 `docs/11_2026-11-15_踩坑总表.md`。
- **`olddefconfig` 不带 `ARCH`**：会静默重写成 x86 配置，编出 25 MB 镜像，刷进去黑屏。

完整清单（含证据和命令）见 `docs/11_2026-11-15_踩坑总表.md`。

## 许可

- 内核改动、设备树和移植的 `odroidgo3-joypad` 驱动：**GPL-2.0**
- 用户态脚本和文档：**CC BY-SA 4.0**（另有说明除外）
- 面板驱动（`dts/panel-r36s.c`）：GPL-2.0
- 第三方来源与版权头：见 **THIRD_PARTY_NOTICES.md**

分发内核二进制时，对应源码 = 上游 Linux 6.12 + `kernel/patches/` 全部补丁 +
`kernel/config-6.12-r36s` + `dts/`；构建方法见 `kernel/build.sh`。

本仓库不包含任何 ROM、BIOS、SWF 游戏资源或受版权保护的素材。

## 链接

- 商店与硬件笔记：<https://r36s.space>
- 开发日志：<https://r36s.space/devlog>
- 项目主页：<https://treeos.tech>（即将上线）
