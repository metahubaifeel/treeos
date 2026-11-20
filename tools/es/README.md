# TreeOS EmulationStation 开机画面补丁

TreeOS 0.1 的 ES 源码（`PROGRAM_VERSION_STRING "TreeOS 0.1"`）加了两处开机画面改动：

1. **`Window::init` 里提前画图**：`Renderer::init` 成功、窗口建好后立刻把本次 splash 选的
   PNG 全屏画出来并 `swapBuffers()`，不等 ResourceManager/字体加载完。
2. **`renderLoadingScreen` 全屏**：读 `/run/treeos-splash-image`，映射到
   `/usr/local/share/treeos-boot/png/<同名>.png`，全屏 640×480，不再 40% 缩放/黑底；
   第一次 `Loading...` 文本传空。

## 文件

| 文件 | 说明 |
|---|---|
| `treeos-boot-splash.patch` | 只含开机画面两处改动（Window.cpp / main.cpp） |
| `treeos-es-working-tree.patch` | 完整工作区 diff（5 个文件，含 TreeOS 既有未提交改动） |
| `emulationstation-treeos-boot-splash` | 已交叉编译好的 aarch64 二进制（TreeOS 0.1） |
| `SHA256SUMS` | 二进制校验值 |

源码基线：`metahubaifeel/EmulationStation-fcamod`，commit `e9b60e7`
（`treeos: fix keyboard popup left/right wrap; drop unavailable go2 header`）。
精确复现请用 `treeos-es-working-tree.patch`；只看开机画面改动用 `treeos-boot-splash.patch`。

## 不要用错源码

- ✅ `/tmp/es-src`（或对应 TreeOS 源码，`EmulationStation.h` 里是 `"TreeOS 0.1"`）
- ❌ `/home/tree/es-gl`、`/home/tree/es-build` 里的上游 ES 2.13（版本号 `2.13.0.0 CHMOD`），
  编出来会丢 TreeOS 功能。

## 在 5060 上交叉编译

5060 已有 `/opt/aarch64-sysroot`（Debian 13 / glibc 2.41，SDL2 2.32.4、freetype、
freeimage、VLC、GL/EGL、ALSA 齐全）。

```bash
# 1. 解压 TreeOS ES 源码
mkdir -p /tmp/es-build/src /tmp/es-build/build
tar xzf es-src-treeos.tgz -C /tmp/es-build/src
cd /tmp/es-build/src
patch -p1 < treeos-es-working-tree.patch

# 2. freeimage 没有 .pc，补一个
mkdir -p /tmp/es-pc
cat > /tmp/es-pc/freeimage.pc <<'EOF'
prefix=/opt/aarch64-sysroot/usr
exec_prefix=${prefix}
libdir=${prefix}/lib/aarch64-linux-gnu
includedir=${prefix}/include

Name: freeimage
Description: FreeImage
Version: 3.18.0
Libs: -L${libdir} -lfreeimage
Cflags: -I${includedir}
EOF

# 3. toolchain
cat > /tmp/es-toolchain.cmake <<'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_SYSROOT /opt/aarch64-sysroot)
set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)
set(CMAKE_FIND_ROOT_PATH /opt/aarch64-sysroot)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF

# 4. configure + build（20 核约 1 分钟）
export PKG_CONFIG_SYSROOT_DIR=/opt/aarch64-sysroot
export PKG_CONFIG_LIBDIR=/tmp/es-pc:/opt/aarch64-sysroot/lib/aarch64-linux-gnu/pkgconfig:/opt/aarch64-sysroot/usr/share/pkgconfig
cd /tmp/es-build/build
cmake -DCMAKE_TOOLCHAIN_FILE=/tmp/es-toolchain.cmake -DGLES=OFF -DCMAKE_BUILD_TYPE=Release ../src
make -j20
```

产物：`/tmp/es-build/src/emulationstation`。

## 安装

优先用现成二进制（与源码同一版）：

```bash
sudo ./install-boot-splash.sh          # 运行中的卡
sudo ./install-boot-splash.sh /mnt/rootfs  # 离线 rootfs
```

脚本会装 `treeos-splash`、`treeos-splash-run`、`treeos-es.sh`、unit、21 张 PNG，
并把 ES 二进制替换为 `emulationstation-treeos-boot-splash`。

## 验证

```bash
grep -E "TreeOS early boot splash|TreeOS loading" ~/.emulationstation/es_log.txt
cat /run/treeos-splash.log
cat /dev/shm/treeos-boot-timing.log
```
