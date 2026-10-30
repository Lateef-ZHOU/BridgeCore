# PetaLinux 激光器桥接服务

本目录对应已核查的板卡 192.168.1.11：ARM64、SysVinit，程序安装在 `/root/apps/bridge/bin/bridge`。不是 systemd 单元。桥接器本身不查询激光器或控制出光，业务由 Windows 客户端发起。

## 当前连接

| 项目 | 配置 |
|---|---|
| 板端 UDP | 192.168.1.11:55100 |
| Windows UDP | 192.168.1.102:55100 |
| 激光器协议 | whlsDevice，115200 / 8N1 / 无流控 |
| 激光器物理插口 | USB 1-1.4，当前 /dev/ttyUSB0 |
| 串口配置 | /dev/serial/by-path/platform-xhci-hcd.0.auto-usb-0:1.4:1.0-port0 |
| 服务文件 | /root/apps/bridge/service/laser-bridge |
| 配置文件 | /root/apps/bridge/etc/laser.conf |
| 运行日志 | /run/laser-bridge.log（内存，重启不保留） |

该插口通过用户拔除温控后重新枚举确认。两个转串口芯片同型号且无唯一序列号，不能依赖 by-id 区分，也不依赖 ttyUSB 数字稳定。服务在启动时只将配置的 by-path 解析为实际 tty；移动激光器插口需修改配置，运行期间拔插导致 tty 重编号时需重启服务。不要让另一个程序同时使用这个串口。

## 操作

```sh
/etc/init.d/laser-bridge status
/etc/init.d/laser-bridge start
/etc/init.d/laser-bridge stop
/etc/init.d/laser-bridge restart
tail -n 50 /run/laser-bridge.log
```

start 的进程存活检查不等于设备已完成通信；串口、UDP 打开结果看日志，端到端状态由 Windows 查询确认。停止发 SIGTERM 并等待既有同步关闭，超时只报告，不强杀。服务自身不补发关光命令；Windows 页面断开时仍按已有行为发送一次关光。

Windows 主程序进入“镭晟激光器”页，选择已配置 `192.168.1.102` 的网卡（本机实测为“以太网 6”），点击连接。初查自动读取脉宽、身份、状态及测量，随后每 500 ms 查询状态/测量。两端使用已有 Pcap 以太网 UDP Handle，载荷是协议主体，MAC 由 ARP 取得；不提供路由/VLAN/分片或可靠交付扩展。

## 文件与开机入口

把本目录 `laser-bridge` 以 0755 放到上述服务位置，`laser.conf` 以 0644 放到配置位置，保持 LF 换行。系统镜像仅增加：

```text
/etc/init.d/laser-bridge -> /root/apps/bridge/service/laser-bridge
/etc/rc5.d/S12laser-bridge -> ../init.d/laser-bridge
/etc/rc0.d/K01laser-bridge -> ../init.d/laser-bridge
/etc/rc1.d/K01laser-bridge -> ../init.d/laser-bridge
/etc/rc6.d/K01laser-bridge -> ../init.d/laser-bridge
```

板卡在 rcS 阶段先加载 `/root` 镜像和运行库，运行级别 5 在网络初始化后启动桥接。服务及配置属于 root-home.tar.gz，系统启动链接属于 image.ub 的用户态根文件系统；内核、设备树、BOOT.BIN 与 boot.scr 保持原样。运行时改配置只改 RAM，长期保存需重新封装 root 镜像和校验文件。科学数据 eMMC 不参与此操作。

2026-10-30 已备份、替换并校验 SD 的 image.ub 与 root-home.tar.gz 及各自 sha256，当前系统也已安装并启动服务。**本轮未重启，开机入口尚未经过实际重启验证。**

## 回退

本次备份在 SD 根目录 `codex_laser_service_backup_20261030`，含原 image.ub、root-home.tar.gz、各自校验文件及 RESTORE.txt。

若 SSH 仍通，先停止桥接服务，再从备份恢复这四个文件，sync 后按需要重启。若 SSH 不通，断电取出 SD，用读卡器将备份目录内四文件覆盖回 SD 根目录，再上电。不要覆盖 BOOT.BIN 或 boot.scr；无需操作 eMMC。备份仅针对本次增量，旧服务与科学数据保持不变。
