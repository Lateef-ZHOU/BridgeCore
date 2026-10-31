# 验证记录

## 2026-10-31：显式网卡与双实例板卡部署

- --iface 必填，按 --list-iface 的接口名选卡；--src 不要求属于系统网卡的 IPv4 配置（用户定），只检查现有地址格式和端口约束。网卡 MTU 和 DMA 突发 MAC 校验保留。枚举地址缺失不再导致 run 拒绝启动。
- ArchHFNL 严格警告 Release 增量构建通过，CTest 4/4。CLI 22项覆盖缺网卡、未知网卡和列表参数冲突；进程32项使用未配置的文档网段源IPv4，保留收发替身与SIGINT/SIGTERM退出验证；注入 inet_ntop 失败后无显示IPv4仍可运行。未操作测试主机的真实设备收发。
- 同一固定依赖经既有 ARM64 SDK / GCC12 / QtCore6.8.3 / 板卡glibc sysroot 重新构建。新bridge SHA256 `4b77f39e791b4f47ba73eff240eeca2a154481cad149306e503ea15348795982`。在板卡先停原laser-bridge再替换程序；实际打开eth0、激光器ttyUSB0和只读DMA设备成功。
- laser-bridge与cdl5-bridge分别使用55100/55200，参数均为 --iface eth0、源192.168.1.11、对端192.168.1.102。DMA目标MAC核对Windows网卡与板端ARP表为14:4f:d7:c7:d0:8c。重复start的PID不变；停止/重启DMA不影响激光器，重启激光器不影响DMA。最终时点PID分别7630/7600；/proc显示DMA只读fd归新桥接。旧采集服务及健康监控退出，原硬件watchdog进程735仍在。
- 为保留板卡既有ADC AC耦合初始化，S11cdl33-service不删除，S13cdl5-bridge接管前调用旧服务stop；运行中不保留双DMA占用。真实活动配置核查为Profile8/5kHz/450×128，区别于早期4k描述。本轮没有下发采集命令，也未重新验证Windows端到端数据。
- 离线重封装逐项比对：image.ub中内核和设备树字节相同，原cpio路径及元数据全部相同，仅增加CDL5的init.d与rc5/0/1/6五个链接；root镜像只替换bridge、激光器脚本及配置，增加CDL5脚本和配置。mkimage保留原FIT的unit-address警告，不改变布局。
- 更新前SD备份 `codex_bridge_dma_backup_20261031`，含两个镜像、两个SHA256和读卡器恢复说明。新image.ub SHA256 `7ada2da72a37711bdc4656cfac0d6cfef4686d65f7c22b4f3db1767fd7956fbc`；root-home.tar.gz `9f54e4b02e46c199df84db109a71de24db8a0a0dd556f82aabbd77d3143790f3`。先写临时文件校验再替换，sync后再次校验通过；BOOT.BIN/boot.scr哈希保持。eMMC科学数据未操作。
- 自启动入口已持久化，**本轮未重启，不宣称重启验证通过**。也未改变内核、驱动、运行库或主程序。无GitHub推送、无新Release。
- 证据：本机 `build-linux-validation/deploy-20261031/{install.log,runtime-check.log,persist.log,output/verification.json}`；ArchHFNL `~/.cache/bridge-fixed-20261029/build-iface-final.log`、`build/Testing/Temporary/LastTest.log`、`~/.cache/bridge-dma-arm-20261031/build-iface-final.log`、`~/board-bridge-dma-20261031/pack-final.log`。镜像含机器配置与密钥，只保存在忽略目录/私有备份，不入Git。

## 2026-10-31：CDL5 DMA 接入

