# 结论先行

**TreeOS 应采用“按设备提供启动包，按 SoC 复用内核组件，所有设备共用 arm64 用户态 rootfs”的方案。**不要把“一个镜像自动适配所有板子”作为首要目标：这些掌机的启动链、设备树、屏幕、电源和输入实现差异太大；误选 DTB 可能导致黑屏或无法启动。可在后续统一安装器里自动识别设备并选包，但底层仍保留每设备启动配置。

**OldBits 兼容性先做“可验证的 ARM64 原生 ports 契约”，不要承诺 Xbox/PS2 大作。**社区提供的运行宣传、设备支持和授权信息目前没有独立证据；应先让每类 SoC 跑通一个轻量原生 port，以帧时间、内存和温度给出可复核的结果。

**Flash 继续优先。**复用导入、逐游戏 profile 和兼容性清单；ports 先作为一个小型试点，不扩成下载商店或游戏平台。

---

## 1. 多 SoC 架构

### 镜像策略

推荐按层拆开发布：

| 层 | 建议做法 | 内容 |
|---|---|---|
| 共用 rootfs | 一份 arm64 rootfs | ES、RetroArch、Ruffle、ports 启动器、共享用户态库、工具 |
| SoC 层 | 每个 SoC 一个内核/模块包 | kernel config、GPU/显示相关内核模块、固件、可复用驱动 |
| 设备层 | 每块板一个启动包 | bootloader 参数、DTB、屏幕配置、Wi-Fi、电源与输入配置 |
| 设备选择 | 安装器或首次启动探测 | 根据设备 ID 选择**已验证**的启动包，不在启动时盲猜 DTB |

维护多个镜像会增加构建和发布负担，但能够把错误限定在具体设备包中。建议先发独立设备镜像；构建与更新仍共用同一个 rootfs 制品。等至少 3 个设备包的启动、升级和回滚流程稳定后，再做统一镜像外壳。

### 板级抽象放置

