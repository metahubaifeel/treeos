# Flash Play · NGP45HT 源码快照

这是已在 NGP45HT（RK3562 / Mali-G52，1024×768，Debian 13 用户态）运行的独立 ES Flash 系统的**源码与补丁**。与本仓库已有的 RK3326 `tools/launcher/` 并列，不替换它。不是系统镜像或安装包，不含播放器二进制、第三方光标库、ROM、封面、存档、私人配置或任何设备凭据。

- ES 传原 `.swf` 或受限 `.flashref`，按游戏独立配置和存储。M 在同一游戏进程中开启/关闭设置；十字、左右摇杆方向分开绑定，同一物理输入可持有最多三个动作。Select+Start 退回系统。
- `shared/flash-saves.py` 备份整棵持久存储，恢复排队到下次启动；不是即时存档。`shared/flash-doctor.py` 是预检，成功并不等于可通关。
- `shared/` 是入口、存档、标题路由及工具；`mapper-pilot/` 是实际 v7 输入编译器与 SDL 设置菜单。`player-patches/` 为上游播放器平台差异。`qa/` 为**未发布为默认运行时**的时间线候选、低画质和测量工具。不应因合并了源码就把试验版推给所有游戏。

## 无掌机先验证

需要 Python 3.11+，在本目录执行：

```sh
python3 shared/test-shared.py
python3 qa/test-slow-window.py
```

测试只生成原创的小 SWF 和临时状态，不读取玩家档。不要直接在其他系统执行 `display-worker.sh`：其中的 KMS 所有权、ES 恢复、背光、Noto CJK 字体路径、SDL 手柄 GUID/槽位及非交互 systemd 权限均是 NGP 宿主契约。不要为移植放宽通用 sudo 权限。

## 播放器源码基线和补丁顺序

取得两个上游仓库，放为同级 `r4c/`（[Fancy2209/ruffle4consoles](https://github.com/Fancy2209/ruffle4consoles)，提交 `8edbe9e548ea23f70d2b9caf3515fb8dda0e68db`）和 `ruffle/`（[ruffle-rs/ruffle](https://github.com/ruffle-rs/ruffle)，提交 `563854a8911eda672b1836fb374063162879035d`），在干净工作树中：

1. 在 **r4c/** 应用 `player-patches/r4c-ngp-base.patch`。包含输入、舞台尺寸与最终面板适配、脏区域纹理更新、本地资源及可选标题级联网适配，并锁定同级 Ruffle 源码。网络未配置时不启用远端获取。本快照不附带站点模拟配置。
2. **共用版**使用上述基线。**仅噩梦专版**使用独立工作树：在 **ruffle/** 依次应用 `mapper-pilot/input-proof/ruffle-sparse-array-enumeration.patch` 和 `ruffle-perf-blit.patch`，在 **r4c/** 应用同目录的 `r4c-render-options.patch`。保留高画质/不跳帧，不配置 `video-options-v1`。BvS 2.6 的两份精确内容另用已验证的历史显示运行时，不能把新二进制复制过去充当回退。
3. 工具链参考 Rust 1.94.1、`aarch64-unknown-linux-gnu`、与目标 sysroot/SDL2 匹配的 C 链接器。覆盖上游 nightly 参数 `RUSTFLAGS=''`，噩梦用 `CARGO_PROFILE_RELEASE_OPT_LEVEL=3 CARGO_PROFILE_RELEASE_DEBUG=0 CARGO_PROFILE_RELEASE_STRIP=debuginfo cargo build --locked --release --target aarch64-unknown-linux-gnu`。须先配置交叉链接器及目标库搜索路径。本页记录源码身份和顺序，不承诺其他工具链产生相同字节。
4. 设置菜单按目标 SDL2 构建：`cc -O2 -fPIC -shared -DNGP_FREE_KEYS -DNGP_SESSION_MOUSE mapper-pilot/quick-menu.c -o quick-menu.so $(sdl2-config --cflags --libs) -ldl -Wl,--no-undefined -Wl,-z,defs -l:libSDL2_ttf-2.0.so.0`。用 `shared/check-menu-so.sh` 验证。运行时另需合法取得、与 ABI 匹配的现有 `libruffle_cursorfix.aarch64.so`、`flash-quit-watch` 和 `compile-face-profile.py`；版本化 `release-ui8/` 与同级 `runtime-*/` 不得混搭。此快照不包含一键安装、商业游戏配套资源或所有历史运行时的重建材料。

## 经设备核对的已装版本（非下载清单）

| 组件 / 角色 | SHA256 |
|---|---|
| ES worker 入口 `flash-play.py` | `574279bf6149342d917e23184b604057bf0b1bce4eb480868ab2f41f371e34f1` |
| 共用 ui8、net2、spoof1 播放器 | `dc9e2434edc1387a08666d4883f24aa340b0f7b74ada662f60aaeb9544ef58e4` |
| BvS 2.6 历史显示例外 | `2fd29a2a108303b9d527e02b64424f0dc899ba553dbf6dcfa83ca454e75db0c9` |
| 噩梦旧档枚举 + 位图优化 | `61f306f26e41a7899c17bb3a382ccc3c43838b5e47f2cd243747c9af1d222792` |

播放器、渲染和 VM 补丁基于上述上游项目，保留其版权与许可要求；参见仓库 `THIRD_PARTY_NOTICES.md`。此目录不重新许可第三方作品。