- 新增 --device cdl5Device、可选 --dma-device 和 DMA 必填 --dstMac；串口枚举仍只列原两个串口型号。Bridge 模板接受具体端点类型，不新增中转对象、分块缓存或尺寸状态。
- ArchHFNL x86_64 / GCC16.2.1 / Qt6.12 / libpcap1.11，Release严格警告构建通过，CTest 4/4：原转发56项、CLI19项、进程退出/信号故障32项保持通过，新增DMA转发测试。
- 新测试使用生产 Cdl5DmaHandle、Bridge 和 PcapHandle，底层 DMA/MMIO/Pcap 为替身。500×128 和450×128帧分别2001/1801包，1024字节块及32字节尾块全部原样到达；UDP发送都在主线程，关闭后无新增发送。桥接器不解析帧头/meta。
- ARM64 已有SDK容器 / GCC12 / QtCore6.8.3 / 板卡glibc sysroot 构建新bridge和测试通过；tests/build-arm.sh记录入口。板卡 /run/bridge-dma-tests-arm64 实际运行退出0，模拟两种尺寸完整转发通过。
- 新bridge仅临时复制到 /run/bridge-arm64，--help 和 --list-serialdevice 运行退出0，帮助已列出CDL5。没有执行真实DMA run、安装新服务或替换 /root/apps/bridge/bin/bridge。
- SIGINT/SIGTERM → signalfd/QSocketNotifier → app.exec返回 → 先关闭设备Handle、再关闭UDP 的链路保持；DMA自身负责帧边界取消及超时恢复，不新增Bridge轮询或保存排空。
- 原生验证前期采用cdl5源码覆盖联调；随后删除该覆盖，从相邻 Git 仓库经 FetchContent 重新取出完整固定提交 `53eb8c1ba3b7ed00adfc0e674c047674d14e7a23`，核对实际HEAD，构建及CTest 4/4再次通过。该仓经本地Git bundle同步，尚未发布远端。ARM构建从相同源码挂载，不宣称远端拉取或真实采集成功。
- 证据：ArchHFNL ~/.cache/bridge-fixed-20261029/{configure-dma-pinned.log,build-dma-pinned.log,build/Testing/Temporary/LastTest.log}，~/.cache/bridge-dma-arm-20261031/build-final.log；本机cdl5Device/out/arm-board-final.txt。现有板卡服务、镜像、FPGA和eMMC均未更改。

## 2026-10-30：ARM 激光器桥接服务与 Windows UDP 实测

- 复用前轮已安装的 ARM64 bridge：SHA256 6e799f2160b606d8b2a6b02265e781a27655406496784422870cf8e5cde68c39，二进制未改。板卡内核 5.4.0-xilinx-v2020.1、PID 1 为 SysVinit，使用 SysV 启停脚本，未套用 systemd。locale 已由前轮系统镜像设置为 C.UTF-8，服务继续读取 /etc/default/locale。
- 用户拔除温控，剩余 USB 1-1.4 为激光器，当前 /dev/ttyUSB0；配置固定该 by-path，启动解析为当时实际 tty 名。不开其他串口，不改相机或温控。相同无 SN 芯片只能按物理插口区分，不宣称设备换口后自动识别。
- 板端 192.168.1.11:55100 ↔ Windows 192.168.1.102:55100。Windows 生产 LaserUdpModule / PcapHandle 真实链路自动取得脉宽四档、型号 1550-ACI-MP、SN 15500300MN260011 和连续测量；关闭停表/清空通过。测试只查询及既有断开关光，不发送出光。默认 CI 仍用替身。
- 服务 start 重复调用不创建第二实例，stop 经 SIGTERM 等待既有同步关闭，重复 stop 成功，重新 start/status 通过；最终运行 PID 7856（时点记录）。原 cdl33-service 仍 ready PID 771。没有引入重启循环、业务查询或强杀兜底。
- 离线系统镜像只添加 init.d 和 rc5/rc0/rc1/rc6 的五个符号链接；内核、设备树及所有原路径/元数据逐项未变。root 镜像保留旧文件与元数据，仅增加服务/配置及目录。写 SD 前保存四个原文件和校验，替换后 SHA256 校验通过；BOOT.BIN/boot.scr/bridge 二进制原哈希保持。备份在 SD 的 codex_laser_service_backup_20261030，内附 RESTORE.txt；未触碰 eMMC 科学数据。
- 新 image.ub SHA256：8cfca162a0374edcc5867c547e56d0fec762f3863f204ce91896cb4ede11e9e2；root-home.tar.gz：08696ae0534b68c57cecd0f73fcf3c764001b8bcf26fcef044ebb145407a552c。启动项持久化已检查，本轮未重启，不宣称重启实测通过。
- 证据：build-board-audit-20261030/laser-service-20261030/ 下的 install.log、output/verification.json、root-verification.json 及新旧镜像；Windows RayshiningDevice/build/live-udp.log。mkimage 沿用旧 FIT 的 unit-address 命名警告，未增加签名或改变 FIT 布局。无 C++ 变更，本轮不重复之前 103 项替身测试。

## 2026-10-30：网卡诊断与信号读取恢复验证