- **设备树**：SoC 与板卡硬件描述，包括 regulator、GPIO、panel、触摸/按键、存储、Wi-Fi、电源键。不要用设备树承载用户配置。
- **Panel**：优先使用内核 DRM panel/bridge 与对应显示控制器驱动；分辨率、时序和旋转由板级 DTB/overlay 配置。GPU 是渲染器，不负责驱动面板；Mesa 文档也明确指出 Mali 是 3D GPU，显示控制器是另一部分。[Mesa Panfrost 文档](https://docs.mesa3d.org/drivers/panfrost.html)
- **Input**：由设备专属的内核输入驱动或用户态桥接层提供统一逻辑手柄。
- **GPU**：内核 DRM 驱动、固件及 devfreq 属于 SoC/板级包；Mesa 的 OpenGL/EGL 用户态尽量与 rootfs 版本统一。Mesa 当前列出 G31、G52 属于 Panfrost 支持范围，但每块板仍需验证 DRM、显示控制器和组合路径。[Mesa Panfrost 支持表](https://docs.mesa3d.org/drivers/panfrost.html)
- **电源管理**：充电芯片、ADC、电池计、thermal zone、CPU/GPU devfreq 属于板级配置；前端只读统一服务或 sysfs 接口，不硬编码 SoC。
- **Wi-Fi/蓝牙**：驱动模块、校准数据和固件放设备/SoC 包，NetworkManager/wpa_supplicant 与配置 UI 共用。固件许可和来源应随包记录。

### 手柄统一

不要要求 ES、RetroArch、Flash、各 port 分别合并原始 ADC 和 GPIO 输入；这会把板级差异泄漏到每个应用。

建议建立 **TreeOS Input Bridge**：

1. 每块板先暴露原始 event 节点。
2. 首选内核驱动把摇杆与按键合并成一个标准 `BTN_*` 设备；RK3326 保持已验证的 `odroidgo3-joypad` 路径。
3. 对没有合适内核驱动的 H700/RK3566，使用轻量用户态 bridge：按 udev symlink/设备标识读取原始 evdev，做 ADC deadzone、方向与按键映射，再通过 `/dev/uinput` 发布统一虚拟手柄。内核文档支持此类用户态虚拟输入设备，也建议新代码考虑 libevdev。[Linux uinput 文档](https://docs.kernel.org/input/uinput.html)
4. 标准设备名和能力固定，例如 `TreeOS Gamepad`，轴限值 `-32767..32767`，标准 BTN/SYN 事件，使用 SDL GameController DB 做 SDL 语义映射。
5. 禁止原始设备和虚拟设备同时被前端消费造成双输入；用 udev 标签或服务配置明确排除原始节点。

前端自行合并只适合作为短期诊断方式，不适合作为系统契约。验收标准：三类设备都只呈现一个逻辑手柄，ES/RA/SDL2 测试程序的按钮、轴、热插拔和休眠恢复一致。

### 一份用户态 rootfs

统一为 `arm64/aarch64`，ports 首选同一 ABI。用 rootfs 内的标准 Mesa、SDL2、ALSA、glibc 和 ES/RA/Ruffle；板级包提供内核、固件和硬件配置。设备身份通过 `/etc/treeos/device.json` 或 systemd hwdb 提供。

不要把每个游戏自带的 `.so` 直接塞进系统库目录。对确有旧库依赖的 port，用 per-port 目录或 bubblewrap/chroot 隔离，并设置局部 `LD_LIBRARY_PATH`。不同板子之间应报告 GPU renderer、EGL/GLES 版本和 Mesa 版本，而不是仅报告“Panfrost 已安装”。

---

## 2. OldBits/ports 兼容层

OldBits 社区描述应当视为**待验证的外部输入**：本次公开检索没有找到足以确认其设备矩阵、许可、构建方法和兼容 ABI 的可信官方资料。官网跳转、第三方网盘和缺少公开授权信息，均不足以作为 TreeOS 的集成依据。

### 推荐目录和启动器契约

```text
/usr/share/treeos/ports/<port-id>/manifest.json     # 系统只读元数据
/usr/lib/treeos/port-runner                         # 统一启动器

/home/<user>/Ports/<port-id>/
  port.json                                         # 用户安装/覆盖元数据
  bin/                                              # ELF 或启动脚本
  data/                                             # 用户自备或 port 自带资源
  lib/                                              # 仅限此 port 的私有库
  config/
  saves/
  logs/
```

ES 系统条目通过 `port-runner <port-id>` 启动，不直接拼接游戏路径。manifest 至少含：

- `id`、版本、架构、入口、工作目录、命令参数；
- 需要的 API：SDL2、GLES、ALSA 等及最低版本；
- 输入类型、默认映射、分辨率/缩放提示；
- 存档与配置相对路径；
- 测试设备、帧率目标、兼容状态、许可说明。

启动器契约：

- 启动时切换到 port 工作目录，传入规范化用户数据路径。
- 每个 port 使用独立 HOME/config/save/log；不能污染 ES 或 RetroArch 配置。
- 用白名单环境启动，再补上 TreeOS 明确支持的变量：`HOME`、`XDG_CONFIG_HOME`、`XDG_DATA_HOME`、`SDL_GAMECONTROLLERCONFIG`、`SDL_VIDEODRIVER`（必要时）、`ALSA_CONFIG_PATH`（必要时）。
- 默认不设置全局 `LD_LIBRARY_PATH`；需要私有库的 port 仅在子进程作用域设置。
- 标准输出、标准错误写入逐 port 日志；记录启动参数、设备标识、Mesa/SDL/内核版本。
- 保留真实退出码；`0` 成功、非零失败。信号退出记录为 `128 + signal`，前端仍返回到 ES，不吞掉崩溃状态。
- 确保只读根下的临时文件、shader cache、配置和存档写到用户可写分区；大型临时数据不要放 tmpfs。

重编译 port 常见交付物是目标 ARM 上的原生 ELF、资源目录和启动脚本，但每个项目实际情况不同，必须逐 port 检查。先用 `file`、`readelf -h/-l/-d` 和 `ldd` 确认 ISA、动态加载器、依赖与 `NEEDED` 项。

**不默认需要 box86/box64/FEX。**本题目标是 ARM Linux 重编译版，原生 `aarch64` 应直接运行。只有遇到 x86/x86_64 ELF 时才评估转译；在 1GB 内存及 A35/A53 上，把它作为单独实验，不算 TreeOS 通用兼容能力，也不能因此承诺 Xbox/PS2 大作。也不要把“支持 Panfrost”误当成“支持 Vulkan/DXVK/Wine”；需要逐项证实驱动 API、游戏 API 和内存余量。

### 三类 SoC 兼容等级

- **L0：能安装**：入口存在、架构正确、依赖检查通过。
- **L1：能启动**：进入可操作画面、音频和手柄可用，连续 10 分钟无崩溃。
- **L2：可玩**：30 分钟，符合性能门槛，无热降频导致的持续退化。
- **L3：推荐**：60 分钟压力场景稳定，存档/配置隔离，升级和退出流程通过。

在验收前，三类 SoC 都只应标 **L0/L1**，不能从某型号的营销视频推定同 SoC 的其他设备也达到 L2。

---

## 3. 性能分级和承诺边界

以下是 TreeOS **建议的验收门槛**，不是对具体 port 的预判。测试必须用最终分辨率和默认画质，关闭后台下载，记录 10 分钟热身后至少 20 分钟数据。报告平均 FPS、1% low、帧时间 P50/P95/P99、RSS、温度曲线和频率。

| 平台 | 现实承诺范围 | 建议 L2 门槛 | 不应承诺 |
|---|---|---|---|
| RK3326，A35×4、G31、1GB | 轻量 2D、SDL 游戏、低分辨率 GLES 原生小品；640×480 更合适 | 目标 30 FPS；P95 帧时间 ≤ 40 ms、P99 ≤ 67 ms；RSS ≤ 450 MB，系统可用余量 ≥ 150 MB | Xbox/PS2 级重型 3D、依赖 Vulkan/DXVK 的复杂游戏、任何 x86 转译的大作 |
| H700，A53×4、G31、1GB | 类似级别的轻量原生 ports；分辨率影响显著，720×720 像素数约为 640×480 的 1.69 倍 | 先用 640×480 参考测试；原生分辨率需另测；同上内存门槛 | 按芯片标称就推定比 RK3326 快；G31 或 A53 名称本身不能证明某重型 port 可玩 |
| RK3566，A55×4、G52、1GB | 更复杂的原生 2D/轻 3D ports，可做更高分辨率试验；单机仍需独立验证 | 目标 30 FPS（轻型 3D）；P95 ≤ 40 ms、P99 ≤ 67 ms；RSS ≤ 500 MB，余量 ≥ 150 MB | 因为宣传案例就承诺 Ninja Gaiden Black、RE4 或通用 Xbox/PS2 性能 |

温度建议使用设备实际 thermal zone 和频率曲线判定，不给一个跨板卡的固定温度承诺：连续 20 分钟不得出现持续 5 分钟以上的帧率跌幅超过热身后基线的 20%；如达到厂商 thermal trip 阈值或系统触发明显降频，判为失败。至少报告机壳环境温度、SoC 温度峰值与测试室温。FPS 不能只报平均值；30 FPS 下应同时看帧时间长尾。

### 适合承诺的类别

- 原生 ARM64、SDL2 或简单 GLES；
- 低分辨率 2D、像素风、轻量独立游戏；
- 资源规模和内存占用可控、输入路径明确的重编译 port。

### 明确不要承诺的类别

- 需要 x86 转译、Wine/DXVK 或复杂 Vulkan 路径的游戏；
- 宣传材料中没有可复现数据的 Xbox/PS2“重编译版”；
- RSS、帧时间或温度未达门槛的游戏；
- 只在 RK3566 跑过就对 RK3326/H700 做全平台承诺的游戏。

---

## 4. 兼容性验证清单

| 项目 | 检查方法 | 通过标准 |
|---|---|---|
| ELF / ABI | `file`, `readelf -h -l -d` | ELF machine 为 AArch64；动态加载器与 rootfs 一致；无缺失 `NEEDED` 库 |
| glibc | `ldd --version`、`readelf --version-info`，查 GLIBC 符号需求 | port 要求的最高 GLIBC 符号版本不高于 TreeOS；或使用隔离兼容运行时 |
| 32 位 ARM | `file`/ELF header；确认 armhf ABI、NEON/VFP 需求 | 只有明示启用 armhf multiarch/chroot 后才纳入支持；不把“ARM Linux”当成 ABI 兼容 |
| SDL2 | `sdl2-config --version`；跑 SDL video/audio/joystick 小样；核对动态链接 | SDL 主版本/导出符号匹配；窗口/全屏、音频、控制器均成功 |
| GLES/EGL | `eglinfo`、`glxinfo`（如适用）、GLES 渲染器信息；跑 GLES 2/3 示例 | 使用硬件 Mesa renderer；记录 ES 版本、EGL vendor、渲染器；禁止意外回落 llvmpipe |
| Panfrost | `dmesg`、`/dev/dri`、`modetest`、Mesa renderer、压力渲染 | DRM 设备可用、无 GPU hang；具体 GLES/Vulkan 能力由运行时探测，不推测 |
| ALSA | `aplay -l`、`speaker-test`、port 实际播放/暂停/退出 | 正确声卡、无明显爆音/延迟；退出释放设备 |
| 手柄 | `evtest`；`sdl2-jstest` 或自制 SDL GameController 检查；检查 GUID 与 DB | 每个逻辑键轴只映射一次；方向、死区、肩键、启动/返回键一致。SDL 的 Linux 排障文档建议从 evtest、设备权限和 udev 识别检查起。[SDL Linux notes](https://wiki.libsdl.org/SDL2/README-linux) |
| 输入合并 | 查 `evtest` 设备列表、重复事件和 udev symlink | 只让应用看见 TreeOS 虚拟手柄；摇杆和按键无重复输入 |
| 路径假设 | 用只读根启动；临时 HOME；从非默认 cwd 启动；路径含空格试跑 | 资源路径由 manifest/工作目录定位；配置/日志/存档只写用户目录 |
| 只读根 | `/` 只读挂载或容器只读根，strace 查写入失败 | 所有预期写入都落到用户分区；shader cache/temp 可清理 |
| 大小写敏感 | 扫描资源引用与实际文件名；在 Linux 文件系统上跑完整关卡/菜单 | 无大小写不匹配、Windows 路径分隔符和硬编码盘符 |
| 存档与升级 | 改配置、保存、退出、重启，再更新 port 版本 | 每个 port 存档独立、更新不覆盖用户数据，可导出备份 |

Mesa 文档显示 G31/G52 有 GLES 3.1 支持项，但硬件驱动、内核、Mesa 构建和具体扩展仍需现场探测，不能只根据 GPU 型号判定 port 可用。[Panfrost 能力表](https://docs.mesa3d.org/drivers/panfrost.html)

---

## 5. 与 Flash 线的关系与优先级

**共用机制，分离运行时。**

复用：

- 用户自备内容导入；
- 每个内容独立的 profile/配置；
- manifest、校验和、启动器日志；
- 兼容性清单和设备级测试结果；
- 数据目录隔离和从只读根运行的原则。

Flash 与 ports 的格式、库依赖和启动行为不同，不能强行共用运行时。Flash 目前 20 个样本能启动、BvS 进入对战，是已有的用户价值；ports 试点不应拖慢 Ruffle 稳定化、输入映射和 Flash profile 工作。

建议时间上限：**前两周总工程量不超过 20%**，先完成一个轻量 port / SoC 的最小试点；第三周再根据是否有真实用户需求、授权可查和性能达标决定是否扩展。不要先做商店、在线安装器或游戏库统计。

---

## 6. 合规边界

这里不能仅凭“重编译”判断合法。游戏代码、音频、图像、关卡数据等可能分别受权利控制；美国版权局资料也将电子游戏区分为程序与视听内容两部分。[美国版权局 Circular 61](https://www.copyright.gov/circs/circ61.pdf)

执行做法：

1. **TreeOS 默认不预装、不镜像、不代转存含商业游戏内容的 port 包或资源。**继续采用用户自备导入。
2. 对 port 启动器、开源引擎和独立游戏，记录源码/构建来源、许可证、版权声明、依赖 SBOM 和再分发条件。
3. 对商业游戏重编译物，要求发布方提供权利人授权或可核验的再分发许可，至少覆盖代码、资源、音乐/视频和下载分发地区；“用户买过游戏”不等同于 TreeOS 可代为上传或分发。
4. 不从第三方网盘下载后转存到 TreeOS CDN、镜像或设备预装包；也不要提供直链聚合、校验值目录或一键获取侵权包。
5. 收费硬件与软件分开评估：出售设备本身不必然等同于侵权，但若以商业游戏内容为卖点、预置内容、代安装或指向未授权下载，会显著增加发行/帮助传播风险。销售页只列经验证的硬件和合法可用的软件，不以未经许可的游戏名称作性能承诺。
6. 许可不清的 OldBits 内容只做本地、自备样本的技术兼容验证；不纳入官方仓库，不宣称官方支持。

以上是产品风控建议，不替代目标销售地的律师意见。

---

## 7. 两周落地计划（收益/风险/工作量排序）

1. **建立统一 port manifest 和启动器（高收益 / 低风险 / 中工作量）**  
   先支持单个原生 ARM64 轻量 port：独立 HOME、配置、存档、日志；返回真实退出码；ES 启动后可正常退出回前端。  
   **验收**：同一 port 连续启动 10 次，无配置串用；只读根下运行；能从日志定位退出原因。

2. **把输入统一接口扩到三类板（高收益 / 中风险 / 中工作量）**  
   RK3326 保留现有方案；H700 与 RK3566 各自先确认原始 evdev，再选内核合并驱动或 TreeOS uinput bridge。  
   **验收**：每类设备由 `evtest` 看到一个统一逻辑设备；SDL 读取到标准按键/轴；方向、死区和返回键通过人工检查。

3. **每类 SoC 各挑一个轻量、许可明确的原生 port 做样机（高验证收益 / 低风险 / 中工作量）**  
   不以 OldBits 未验证重型游戏作为样机；首选开源或明确许可、原生 ARM64、SDL2/GLES 的项目。  
   **验收**：每个 SoC 至少跑 20 分钟；记录平均 FPS、1% low、帧时间 P95/P99、RSS、峰值温度、GPU renderer、内核/Mesa 版本。按上述 L2 门槛判定，未达标就只标 L1。

4. **制作单一兼容性报告格式和检查脚本（中收益 / 低风险 / 低工作量）**  
   收集 ABI、glibc、SDL2/GLES、Mesa renderer、ALSA、手柄、路径、只读根与大小写检查结果。  
   **验收**：每个报告都能关联 port 版本、设备 SKU、系统构建号；不同设备结果可以直接比较。

5. **OldBits 风险与需求复核，作为 go/no-go（中收益 / 中风险 / 低工作量）**  
   要求对方或维护者给出可核验的源码/二进制来源、目标架构、依赖和授权信息；没有这些信息时只保留兼容层设计，不做下载集成或性能宣传。  
   **验收**：两周结束时决定“进入扩展测试 / 暂缓集成”；决定依据是授权可核验、原生 ABI 已确认、至少一台目标机达到 L2 三项，而不是宣传视频。

**最终建议：先把 TreeOS 做成三类硬件共用用户态的“设备包 + 共用 rootfs”系统，再通过 manifest、隔离启动器和统一输入接口接 ports。两周目标是证明原生轻量 port 的跨 SoC 契约成立，不是证明这些 1GB 掌机能跑 Xbox/PS2 大作。**
