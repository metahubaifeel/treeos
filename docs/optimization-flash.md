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