- 本地地址枚举、MTU 查询 socket 创建、IPv4 文本转换和 MTU 读取失败时记录操作、errno 数值及系统错误文字，单接口失败附接口名称；名称超出 MTU 查询结构限制也明确记录。原空列表、缺失地址和 MTU=0 返回方式保持，不改变启动校验。
- 通过现有进程测试链接入口注入上述四类系统调用失败，分别检查列表命令和 run 的具体诊断、原返回码与资源释放，共新增 8 项检查。未修改 HandleCore 或其他依赖。
- 两种串口型号分别注入一次 EINTR 和 EAGAIN：EINTR 保留待读信号并检查当前回调内完成重试；EAGAIN 先消耗旧信号，再通过主线程排队动作发送新的 SIGTERM，确认返回事件循环继续等待并正常退出。故障控制状态仅存在于测试替身，生产信号处理代码未改。
- ArchHFNL Linux Release 增量构建通过，`-Wall -Wextra -Wpedantic -Werror` 无警告；CTest 3/3，共 103 项检查：56 项转发/生命周期、15 项 CLI、32 项进程检查。未操作真实串口或网卡收发，未进行 ARM 验证。
- 双向转发、worker finished 退出、信号屏蔽及关闭顺序保持。证据：远端 `~/.cache/bridge-fixed-20261029/build-interface-signal-recovery.log`、`test-interface-signal-recovery.log`；本地副本在 `build-linux-validation/`。

## 2026-10-29：退出尾声与读取诊断整理

- 删除 previousMask 和 restoreMask，不再保存、恢复旧信号屏蔽；SIGINT/SIGTERM 的屏蔽保持到进程结束，避免另一个尚未读取的退出信号在关闭尾声触发默认终止。Bridge 关闭、notifier 销毁和 fd 关闭顺序保持，双向转发与 worker finished 退出接线未改，无新增业务状态或转发层。
- 信号描述符读取失败时记录 errno 数值和系统错误文字，异常长度时记录实际/预期字节数；原 EINTR 重试和 EAGAIN 返回逻辑保持。
- 两型号分别在事件循环前注入 SIGINT 和 SIGTERM，并先确认二者同时处于 pending；程序正常返回 0，传输资源和信号描述符均释放。测试链接入口额外注入 EBADF 与短读，验证返回 1 并记录具体诊断；生产代码不提供测试开关。
- ArchHFNL Linux Release 增量构建无警告；CTest 3/3，共 91 项检查：56 项转发/生命周期、15 项 CLI、20 项进程检查。保留单信号、运行中退出、通知创建失败和两侧首开失败验证。未操作真实设备，未进行 ARM 验证。
- 证据：远端 `~/.cache/bridge-fixed-20261029/build-pending-signals.log`、`test-pending-signals.log`；本地副本在 `build-linux-validation/`。下方“恢复原信号屏蔽”的记录属于此前版本，现已由本轮替代。

## 2026-10-29：退出信号改为事件通知

- 删除 50 ms QTimer、全局退出标记和信号处理函数；Linux signalfd 配合 QSocketNotifier 在主线程处理 SIGINT/SIGTERM。屏蔽在创建 worker 前设置，由新线程继承；循环前到达的信号由内核保留至 notifier 处理。
- 未修改 Bridge 双向转发、任一 worker finished 触发退出及串口先于 UDP 的同步关闭顺序。通知资源为 run 的局部资源：Bridge 先析构，再销毁 notifier、关闭 fd、恢复原信号屏蔽；未新增业务状态或转发层。
- 增加实际进程测试：两型号分别在 exec 前注入 SIGINT/SIGTERM，正常退出并释放资源；注入 signalfd 创建失败，明确返回失败。原运行中信号退出与两侧首开失败继续通过；测试同时检查信号描述符已关闭。信号故障注入只在测试链接入口实现，不在生产代码中设置测试开关。
- ArchHFNL Linux Release 增量构建无警告；CTest 3/3，共 85 项检查：56 项转发/生命周期、15 项 CLI、14 项进程检查。初次验证因主机 Qt 日志未进入捕获的 stderr 导致故障文案断言失败，测试指定 QT_FORCE_STDERR_LOGGING 后通过；生产日志策略未改。未进行实物或 ARM 验证。
- 证据：远端 `~/.cache/bridge-fixed-20261029/build-signal-notifier.log`、`test-signal-notifier.log`；本地副本在 `build-linux-validation/`。下方保留的 50 ms 轮询记录是此前版本的验证，不再描述当前实现。

## 2026-10-29：返回路径与双向排队销毁验证

