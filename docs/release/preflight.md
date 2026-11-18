# 发布前检查清单（公开镜像）

目标：镜像可公开分发，不携带任何个人信息、凭据或用户数据。

## 必须清除

- [ ] WiFi 配置：`/etc/NetworkManager/system-connections/*`（SSID/密码）
- [ ] SSH：`/etc/ssh/ssh_host_*`、`~/.ssh/authorized_keys`、`known_hosts`、私钥
- [ ] shell 历史：`~/.bash_history`、`~/.zsh_history`、`~/.python_history`
- [ ] 用户目录个人文件：下载、文档、图片、聊天记录、项目源码
- [ ] EmulationStation：`~/.emulationstation/gamelists/` 的最近游玩、收藏、缩略图缓存
- [ ] RetroArch：`~/.config/retroarch/` 的最近记录、播放列表、存档状态（如含个人信息）
- [ ] 日志：`/var/log/*`、`journalctl --vacuum-time=1s`、`/tmp/*`
- [ ] 浏览器/输入法/剪贴板残留
- [ ] 本机 hostname/时区之外的设备标识（如需要可保留通用 hostname）

## 必须保留

- [ ] 驱动、固件、内核/DTB、启动配置
- [ ] 中文语言包、字体、主题
- [ ] 空白用户目录与默认配置
- [ ] 许可证与第三方声明

## 校验

- [ ] `grep -rE "ssid|psk|password|BEGIN .*PRIVATE KEY"` 扫描 rootfs
- [ ] 挂载后人工检查 `/home/*`、`/etc/NetworkManager`、`/etc/ssh`
- [ ] 新卡冷启动：无网络配置、无个人数据、可进 ES
- [ ] 镜像 SHA256 与发布说明一致

## 发布物

- [ ] 镜像（压缩 + sha256）
- [ ] 内核/补丁/config/DTS 的对应源码获取方式（GPL 义务）
- [ ] 已知问题与支持矩阵（引用 `STATUS.md`）
