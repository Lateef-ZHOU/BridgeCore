# PetaLinux 桥接服务

板卡 `192.168.1.11` 使用 ARM64、SysVinit，统一程序安装在 `/root/apps/bridge/bin/bridge`。两个独立进程分别接激光器和 CDL5 DMA，各有配置、PID 和日志。服务不发起出光或采集；业务命令由 Windows 客户端下发。

管理 SSH 仍使用系统地址 `192.168.1.11`；两路业务改用未配置到系统网卡的 `192.168.100.100` ↔ `192.168.100.200`，端口、物理网卡、MAC 和卸载设置保持。此改动用于对照接收合并现象，是否消除合并以双端抓包为准。

## 连接与文件

| 项目 | 激光器 | CDL5 |
|---|---|---|
| 服务 | laser-bridge | cdl5-bridge |
| Handle 类型 | whlsDevice | cdl5Device |
| 网卡 | eth0 | eth0 |
| 板端 UDP | 192.168.100.100:55100 | 192.168.100.100:55200 |
| Windows UDP | 192.168.100.200:55100 | 192.168.100.200:55200 |
| 设备 | USB 物理插口 1-1.4，115200/8N1/无流控 | /dev/cdl5_dma_proxy 与 FPGA 寄存器 |
| 配置 | /root/apps/bridge/etc/laser.conf | /root/apps/bridge/etc/cdl5.conf |
| 脚本 | /root/apps/bridge/service/laser-bridge | /root/apps/bridge/service/cdl5-bridge |
| 日志 | /run/laser-bridge.log | /run/cdl5-bridge.log |

配置的 `INTERFACE` 明确传入 `--iface`。`SOURCE_IP` 只决定 Pcap 组帧的源 IPv4，不要求系统网络栈已经配置这个地址；接口枚举的 IPv4 仅供查看。两个实例使用同一网卡、不同端口，互不接收对方的数据报。

激光器固定 `/dev/serial/by-path/platform-xhci-hcd.0.auto-usb-0:1.4:1.0-port0`，启动时解析到实际 tty；不依赖 ttyUSB 编号、不探测其他串口。设备换插口需修改配置。激光器目标 MAC 通过 ARP 取得。

CDL5 的 `DESTINATION_MAC=14:4f:d7:c7:d0:8c` 是本轮 Windows“以太网 6”的实际 MAC。更换上位机网卡时需一起修改。DMA 突发强制指定 MAC，避免现有 ARP 等待槽只留最后一包而丢掉首帧；Bridge 不增加分片头，按 Handle 的 1024 字节/32 字节对齐块直接转发。帧长由 FPGA 活动寄存器取得，桥接服务不固化 4 kHz/5 kHz/10 kHz 尺寸。

## 操作

```sh
/etc/init.d/laser-bridge status
/etc/init.d/cdl5-bridge status
tail -n 50 /run/laser-bridge.log
tail -n 50 /run/cdl5-bridge.log

/etc/init.d/cdl5-bridge stop
/etc/init.d/cdl5-bridge start
/etc/init.d/laser-bridge restart
```

两服务都支持 start/stop/restart/status。重复 start 不增加实例；PID 检查同时核对程序路径和设备类型，避免两个实例使用同一可执行文件时串认。start 只检查进程存活，连接是否打开看日志，端到端采集需客户端实测。stop 发 SIGTERM 并等待原同步关闭，不强杀。CDL5 最多等待 20 秒，容纳既有累积窗口和恢复过程；激光器最多等待 10 秒。服务本身不补发关光，Windows 页面断开仍按其既有逻辑发送。

## 开机顺序与 DMA 占用

1. rcS 加载系统运行库和 root-home.tar.gz。
2. `S11cdl33-service` 保留原板卡初始化，包括现有 ADC AC 耦合配置；当前配置不自动开始采集。
3. `S12laser-bridge` 启动激光器桥接。
4. `S13cdl5-bridge` 先调用 `cdl33-service stop`，停止旧采集程序及其健康监控，再独占打开 DMA。

保留 S11 是为了继续使用其已有初始化，不是让两个采集程序并行运行。CDL5 桥接启动后，旧 HTTP 采集服务也随之停止。独立的硬件喂狗服务 `S10cdl33-watchdog` 保持运行，与旧采集健康监控不是一回事。将初始化从旧服务拆出属于后续工作；不要在桥接占用 DMA 时手动重启旧采集服务。

## 镜像归属与回退

程序、两份配置和两份脚本放在 root-home.tar.gz，脚本 0755、配置 0644、LF 换行。image.ub 的用户态根文件系统保留激光器已有链接，仅增加：

```text
/etc/init.d/cdl5-bridge -> /root/apps/bridge/service/cdl5-bridge
/etc/rc5.d/S13cdl5-bridge -> ../init.d/cdl5-bridge
/etc/rc0.d/K01cdl5-bridge -> ../init.d/cdl5-bridge
/etc/rc1.d/K01cdl5-bridge -> ../init.d/cdl5-bridge
/etc/rc6.d/K01cdl5-bridge -> ../init.d/cdl5-bridge
```

内核、设备树、内核模块、系统运行库、BOOT.BIN、boot.scr 和 eMMC 科学数据不变。运行时改 `/root` 只影响 RAM，持久化需同步封装 root-home.tar.gz 及校验文件。

2026-10-31 已更新运行程序，验证两个实例分别启停、重复 start 和 DMA 独占。SD 镜像及 SHA256 已替换并校验；本轮没有重启，开机链接检查不等同于重启实测，也未触发真实采集。

本轮备份目录为 SD 根目录的 `codex_bridge_dma_backup_20261031`，保存更新前 image.ub、root-home.tar.gz、各自 SHA256 和 RESTORE.txt。SSH 不通时可断电取 SD，通过读卡器恢复这四个文件，再上电；不要改 BOOT.BIN、boot.scr 或 eMMC。之前的备份全部保留。

2026-10-31 地址对照已部署并持久化；备份在 `/media/sd-mmcblk1p1/codex_bridge_ip100_backup_20261031_193845`，恢复其中的 root-home.tar.gz 与校验文件后重启可回到旧业务地址（客户端默认值需配套恢复）。22 个完整帧双端比对相同，但新地址仍发生 7168 字节等合并，地址改动没有消除该现象。详细统计见 VALIDATION。

## 三规格程序升级（2026-11-03）

同一 bridge 支持 Profile9/500×128、Profile8/450×128、Profile7/240×240，三路ADC；前两者五lane，后者六lane。服务参数与UDP端口不随FPGA频率改变，DMA Handle按活动寄存器校验布局。上位机须选匹配的官邸。

升级仅替换 root-home.tar.gz 内的 root/apps/bridge/bin/bridge，保留其余文件及元数据；先备份旧归档和校验文件，再验证新归档与上传校验。停止两路服务后更换运行程序，验证再持久化并重启。恢复时将备份归档和校验文件放回SD根目录。保留原V52初始化程序及开机顺序，不使用交付包中更旧的V44替换；内核、代理驱动、系统运行库和eMMC不随此次升级修改。

本次更新已完成并经正常重启验证：两服务自动启动，DMA模块加载，新bridge从root归档恢复。回退备份为 `/media/sd-mmcblk1p1/codex_bridge_profiles_backup_20261103_150805`；恢复其中两份root归档/校验文件到SD根目录即可，不恢复其他层。10kHz真实采集尚待FPGA刷入，当前验证不包含其硬件数据链。