- run 直接返回 app.exec()，局部 Bridge 仍在函数返回前析构；Interfaces.h 删除未使用的 algorithm，补齐 std::move 的 utility 直接依赖。
- 两型号均在 UDP → 串口及串口 → UDP 回调排队后销毁 Bridge；等待串口完成读帧，但不处理主线程事件，确认上行尚未发送。检查析构期间及销毁后处理事件均无新增 UDP 发送，串口也没有新增写入，两侧资源全部释放。
- 线程切换、关闭顺序、50 ms 轮询及现有退出机制均未改，不新增状态或转发层。
- ArchHFNL 固定依赖 Linux Release 增量构建通过，`-Wall -Wextra -Wpedantic -Werror` 无警告；CTest 3/3，共 79 项检查：56 项转发/生命周期、15 项 CLI、8 项进程检查。收发仍用 C API 替身，未进行实物或 ARM 验证。
- 证据：远端 `~/.cache/bridge-fixed-20261029/build-queued-destruction.log`、`test-queued-destruction.log`；本地副本在 `build-linux-validation/`。

## 2026-10-29：转发所有权与串口规格整理

- 串口 → UDP 按值捕获成帧器交出的自有 QByteArray，隐式共享保活；UDP → 串口保留对 Pcap 借用视图的深拷贝。未改变排队线程、发送线程、重连或关闭顺序。
- 设备列表从 SqafConfig::serialParameters / LaserHandle::parameters 取得规格，与连接共用构参。补清 start 的 true 只代表接受异步启动请求，并不代表连接完成。
- 在上述固定依赖的 Linux Release 构建目录增量构建通过，`-Wall -Wextra -Wpedantic -Werror` 无警告。CTest 3/3，共 71 项检查：48 项转发/生命周期、15 项 CLI、8 项进程验证。
- 加强串口投递测试：暂停主线程事件处理，等串口读帧结束后再投递，验证局部帧释放后两型号仍交付正确字节；保留 Pcap 缓冲复用、主线程发送、队列销毁、首开失败和 SIGINT/SIGTERM 退出验证。CLI 检查两型号完整串口规格，生产枚举输出与预期一致。
- 证据：远端 `~/.cache/bridge-fixed-20261029/build-copy-config.log`、`test-copy-config.log`；本地副本在 `build-linux-validation/`。收发使用 C API 替身，未操作真实硬件，未进行 ARM 验证。

## 2026-10-29：Linux 首版桥接器

- ArchHFNL，Linux x86_64 / GCC 16.2.1 / Qt 6.12.0 / libserialport 0.1.2 / libpcap 1.11.0；Release `-Wall -Wextra -Wpedantic -Werror` 构建通过。动态依赖检查只有 Qt Core，没有 QML、Quick 或 DeviceCore。
- 两次构建：先用源码目录覆盖联调；再在全新目录从远端固定获取 HandleCore `3470a1d9f3826f4a666b0e53c29c001f0901fbbd`、SqafDevice `9f2e4fdfbcaee661c60ec17330a736596a6dac71`，从本地 Git bundle 镜像固定获取未发布激光器 `1d5e1a3cdfbab3e7d06c983aa242ce71de6f2333`。逐项核对实际依赖 HEAD，全新构建通过。
- CTest 3/3、共 67 项检查：44 项双向转发/生命周期，15 项 CLI，8 项独立进程退出/失败。覆盖主线程发送、抓包视图复用后的字节所有权、串口半帧/粘包/坏帧重同步、一帧一数据报、未知/残缺指令拒绝、超 MTU 丢弃、ARP 学习、队列未交付时析构、两种型号 SIGINT/SIGTERM 和两侧首次打开失败。两个传输都释放后才退出。
- 系统枚举命令正常列出真实网卡/IPv4/MTU与两个型号；串口与 Pcap 实际收发测试使用 C API 替身，生产 Handle 和主程序代码不替换。未打开真实设备、未进行激光操作，未验证真实串口↔网口链路或 ARM 平台。
- 首轮编译发现 CLI 选项的嵌套列表初始化有重载歧义，改为显式单个名称形式。进程故障注入首次因测试全局初始化顺序未生效，改为 QCoreApplication 启动钩子初始化；生产代码未为测试增加环境变量或状态。
- 远端证据：`~/.cache/bridge-fixed-20261029/configure.log`、`build.log`、`test.log`、`build/Testing/Temporary/LastTest.log`；本地副本位于忽略目录 `build-linux-validation/`。首轮联调证据在 `~/.cache/bridge-20261029/`。

当前本地独立仓库，无 GitHub 远端，未确定本仓许可证；未改 HandleCore 的重连/关闭策略，未发布 Release。部署和载荷边界见 README；首个硬件及 ARM 验证待后续。
