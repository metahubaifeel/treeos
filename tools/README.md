# tools

Flash 运行时相关的辅助脚本与示例。除特别注明外，本目录代码为 **MIT**。

- `batch_test_flash.sh`：串行启动 SWF 样本，记录退出码、OOM、存活时间；
  结束前会清理 Ruffle/gptokeyb 进程再恢复 ES。
- `gamecontrollerdb-r36s.template`：合并手柄（`r36s_Gamepad`）的 SDL 映射模板。
- `example.profile`：逐游戏键位 profile 示例（无游戏资源）。

说明：早期版本的 `padmerge.py` / `stickmouse.py` 是用户态输入桥，
在内核层合并手柄之后已退役，不再随仓库提供。

本目录不含任何 ROM、SWF、音频、字体或汉化资源。
