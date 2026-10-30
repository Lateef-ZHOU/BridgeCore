# BridgeCore 维护入口

- Linux 无 UI 命令行桥接器，Qt Core 事件循环；一进程一串口设备与一对 UDP 端点。
- 复用 HandleCore Serial/Pcap 和官邸协议头，不依赖 DeviceCore、TaskCore、QML 或绘图库；不复制协议实现。Pcap 是已有以太网 UDP Handle，不新增同功能传输层。
- 数据边界均为 QByteArray。whlsDevice 下行/上行均为 6 字节元信息加数据的主体，由 LaserHandle 补/验/去外层。sqafDevice 沿用当前 Handle：下行 4 字节命令、上行完整 7 字节回帧；本轮未改 Pelco-D 接收接口。
- worker 只收包并排队交出自有字节，主线程执行对端 send；来自 pcap 的原始视图必须深拷贝后跨线程；串口成帧器必须交出自有 QByteArray，按值捕获以隐式共享保活。析构先同步停止两个源，再销毁成员；不增状态缓存、第二条关闭链或业务查询。
- 连接与设备列表的串口规格均从官邸构参取得，不在列表中重复写死。start 返回 true 仅代表两侧接受异步启动，不代表已连接；部分接受后拒绝仍由调用者析构关闭。
- 本地地址枚举、查询 socket、IPv4 文本转换及 MTU 查询失败在现场记录具体系统错误，单接口失败附接口名；空列表及 MTU=0 等返回方式不变。信号读取 EINTR 当前回调重试、EAGAIN 返回事件循环；针对性故障注入只放测试链接入口。
- 首次打开失败退出；后续失败重连归 Handle。Linux signalfd 经 QSocketNotifier 接入 SIGINT/SIGTERM，无轮询定时器、退出标记或异步信号处理函数；启动 worker 前屏蔽这两个信号，由 worker 继承，屏蔽持续到进程结束，不保存或恢复旧 mask，避免未读取的退出信号在关闭尾声触发默认终止。主线程退出循环后先同步关闭 Bridge，再销毁 notifier、关闭 fd。任一 worker finished 仍触发退出。退出不排空，不主动开光/关光或发设备业务命令。
- 只使用普通 Ethernet/IPv4/UDP，直接对目标地址 ARP；不支持网关/VLAN/分片，超出接口 MTU 的回帧明确拒绝。沿用 Pcap 首次 ARP 只暂存最新包的行为，不承诺 UDP 无损传输。
- 中文提交说明。修改后构建并执行 CTest，硬件与 ARM 未实测不能宣称通过。不使用 friend，不新增假想的设备注册框架。
- 当前为本地仓库，未建立远端，未确定本仓许可证/版权署名。激光器新主体接口尚未推送，CMake 固定相邻本地 RayshiningDevice 提交；HandleCore/SqafDevice 使用远端完整提交。发布时再切换远端激光器地址。
