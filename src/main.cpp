#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <pthread.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QScopeGuard>
#include <QSocketNotifier>
#include <QTextStream>
#include <Device/Handle/PelcoDHandle.h>
#include <Device/GMB/Sqaf/SqafConfig.h>
#include <Rayshining/LaserHandle.h>
#include "Bridge.h"
#include "Interfaces.h"

namespace {
template <typename Serial>
int run(QCoreApplication &app, const SerialParams &serial, const PcapParams &udp, int maxPayload)
{
    // Workers inherit this mask; keep it until process exit, including synchronous teardown.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    const int maskError = pthread_sigmask(SIG_BLOCK, &signals, nullptr);
    if (maskError != 0) {
        qCritical() << "Cannot configure exit signals:" << std::strerror(maskError);
        return 1;
    }
    const int signalFd = signalfd(-1, &signals, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signalFd < 0) {
        qCritical() << "Cannot create exit signal descriptor:" << std::strerror(errno);
        return 1;
    }
    const auto closeSignal = qScopeGuard([signalFd] { close(signalFd); });
    QSocketNotifier notifier(signalFd, QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, &app, [signalFd, &app] {
        signalfd_siginfo info{};
        ssize_t count;
        do {
            count = read(signalFd, &info, sizeof(info));
        } while (count < 0 && errno == EINTR);
        if (count == static_cast<ssize_t>(sizeof(info))) app.quit();
        else if (count < 0 && errno == EAGAIN) return;
        else {
            if (count < 0) {
                const int error = errno;
                qCritical().noquote() << "Cannot read exit signal descriptor: errno="
                                     + QString::number(error) << std::strerror(error);
            }
            else
                qCritical() << "Unexpected exit signal descriptor read length:" << count
                            << "bytes; expected" << sizeof(info);
            app.exit(1);
        }
    });
    // Destroy the bridge before notifier/fd cleanup; no event dispatch during its stop/wait.
    Bridge<Serial> bridge(maxPayload);
    if (!bridge.start(serial, udp)) return 1;
    // Stack destruction synchronously closes both Handles before returning to the shell.
    return app.exec();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("bridge");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Framed serial <-> Ethernet UDP bridge (Linux)");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("run", "Start bidirectional forwarding", "[run]");
    parser.addOptions({
        {"list-serialport", "List serial ports"},
        {"list-iface", "List Ethernet interfaces and local IPv4 addresses"},
        {"list-serialdevice", "List supported serial device types"},
        {"device", "sqafDevice or whlsDevice", "type"},
        {"serialport", "Serial port, e.g. /dev/ttyUSB0", "path"},
        {"src", "Local IPv4 (selects its Ethernet interface)", "IPv4"},
        {"srcPort", "Local UDP port", "port"},
        {"dst", "Peer IPv4 on the same Ethernet network", "IPv4"},
        {"dstPort", "Peer UDP port", "port"}
    });
    parser.process(app);
    const auto reject = [](const QString &reason) {
        QTextStream(stderr) << reason << '\n';
        return 2;
    };
    const QStringList listOptions{"list-serialport", "list-iface", "list-serialdevice"};
    const QStringList runOptions{"device", "serialport", "src", "srcPort", "dst", "dstPort"};
    int lists = 0;
    for (const auto &option : listOptions) lists += parser.isSet(option);
    if (lists > 0) {
        if (lists != 1 || !parser.positionalArguments().isEmpty())
            return reject("Use exactly one list command without run arguments");
        for (const auto &option : runOptions)
            if (parser.isSet(option)) return reject("List commands do not accept run options");
        QTextStream output(stdout);
        if (parser.isSet("list-serialdevice")) {
            const auto describe = [&output](const char *device, const char *protocol, const SerialParams &params) {
                output << device << '\t' << protocol << '\t' << params.baudRate
                       << " baud, data=" << toString(params.dataBits)
                       << ", parity=" << toString(params.parity)
                       << ", stop=" << toString(params.stopBits) << '\n';
            };
            describe("sqafDevice", "Pelco-D", SqafConfig::serialParameters({}));
            describe("whlsDevice", "WHLS", Rayshining::LaserHandle::parameters({}));
        }
        else if (parser.isSet("list-serialport"))
            for (const auto &port : availablePorts()) output << port.portName << '\t' << port.description << '\n';
        else
            for (const auto &item : localInterfaces())
                output << item.interface.pcapDeviceName << '\t' << item.addresses.join(",")
                       << "\tMTU=" << item.mtu << '\n';
        return 0;
    }
    if (parser.positionalArguments() != QStringList{"run"}) return reject("Expected run or a list command; see --help");
    for (const auto &option : runOptions)
        if (!parser.isSet(option) || parser.value(option).isEmpty()) return reject("Missing --" + option);
    const auto type = parser.value("device");
    if (type != "sqafDevice" && type != "whlsDevice") return reject("Unsupported --device");
    bool valid = false;
    const auto src = IPv4::fromString(parser.value("src"), valid);
    if (!valid || src == IPv4{}) return reject("Invalid --src IPv4");
    const auto dst = IPv4::fromString(parser.value("dst"), valid);
    if (!valid || dst == IPv4{} || static_cast<unsigned char>(dst[0]) >= 224)
        return reject("Invalid --dst unicast IPv4");
    const auto srcPort = parser.value("srcPort").toUInt(&valid);
    if (!valid || srcPort == 0 || srcPort > 65535) return reject("Invalid --srcPort (1..65535)");
    const auto dstPort = parser.value("dstPort").toUInt(&valid);
    if (!valid || dstPort == 0 || dstPort > 65535) return reject("Invalid --dstPort (1..65535)");
    const auto interfaces = localInterfaces();
    const auto found = std::find_if(interfaces.begin(), interfaces.end(), [&](const auto &item) {
        return item.addresses.contains(src.toString());
    });
    if (found == interfaces.end() || found->mtu <= 28)
        return reject("--src must be assigned to an Ethernet interface with a valid MTU; see --list-iface");
    PcapParams udp;
    udp.host = found->interface;
    udp.hostIPv4 = src;
    udp.deviceIPv4 = dst;
    udp.hostPort = static_cast<quint16>(srcPort);
    udp.devicePort = static_cast<quint16>(dstPort);
    const SerialPortInfo port{parser.value("serialport"), {}};
    const int maxPayload = std::min(found->mtu - 28, 65507);
    if (type == "sqafDevice")
        return run<PelcoDHandle>(app, SqafConfig::serialParameters(port), udp, maxPayload);
    return run<Rayshining::LaserHandle>(app, Rayshining::LaserHandle::parameters(port), udp, maxPayload);
}
