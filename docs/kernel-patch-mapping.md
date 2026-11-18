# 内核补丁映射：TreeOS 自有补丁 vs ROCKNIX 补丁栈

更新：2026-11-18。目的：在迁移前逐条标清「上游已覆盖 / 仍需本地维护 / 未知」。
来源：`ROCKNIX/distribution` → `projects/ROCKNIX/devices/RK3326/patches/linux/`。

## 一、映射表

| TreeOS 现有 | ROCKNIX 对应 | 结论 |
|---|---|---|
| `kernel/patches/0004-input-drivers-archr.patch` | `004-input-drivers.patch` | **上游覆盖**，改用上游 |
| `kernel/patches/0005-odroidgo3-joypad-6.12-compat.patch`（改驱动适配新 API） | `002-gpiolib-of-legacy-api.patch` + `003-input-add-input-polldev-driver.patch` | **上游覆盖**，把老 API 加回内核，驱动不改；本地补丁退役 |
| `kernel/patches/0001-mfd-rk8xx-codec-of-compatible.patch` | 未在 RK3326 列表直接对应 | **本地保留**，迁移时确认是否仍需要 |
| `dts/panel-r36s.c`（自写 DSI 面板驱动 + 151 条初始化） | `001-panel-updates.patch`、`007-ogs-panel-timings.patch`、`025-px30s-dsi-dphy-timing-table.patch` | **待验证**：若上游面板驱动能覆盖 R36S 的初始化序列，则退役自写驱动；否则本地保留 |
| `dts/rk3326-r36s.dts` | ROCKNIX 有自己的板级 DTS/overlay | **本地保留**（至少 R36S 机型），迁移时对比 pinmux、电源、面板节点 |
| — | `023`–`039` px30s/px30 系列（tsadc、GPU 电源域、DDR、USB2 PHY、VOP AFBC 等） | **新增收益**，之前没有；迁移后一并测试 |
| — | `0008-mmc-sd-retry-*`、`003/004/010/035/038 usb-dwc2*` | **新增收益**，与关机/重启/USB 稳定性相关 |

## 二、迁移原则

1. **先锁定上游**：在 `manifests/sources.lock` 固定 ROCKNIX 提交、Linux 版本、补丁校验值；
2. **只替换内核构建输入**：rootfs、设备启动包、Flash 运行时保持不动，便于对照回滚；
3. **逐条验收**：显示、输入、音频、WiFi、SD、USB、ES/RA 输入、Flash 回归；
4. **重复项不双维护**：上游已覆盖的本地补丁直接删除，历史留在 `docs/history/`；
5. **未知项不猜**：`0001` 和面板驱动必须实测后再决定去留。

## 三、待决策

- **内核版本**：ROCKNIX nightly 已到 6.18/7.1；我们当前是 6.12。是 pin 一个 6.12 时代的补丁集，
  还是直接跟 6.18 LTS？需要先验证补丁在目标版本的适用性。
- **R36S 是否在 ROCKNIX 上游设备列表**：若没有独立 DTS，我们的板级 DTS 仍需本地维护。
- **面板驱动**：自写 vs ROCKNIX panel 更新，二者只能留一个。

## 四、下一步

1. 拉取 ROCKNIX RK3326 补丁全集，逐个 `git apply --check` 到本地 6.12/6.18 树；
2. 记录失败项（上下文漂移、API 变化）；
3. 先做「输入 + 面板 + 电源」最小可启动组合，再逐步并入其余补丁；
4. 通过回归后删除本地重复补丁并更新 `docs/history/`。


## 五、版本事实核对（2026-11-18）

- Debian 13 的代号就是 **Trixie**，这点没争议。
- **ROCKNIX 并没有一套“6.12 LTS 的 RK3326 设备补丁”可直接拿**：
  - 他们的 `projects/ROCKNIX/devices/RK3326/patches/linux/` 目录约在 **2025-07** 才出现，
    当时内核已到 6.16/6.17；
  - 仓库随后迁到 6.18，再迁到 7.x；
  - 6.12 时代（2025-01 之前）仓库里没有这个设备补丁目录。
- 因此只有两条路：
  1. **内核跟到 6.17/6.18 LTS**，直接复用 ROCKNIX 补丁（维护成本最低，但要重新回归）；
  2. **留在 6.12**，把补丁逐条移植（我们已做了一部分，但维护两份）。
  建议先做 6.18 LTS 的试编译与启动验证，再决定是否切换；6.12 保留回滚。
- dArkOS 的构建流程可参考：`christianhaitian/dArkOS` 的 `bootstrap_rootfs.sh`、`Makefile`、
  `Emulationstation/`、`audio/`、`batteryplus/` 等；只参考流程与目录，不整体复制。
