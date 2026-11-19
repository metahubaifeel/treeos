# Flash / Ruffle 优化（GPT-6.2 Astra 建议，2026-11-19）

先分清瓶颈：**VM 主线程慢 / 绘制慢 / 仅加载慢**。GPU timeout 是稳定性故障，不是帧率问题。
一次只改一项，按下面的门槛验收。

## 基线（必须先记录）

- GL renderer 必须是 Mali/Panfrost，不能是 llvmpipe；
- Ruffle surface 物理像素、输出刷新率、SWF 原始帧率；
- CPU/GPU governor、MemAvailable、swap/zram。
- `--width/--height` 不等于内部渲染分辨率；`--quality low` 也不等于低分辨率。

## 优化项（按收益排序）

1. **CPU governor → performance**（单游戏 profile，退出恢复）；用 `top -H`/`perf` 找吃满一核的线程。
2. **降低渲染像素**：640×480 → 480×360（-44%），不够再 320×240（-75%）；4:3、同视野、全屏放大。
   必须确认 Ruffle 仍只画小 buffer，否则 Weston 又放大回去等于没改。
3. **Weston 合成**：保留 `--no-gui`；检查 client buffer 是否上 scanout plane；kiosk ≠ 直扫。
4. **GPU governor → performance**：只在该节点支持时做，记录 cur_freq/温度；仍 timeout 立即回退。
5. **加载**：分开测首次进关与重复战斗；`strace -f -e openat` 找读文件；不要把 291MB 全拷 tmpfs。
6. **内存**：看 `pressure/memory`、`pgmajfault`、`pswpin/out`；有压力才试 256MiB lz4 zram。
7. **wgpu/Mesa**：GL 为对照组；只有确认 G31 硬件 Vulkan 且版本支持才切 Vulkan。
   PresentMode 默认 FIFO，只有确认 acquire 等待占主因才试其他模式；AFBC/tiling 交给 Mesa，不批量堆 MESA_*。
8. **帧率上限**：先读 SWF 原始帧率；有 `--frame-rate` 才试，且要核对移动/攻击/计时/声音同步。
9. **AS3 热点**：降像素无效且 perf 指向 AVM2 解释循环时，先确认 release 构建，再试 `-C target-cpu=cortex-a35` 本机 release build。

## 30 分钟 A/B（最多 3 个变量）

- 指标：帧时间 P95、>250ms 卡顿次数、GPU timeout、MemAvailable 最低值、温度；
- 0–4 预热；4–22 三组配对（CPU governor / 低像素 / GPU governor 或直扫），每组 A3+B3，中间回归同状态；
- 22–28 对最佳项反序 B/A 复测；28–30 只比战斗热段；
- 通过线：两次配对 P95 均降 ≥10%，或 >250ms 次数降 ≥50% 且少 ≥2 次/轮，另一指标恶化 ≤5%；
  timeout 必须 0、无 reset/OOM、MemAvailable 最低 ≥64MiB；
- 任一 timeout、映射错误、游戏速度变化、破图即回退。


## 造梦西游实机结论（2026-11-19）

- `--quality low` 不等于降渲染像素；真正有效的是 Ruffle 窗口/渲染尺寸。
- **480×360 是 RK3326/1GB 的平衡点**：RSS 466→439MB，用户反馈“快很多”。
- **320×240 反而更慢**：Weston 放大到 640×480 的额外开销抵消收益；实测温度 84.6°C，
  已贴近 85°C 降频线，GPU 只有 400MHz。
- 结论：再降像素无效，瓶颈在**热降频 + AS3 VM**，不在绘制。
- 正确 profile：`profiles/custom/0_L4399Main_gamefile-offine-mod2.profile`（width=480 height=360 quality=low）；
  多文件游戏必须写到主 SWF 的 stem 上。
- 修复：`flashcfg dump --shell <profile>` 曾把 `--shell` 当 profile，导致启动器没应用配置。


## Flash 控制模板与自动推荐（GPT-6.2 Astra，2026-11-19）

目标：**自动推荐模板 → 玩家确认一次 → 下次直接启动**；不做“自动试玩判断成功”。

### 五个基础模板

`arrows | wasd | pointer | arrows_pointer | wasd_pointer`

- 模板由启动器编译成现有 v2 配置；padinput 与 gptkeyb2 消费同一份解析结果（经典模式不再写死 `dpad=mouse_movement`）。
- 优先序：用户覆盖 > 本机确认 > 包指纹精确库 > 静态证据 > 类型元数据 > 引导。
- 未知游戏先给 arrows/pointer/wasd，并可原地切换全部模板。

### 静态分析（只用于排序，不代替试玩）

- AS2：FFDec 导出 P-code，扫 `Key.isDown` 实参、`onPress/onRelease`、`Mouse.addListener`、`_xmouse/_ymouse`；
- AS3：读 ABC 常量池/方法体，识别 KEY_DOWN/UP、MOUSE_DOWN/UP/MOVE/CLICK；
- 多文件只分析入口与可辨依赖，哈希去重离线索引，不在掌机冷启动全扫；
- 成本 2–3 人日；首推高置信正确率 ≥90%，否则只排序、不静默套用。

### 首次启动引导

- 首推模板 + 一行提示；进入可操作关卡后用热键循环最多 3 个候选，原地切换不重启；切换前释放所有按键/鼠标。
- 判定要“完成一次主要动作”（如黄金矿工下钩、射击命中、拖拽释放），只点开始菜单不算。
- 门槛：两后端各连续切换 30 次无粘键；二次启动恢复已确认映射。

### 启动器优化

- **输入互斥**：增强模式只启动 padinput，经典只启动 gptkeyb2；查清 game-keys 的拉起源，会话结束只回收本会话进程。
- 物理输出恒 640×480；按 Stage 比例适配/留黑边，并做鼠标坐标逆变换。
- 失败回落只在 Weston/wgpu 明确初始化失败时发生一次，不循环、不覆盖偏好。
- 中文名/封面：用户设置 > 本地清单 > 文件名；无图用中文标题卡。
- 诊断导出：版本/包指纹/模板来源/最终映射/日志，默认排除 SWF、存档、私人路径。

### 两周排期（1 人）

D1–2 互斥+经典输入；D3–4 模板编译+指纹库；D5 试玩确认与缓存；D6–7 离线静态推荐；D8 回落+日志；D9–10 实机盲测修复。
超期先砍分析深度和封面，不砍互斥/确认/回归。

### 验收

- 新游戏从进入可操作场景到确认可玩：中位数 ≤1 分钟、P90 ≤3 分钟，较基线降 ≥60%；
- ≥40 个未参与调规则的游戏包覆盖五类，含“菜单鼠标但玩法键盘”“多文件加载器”反例；
- 未知游戏首推直接可玩 ≥75%，前三候选不改键可玩 ≥90%；
- 最终“真的可玩”、动作语义与特殊组合键仍需人工确认。
