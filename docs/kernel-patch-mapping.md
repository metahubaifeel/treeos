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