- 另有一个现成的开源参考：`Cristiangc67/ruffle-r36s-darkos`（R36S 上的 Ruffle ARM64 适配），
  可用于对比启动器与引擎选型；使用前先核对其许可证。


## 六、第一次真实试编译（2026-11-18）

对象：Linux **6.15.6** + ROCKNIX 提交 `4ad88b8bc7`（2025-12-05）的 12 个 RK3326 补丁。

结果：

1. **12 个补丁全部干净应用**（`patch -p1 --dry-run` 全 OK），补丁里直接带
   `rk3326-gameconsole-r36s.dts`、面板 `gameconsole,r36s-panel`、`joypad-name = "r36s_Gamepad"`。
2. **但直接编译失败**：`004-input-drivers.patch` 依赖 `input-polldev` 和 `of_gpio_legacy`，
   而这两个 API 在 6.15 已删除；ROCKNIX 后来（2026-08/09）才补了 002/003 两个补丁。
3. 用当前 main 的 `003-input-add-input-polldev-driver.patch` 对 6.15.6 应用：**干净**；
   `002-gpiolib-of-legacy-api.patch` 的 `of_gpio.h` hunk 冲突，其余部分可用。
4. 最小修复：在 `include/linux/of_gpio.h` 补回 `enum of_gpio_flags` 和
   `of_get_named_gpio_flags()` 声明；开启 `CONFIG_INPUT_POLLDEV=y`。
5. **编译通过**：`Image` 48MB、`rk3326-gameconsole-r36s.dtb` 49KB。

结论：**6.15.6 + ROCKNIX 补丁是一条可编译路径**，但不是“拿来即用”，需要一个小 backport。
下一步是刷机验证启动、面板、输入、音频、WiFi 与 Flash；通过后再决定长期 pin 6.15 还是移植到 6.18/7.x。


## 七、6.15 控制组与 v3 实机结果（2026-11-18）

- **控制组**：6.15.6 原版 + 我们的 `rk3326-r36s.dts` + 自写 `panel-r36s.c`（内置）。
  结果：**启动成功**，面板 `probe/prepare/enable`、`fb0`、panfrost、WiFi、SSH 全部正常 → 证明 6.15 内核本身可用。
- **v3**：控制组 + ROCKNIX `004-input-drivers` + `003`/`002` backport（内置 polled input、恢复 legacy GPIO）。
  结果：**启动成功**，`r36s_Gamepad`（odroidgo3-joypad）恢复，面板/网络正常。
- 结论：之前 v1/v2 的黑屏来自 **ROCKNIX 的 DTS/面板补丁（000/001/007）**，不是 6.15 内核。
- 未解决：`rk817-sound: deferred probe pending: asoc-simple-card: parse error`（缺 6.12 的
  `0001-mfd-rk8xx-codec-of-compatible.patch`）；电量显示 0%（rk817 电量驱动/DTS 待修）。
- 下一版：v4 = v3 + 0001 codec 补丁，先恢复声音；再单独做面板补丁对比与电量修复。


## 八、v3 回归结果（2026-11-18，Flash 线实测）

- 输入：`r36s_Gamepad`，键码 304/305/307/308、310–313、314–316、317/318、544–547，
  `ABS=0x1b`，与 6.12 完全一致；
- 面板、ES、WiFi 正常；
- 旧冻结核心 2.6：因无声卡 `rc=101` 退出（6.12 上无音频设备时同样行为）；
- 新运行时（Weston + wgpu）：画面/中文正常，无声卡仅告警，不致命；
- 结论：**除音频外，6.15 v3 与 6.12 等价**。

**v4**：v3 + `0001-mfd-rk8xx-codec-of-compatible.patch`（修声卡），Image md5 `0295f5db…`，待装。
**待修**：rk817 电量显示 0%。

**v4 结果（2026-11-18）**：6.15.6 + `0001-mfd-rk8xx-codec-of-compatible.patch` 启动成功；
`card 0 [rockchiprk817co]` 出现，Master 60/255；`r36s_Gamepad`、面板、WiFi 正常。
**电量仍为 1%/Discharging，未修**。6.15 目前是候选内核，默认仍是 6.12。

**v5 结果（2026-11-18 深夜）**：把 6.12 的 `drivers/power/supply/rk817_charger.c` backport 到 6.15，
电量读数恢复：`capacity=66%`、`charge_now=2109580/3200000`、`voltage=4.10V`。6.15 至此具备
启动/面板/输入/音频/电量/WiFi；仍是候选内核，默认 6.12，待 Flash A/B 回归后决定。
