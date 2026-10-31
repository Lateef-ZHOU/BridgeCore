# BridgeCore 维护入口

2026-10-31：桥接器新增必填 --iface，按接口名选卡；--src 仅为 Pcap 组帧源地址，不校验是否已配置在系统网卡上（用户定）。已更新板卡 ARM64 程序，激光器55100和CDL5 DMA55200两个独立实例经eth0运行，单独启停/重复启动通过。程序与配置持久化到root镜像，系统镜像仅加CDL5五个开关机链接；SD备份 codex_bridge_dma_backup_20261031。保留S11旧服务执行ADC AC耦合初始化，S13新桥接先停止旧程序及健康监控再独占DMA，硬件喂狗不变。内核/设备树/运行库/BOOT.BIN/boot.scr/eMMC未改。本轮未重启或触发真实采集；详见部署说明和VALIDATION。后续可将旧服务中的初始化独立出来，不丢失既有耦合配置。

CDL5接入：8字节命令原样下发，Handle交出最多1024字节/32字节对齐的自有块，Bridge只转发。4k/5k帧长由Handle读寄存器，不在Bridge固化尺寸。DMA入口要求 --dstMac，避免现有ARP单包槽覆盖首帧。cdl5Device固定相邻本地提交，共享ABI显式引用DmaProxy。板上当前活动Profile8为5kHz/450×128，后续真实采集校验以活动配置为准，不沿用早期4k假设。

2026-10-30：现有 ARM64 bridge 安装在 /root/apps/bridge/bin，新增 deploy/petalinux 的 SysV 服务和物理串口配置。激光器由拔温控核实为 USB 路径 1-1.4，当前 ttyUSB0；服务仅解析配置的 by-path，不猜测或探测其他设备。板端 192.168.1.11:55100 对端 Windows 192.168.1.102:55100。服务启动不发送业务命令；正常停止沿既有 SIGTERM 链。实机查询、服务重复启停及持久化校验已通过；开机钩子已写入 SD 镜像，本轮未重启验证。内核、设备树、BOOT.BIN、boot.scr、原采集服务和 eMMC 数据未改。详见部署说明及 VALIDATION；本轮仅本地提交。

- Linux 无 UI 命令行桥接器，Qt Core 事件循环；一进程一串口或DMA设备与一对 UDP 端点。
- 复用 HandleCore Serial/Pcap 和官邸协议头，不依赖 DeviceCore、TaskCore、QML 或绘图库；不复制协议实现。Pcap 是已有以太网 UDP Handle，不新增同功能传输层。
- 数据边界均为 QByteArray。whlsDevice 下行/上行均为 6 字节元信息加数据的主体，由 LaserHandle 补/验/去外层。sqafDevice 沿用当前 Handle：下行 4 字节命令、上行完整 7 字节回帧；本轮未改 Pelco-D 接收接口。
- worker 只收包并排队交出自有字节，主线程执行对端 send；来自 pcap 的原始视图必须深拷贝后跨线程；串口成帧器必须交出自有 QByteArray，按值捕获以隐式共享保活。析构先同步停止两个源，再销毁成员；不增状态缓存、第二条关闭链或业务查询。
- 连接与设备列表的串口规格均从官邸构参取得，不在列表中重复写死。start 返回 true 仅代表两侧接受异步启动，不代表已连接；部分接受后拒绝仍由调用者析构关闭。
- 本地地址枚举、查询 socket、IPv4 文本转换及 MTU 查询失败在现场记录具体系统错误，单接口失败附接口名；空列表及 MTU=0 等返回方式不变。信号读取 EINTR 当前回调重试、EAGAIN 返回事件循环；针对性故障注入只放测试链接入口。
- 首次打开失败退出；后续失败重连归 Handle。Linux signalfd 经 QSocketNotifier 接入 SIGINT/SIGTERM，无轮询定时器、退出标记或异步信号处理函数；启动 worker 前屏蔽这两个信号，由 worker 继承，屏蔽持续到进程结束，不保存或恢复旧 mask，避免未读取的退出信号在关闭尾声触发默认终止。主线程退出循环后先同步关闭 Bridge，再销毁 notifier、关闭 fd。任一 worker finished 仍触发退出。退出不排空，不主动开光/关光或发设备业务命令。
- 只使用普通 Ethernet/IPv4/UDP，直接对目标地址 ARP；不支持网关/VLAN/分片，超出接口 MTU 的回帧明确拒绝。沿用 Pcap 首次 ARP 只暂存最新包的行为，不承诺 UDP 无损传输。
- 中文提交说明。修改后构建并执行 CTest，硬件与 ARM 未实测不能宣称通过。不使用 friend，不新增假想的设备注册框架。
- 当前为本地仓库，未建立远端，未确定本仓许可证/版权署名。激光器新主体接口尚未推送，CMake 固定相邻本地 RayshiningDevice 提交；HandleCore/SqafDevice 使用远端完整提交。发布时再切换远端激光器地址。
