# TreeOS Flash 启动器（R36S / RK3326）

统一入口，把 Flash 游戏的输入、后端与画面收敛成一次会话。

```
treeos-flash <swf>
  └─ 会话锁 /run/treeos-flash/session.lock
  └─ flash-template 编译控制模板 → session.env / session.profile
  └─ 增强：treeos-flash2 → Weston(kiosk) + ruffle_desktop + padinput
  └─ 经典：es-launch.sh + gptokeyb2（回落路径）
```

## 文件

| 文件 | 作用 |
|---|---|
| `treeos-flash` | 唯一入口：会话锁、停常驻键守护、编译模板、按后端分派、退出清理 |
| `treeos-flash2` | 增强后端：Weston kiosk + Ruffle desktop + padinput |
| `flash-template` | 把模板/逐游戏覆盖编译成 `session.env`、`session.profile`、`session.json` |
| `flashcfg` | profile 唯一读写入口（schema v3，原子写、旧键兼容、.stick 兼容） |
| `padinput.py` | 输入层：独占手柄（EVIOCGRAB）、uinput 键盘/鼠标、一柄多键、点击与键盘并发 |
| `flash-return2.sh` | 设置界面保存后：重新编译 session，再回增强运行时 |
| `treeos-import` | 把 `/roms/import/` 里的文件按扩展名归档到各游戏目录，并为 Flash 包生成 Ports 入口 |
| `treeos-import.service` | 开机前跑一次导入（`Before=treeos-emulationstation`） |
| `flash-mapper-main.c` | 设置 UI（SDL2 + stb_truetype + stb_image；C，单文件） |
| `icons/` | 设置 UI 图标（Kenney Input Prompts，CC0，见 `icons/LICENSE.txt`） |
| `flash-es-launch` / `stop-flash` | 经典后端与清理脚本（R36S 设备路径） |
| `flash-compat-run` | 批量兼容性冒烟脚本 |

## 构建 flash-mapper

```bash
# 需要 SDL2 开发包、同目录的 stb_truetype.h / stb_image.h
gcc -O2 -I. -o flash-mapper flash-mapper-main.c \
    $(sdl2-config --cflags --libs) -lm
# 设备上还需把 icons/*.png 放到 /opt/flashmenu/icons/
```

## 安装游戏（给用户）

镜像不预装任何游戏。用户把自己的文件放进 `EASYROMS` 分区（挂载为 `/roms`）的
`import/` 目录，开机时自动归档：

| 放进去 | 结果 |
|---|---|
| `xxx.gba` / `.gb` / `.nes` / `.sfc` / `.md` / `.n64` / `.cue` … | 移到 `/roms/<对应核心目录>/` |
| `xxx.swf`（单文件） | 移到 `/roms/flash/`，ES 的 Flash 分类直接出现 |
| 一个文件夹 / zip，里面有 `.swf` 和素材 | 整包移到 `/roms/flash/<名字>/`，自动找入口 SWF，生成 `/roms/ports/<名字>.sh` |
| 不认识的类型 | 留在 `import/`，写进 `/roms/import-report.txt` |

- 不覆盖同名文件；已存在会跳过并在报告里列出
- Windows 中文 zip 的 GBK 文件名会自动还原
- Flash 包首次启动可在设置界面按 FN 调键；模版默认 `arrows_pointer`
- 导入器只搬运用户自己的文件，不下载、不内置任何游戏

手动执行：`sudo -n /usr/local/bin/treeos-import`，然后重启 ES。

## profile（schema v3）

最低要求：

```ini
schema=3
control.template=arrows_pointer
control.mouse_left=south
control.mouse_right=east
control.confirmed=1
width=480
height=360
quality=low
```

- `control.template`：`arrows` / `wasd` / `pointer` / `arrows_pointer` / `wasd_pointer`
- 一柄多键：`south=74,32` 表示 A 同时发 J 和空格（最多 6 个）
- 鼠标与键盘可同时：A 可以既是鼠标左键又发键盘键
- 只写用户显式改过的键位；未出现的键沿用模板默认值
- 空值表示显式不绑：`east=`

## 设计不变量

1. 启动时不写持久配置，只写 `/run/treeos-flash/<stem>-<pid>/`
2. 一个会话只有一个输入所有者：增强模式 `padinput`，经典模式 `gptokeyb2`
3. 设置界面是唯一事实源：启动器读取的每个键都能在 UI 里看到、改到
4. 所有退出走同一清理路径；设置界面初始化失败回 ES，不重开游戏

## 第三方

- `stb_truetype.h`、`stb_image.h`：public domain / MIT（见文件头）
- 图标：Kenney *Input Prompts*，CC0；`icons/LICENSE.txt`
