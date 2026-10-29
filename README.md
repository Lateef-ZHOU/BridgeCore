# BridgeCore

Linux 串口 ↔ UDP 桥接器，可执行文件名 `bridge`。Qt Core 事件循环常驻，无 UI；一进程接一台串口设备，多个设备分别启动多个进程并使用不同 UDP 端口。

串口收发、校验、组帧、断线重连和同步关闭由已有 Handle 完成。桥接器只选择协议、连接两端并转发字节；不包含设备控制页面、采集任务、图表、业务轮询或出光命令。

## 构建

需要 C++20、CMake ≥3.28、Qt Core ≥6.8.3、libserialport ≥0.1.1、libpcap ≥1.10.0。Boost.CircularBuffer ≥1.88 优先从系统查找，否则按固定 SHA256 下载。无需 Qt Quick/QML。

当前源码及新激光器接口只在本地，尚未发布：把 `BridgeCore` 和包含提交 `1d5e1a3cdfbab3e7d06c983aa242ce71de6f2333` 的 `RayshiningDevice` Git 仓库放在同一父目录。CMake 从该仓固定提交获取协议头；其他依赖从 GitHub 固定提交获取。源码目录覆盖可使用标准 `FETCHCONTENT_SOURCE_DIR_WHLS` 参数，不能覆盖为旧版完整帧发送接口。

```sh
cmake -S BridgeCore -B BridgeCore/build -DCMAKE_BUILD_TYPE=Release
cmake --build BridgeCore/build -j
ctest --test-dir BridgeCore/build --output-on-failure
```

运行所需动态库由系统提供。使用自建 libserialport 时，配置 `PKG_CONFIG_PATH`，并确保运行时能找到该动态库。当前验证平台为 Linux x86_64；源代码没有限定 x86 指令，但 ARM 交叉编译和实机尚未验证。仓库许可证和公开发布尚未确定；依赖各自许可保持不变。

## 查询与启动

```sh
./BridgeCore/build/bridge --list-serialport
./BridgeCore/build/bridge --list-iface
./BridgeCore/build/bridge --list-serialdevice

./BridgeCore/build/bridge run --device whlsDevice \
  --serialport /dev/ttyUSB0 \
  --src 192.168.1.102 --srcPort 57568 \
  --dst 192.168.1.10 --dstPort 57568
```

将示例地址换成实际配置。`--src` 必须已配置在本机以太网接口上，程序据此选卡，`--list-iface` 会列出地址和 MTU。对端与本机应处于同一以太网，程序直接向 `--dst` 发 ARP，不做路由/网关选择。

串口用户需要设备读写权限；Pcap 打开和注入需要相应网络权限。可由部署方为运行账户和可执行程序配置权限，例如对构建出的程序授予 `cap_net_raw,cap_net_admin`；也可首次验证时以具备这些权限的账户运行。程序不自动提权或修改系统设置。

`sqafDevice` 固定 9600/8N1/无流控；`whlsDevice` 固定 115200/8N1/无流控。连接与 `--list-serialdevice` 显示的串口规格均从对应官邸构参取得，列表不另写一套规格，CLI 不重复提供线路设置。

`Bridge::start()` 返回 true 只表示两侧接受了异步启动请求，不表示串口或 UDP 已连接就绪。打开结果由 Handle 后续报告；若第二侧拒绝启动，调用者返回并析构 Bridge，关闭先前已接受启动的那一侧。此处不增加回滚状态或另一条关闭链。

前台按 Ctrl+C 停止；后台可 `kill -TERM <PID>`，systemd 可直接用 SIGTERM 停止。信号经主线程事件循环处理，然后同步关闭并等待两个 Handle。正常信号退出码 0；启动/连接失败为 1；参数错误为 2（Qt 自带选项解析错误可能为 1）。后续 I/O 失败仍由 Handle 自动重连，不另建重试状态机。

## UDP 载荷

UDP 一个数据报对应 Handle 一次 send；串口完整校验帧对应一个上行数据报。没有附加桥接器消息头、时间戳或业务解码。

两个方向都由接收 worker 排队，再在主线程调用对端 send。UDP 接收交出的 Pcap 视图借用抓包缓冲，排队前深拷贝；当前两种串口成帧器交出自有 `QByteArray`，排队按值捕获、通过隐式共享保活，不额外复制字节。以后接入的串口成帧器也必须满足这一所有权契约。

| 类型 | UDP → 串口 | 串口 → UDP |
|---|---|---|
| whlsDevice | 地址 2 + 命令 1 + 脉宽号 1 + 长度 2 + 数据 N | 相同主体格式 |
| sqafDevice | cmd1、cmd2、data1、data2，共 4 字节 | 当前 PelcoDHandle 交付的完整 7 字节帧 |

激光器地址和长度保持大端，数据区不改字节序。LaserHandle 为下行补 WHLS、CRC16 和 0x55；上行检查后移除这层封套。**不要向 UDP 发送完整 WHLS 串口帧**。视强下行由 PelcoDHandle 补帧头、固定设备地址和校验和，上行接口本轮未改。

桥接器不替客户端理解查询、启动、停止等业务。Windows 上位机后续需要 UDP 接入适配，不能直接把完整串口报文送给这个主体接口；本轮未替换其现有直连串口页面。

边界沿用现有 Pcap：普通 Ethernet/IPv4，不处理 VLAN、非以太网链路头或 IP 分片。上行主体超过接口 MTU−28 或 UDP 上限时记日志并丢弃，不截断、不添加私有分片协议。首次 ARP 未完成时 Pcap 只保留最新待发包；断连、退出和 UDP 网络本身均不提供可靠交付保证，不新增排空或离线缓存。

## 验证

`tests` 链接串口/Pcap 的 C API 替身，生产 Handle 和桥接逻辑不替换。覆盖两型号双向字节、半帧/粘包/坏帧、pcap 缓冲复用、主线程发送、销毁前在途回调、超长拒绝、ARP、CLI 与实际进程的 SIGINT/SIGTERM/首开失败。CLI 枚举读取系统信息，但测试不打开实物串口/网卡；不发送真实设备指令。实际结果见 [VALIDATION.md](VALIDATION.md)。
