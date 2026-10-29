# 验证记录

## 2026-10-29：Linux 首版桥接器

- ArchHFNL，Linux x86_64 / GCC 16.2.1 / Qt 6.12.0 / libserialport 0.1.2 / libpcap 1.11.0；Release `-Wall -Wextra -Wpedantic -Werror` 构建通过。动态依赖检查只有 Qt Core，没有 QML、Quick 或 DeviceCore。
- 两次构建：先用源码目录覆盖联调；再在全新目录从远端固定获取 HandleCore `3470a1d9f3826f4a666b0e53c29c001f0901fbbd`、SqafDevice `9f2e4fdfbcaee661c60ec17330a736596a6dac71`，从本地 Git bundle 镜像固定获取未发布激光器 `1d5e1a3cdfbab3e7d06c983aa242ce71de6f2333`。逐项核对实际依赖 HEAD，全新构建通过。
- CTest 3/3、共 67 项检查：44 项双向转发/生命周期，15 项 CLI，8 项独立进程退出/失败。覆盖主线程发送、抓包视图复用后的字节所有权、串口半帧/粘包/坏帧重同步、一帧一数据报、未知/残缺指令拒绝、超 MTU 丢弃、ARP 学习、队列未交付时析构、两种型号 SIGINT/SIGTERM 和两侧首次打开失败。两个传输都释放后才退出。
- 系统枚举命令正常列出真实网卡/IPv4/MTU与两个型号；串口与 Pcap 实际收发测试使用 C API 替身，生产 Handle 和主程序代码不替换。未打开真实设备、未进行激光操作，未验证真实串口↔网口链路或 ARM 平台。
- 首轮编译发现 CLI 选项的嵌套列表初始化有重载歧义，改为显式单个名称形式。进程故障注入首次因测试全局初始化顺序未生效，改为 QCoreApplication 启动钩子初始化；生产代码未为测试增加环境变量或状态。
- 远端证据：`~/.cache/bridge-fixed-20261029/configure.log`、`build.log`、`test.log`、`build/Testing/Temporary/LastTest.log`；本地副本位于忽略目录 `build-linux-validation/`。首轮联调证据在 `~/.cache/bridge-20261029/`。

当前本地独立仓库，无 GitHub 远端，未确定本仓许可证；未改 HandleCore 的重连/关闭策略，未发布 Release。部署和载荷边界见 README；首个硬件及 ARM 验证待后续。
