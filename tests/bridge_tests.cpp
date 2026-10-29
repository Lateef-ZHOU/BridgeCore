#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <Device/Handle/PelcoDHandle.h>
#include <Device/GMB/Sqaf/SqafConfig.h>
#include <Rayshining/LaserHandle.h>
#include "Bridge.h"
#include "FakeSerialPort.h"
#include "FakePcap.h"

namespace {
int checks = 0;
void require(bool ok, const char *message)
{
    ++checks;
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
template <typename F> void until(F condition, bool pump = true)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!condition() && std::chrono::steady_clock::now() < end) {
        if (pump) QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    require(condition(), "timed out");
}
PcapParams params()
{
    bool ok = false;
    PcapParams p;
    p.host = {"test", "test", "test0", MAC::fromString("02:00:00:00:00:01", ok)};
    p.hostIPv4 = IPv4::fromString("192.0.2.1", ok);
    p.deviceIPv4 = IPv4::fromString("192.0.2.2", ok);
    p.hostPort = 2000; p.devicePort = 2001;
    p.deviceMAC = MAC::fromString("02:00:00:00:00:02", ok);
    return p;
}
QByteArray inbound(const PcapParams &p, const QByteArray &body)
{
    return UDPDatagram(p.host.macAddress, p.deviceMAC, p.deviceIPv4, p.hostIPv4,
                       p.devicePort, p.hostPort, body);
}
template <typename Serial>
void transfer(const SerialParams &serial, const QByteArray &down, const QByteArray &wire,
              const QByteArray &up, int baud)
{
    FakePcap::reset(); FakeSerialPort::reset();
    const auto p = params();
    auto bridge = std::make_unique<Bridge<Serial>>(1472);
    require(bridge->start(serial, p), "start bridge");
    until([] { return FakeSerialPort::reads() > 0 && FakePcap::last(); });
    const auto session = FakePcap::last();
    require(FakeSerialPort::baudRate() == baud, "model serial settings reused");
    FakePcap::inject(session, inbound(p, down));
    until([&] { return session->delivered == 1; }, false);
    require(FakeSerialPort::writes().empty(), "worker must not directly send to serial");
    until([] { return FakeSerialPort::writes().size() == 1; });
    require(FakeSerialPort::writes().back() == wire, "UDP body framed exactly once after pcap buffer reused");
    FakePcap::inject(session, inbound(p, "bad"));
    FakePcap::inject(session, inbound(p, down));
    until([] { return FakeSerialPort::writes().size() == 2; });
    require(FakeSerialPort::writes().back() == wire, "bad command rejected, next command works");
    const auto reads = FakeSerialPort::reads();
    FakeSerialPort::enqueue(wire.first(3));
    until([&] { return FakeSerialPort::reads() > reads; });
    require(FakePcap::sent().empty(), "partial serial frame not forwarded");
    auto bad = wire; bad[bad.size() - 2] ^= 1;
    const auto beforeCompleteRead = FakeSerialPort::reads();
    FakeSerialPort::enqueue(wire.sliced(3) + bad + wire);
    // Let the serial read/feed finish and destroy its local frames before main-thread delivery.
    until([&] { return FakeSerialPort::reads() >= beforeCompleteRead + 2; }, false);
    require(FakePcap::sent().empty(), "serial worker must only queue UDP forwarding");
    until([] { return FakePcap::sent().size() == 2; });
    for (const auto &send : FakePcap::sent()) {
        require(send.thread == QCoreApplication::instance()->thread(), "UDP sends on main thread");
        require(UDPDatagram(send.bytes).bodyView() == up, "one valid serial frame per UDP payload");
    }
    // Queued forwarding must not outlive its bridge or run during synchronous teardown.
    const auto delivered = session->delivered.load();
    FakePcap::inject(session, inbound(p, down));
    until([&] { return session->delivered > delivered; }, false);
    bridge.reset();
    const auto writes = FakeSerialPort::writes().size();
    QCoreApplication::processEvents();
    require(FakeSerialPort::writes().size() == writes, "queued forwarding removed with context");
    require(session->closed && FakeSerialPort::livePorts() == 0, "both sources stopped and released");
}
void limitAndArp()
{
    FakePcap::reset(); FakeSerialPort::reset();
    auto p = params(); const auto peer = p.deviceMAC; p.deviceMAC = {};
    Bridge<Rayshining::LaserHandle> bridge(1472);
    require(bridge.start(Rayshining::LaserHandle::parameters({"test", {}}), p), "ARP bridge start");
    until([] { return FakeSerialPort::reads() > 0 && FakePcap::last(); });
    const auto session = FakePcap::last();
    const auto body = Rayshining::makeBody(0, 0xb1, 0, {});
    FakeSerialPort::enqueue(Rayshining::encode(Rayshining::makeBody(0, 0xb1, 0, QByteArray(1500, 'x')))
                            + Rayshining::encode(body));
    until([] { return FakePcap::sent().size() == 1; });
    require(kindOf(FakePcap::sent().front().bytes) == FrameKind::ARPRequest, "oversize dropped and valid reply probes ARP");
    FakePcap::inject(session, ARPReply(ARPRequest(p.host.macAddress, p.hostIPv4, p.deviceIPv4), peer));
    until([] { return FakePcap::sent().size() == 2; });
    require(UDPDatagram(FakePcap::sent().back().bytes).bodyView() == body, "existing ARP learning flushes reply");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    transfer<PelcoDHandle>(SqafConfig::serialParameters({"test", {}}), QByteArray::fromHex("00510000"),
                          QByteArray::fromHex("ff010051000052"), QByteArray::fromHex("ff010051000052"), 9600);
    const auto body = QByteArray::fromHex("000036000000");
    transfer<Rayshining::LaserHandle>(Rayshining::LaserHandle::parameters({"test", {}}), body,
                                    QByteArray::fromHex("57484c53000036000000930f55"), body, 115200);
    limitAndArp();
    std::cout << checks << " forwarding/lifecycle checks passed\n";
}
