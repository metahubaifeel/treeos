# TreeOS 交付检查工具（0.1.x）

两个只读脚本，给整卡交付/验收用。**不下载、不内置任何 BIOS 或游戏。**

## `treeos-bios-check`

```bash
treeos-bios-check --roms /roms
treeos-bios-check --roms /mnt/card/EASYROMS --json
```

- 只检查 `roms/*/` 里**实际有 ROM** 的平台，空目录跳过
- 输出四档：`OK` / `FAIL(缺必需)` / `WARN(缺可选)` / `WARN(无法校验)`
- `neogeo.zip` 只能查存在，不能验证与 ROM set 匹配 → WARN
- 0 字节文件、`bios/` 里误放 `.zip` → FAIL/WARN
- 退出码：有 FAIL = 1，否则 0
- 数据：`bios.json`（安装到 `/usr/local/share/treeos-accept/`）

## `treeos-accept`

```bash
treeos-accept --roms /roms --staging /mnt/staging
treeos-accept --roms /roms --allow-saves
```

硬性检查：

1. **ROM 目录里不得有** `.srm` / `.sav` / `.state*` 残留；默认 FAIL，
   只有 `--allow-saves` 才放行（用于你明确要交付自己的存档）
2. `gamelist.xml` 的 `<path>` 指向不存在的 ROM → FAIL
3. `<image>` / `<video>` 缺失 → WARN

然后打印固定样本表：每平台一个样本、通过标准，按 TSV 记录。

退出码：有 FAIL = 1，否则 0。

## 数据文件

| 文件 | 内容 |
|---|---|
| `bios.json` | 平台 → 必需/可选 BIOS、说明 |
| `samples.tsv` | 平台 → 验收样本 → 通过标准 |

字段就是 `10_掌机游戏工程/17_掌机整卡交付模板.md` §七/§八 的表格列。
改表只改这两个文件，不改脚本。

## 安装到卡/镜像

```bash
sudo install -m755 treeos-bios-check treeos-accept /usr/local/bin/
sudo install -d /usr/local/share/treeos-accept
sudo install -m644 bios.json samples.tsv /usr/local/share/treeos-accept/
```

脚本默认从 `/usr/local/share/treeos-accept/` 读数据；测试时可用
`--data /path/to/tools/accept` 或 `TREEOS_ACCEPT_DATA=...`。

## 边界

- 不下载 BIOS/ROM，不提供来源
- 不启动游戏；启动/输入/存档仍由人工按样本表验收
- 不修改 ROM、gamelist 或存档；只报告
