#include <DmaFixture.h>
#include "Bridge.h"
#include "FakePcap.h"

template<class F> void until(F ready)
{
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    while(!ready()&&std::chrono::steady_clock::now()<deadline) {
        QCoreApplication::processEvents();QThread::msleep(1);
    }
    check(ready(),"bridge DMA progress");
}
int main(int argc,char **argv)
{
    DmaFixture fixture;
    QCoreApplication app(argc,argv);
    for(int gates:{500,450,240}) {
        prepare(gates);FakePcap::reset();
        bool ok=false;
        PcapParams p;
        p.host={"test","test","test0",MAC::fromString("02:00:00:00:00:01",ok)};
        p.deviceMAC=MAC::fromString("02:00:00:00:00:02",ok);
        p.hostIPv4=IPv4::fromString("192.0.2.1",ok);p.deviceIPv4=IPv4::fromString("192.0.2.2",ok);
        p.hostPort=55200;p.devicePort=55200;
        auto bridge=std::make_unique<Bridge<Cdl5DmaHandle>>(1472);
        auto params=Cdl5DmaHandle::parameters("/fake/dma");params.memoryDevice="/fake/mem";
        check(bridge->start(params,p),"DMA bridge starts");
        until([] { return FakePcap::last()!=nullptr; });
        // Wait for both asynchronous connections, without adding production readiness state.
        QThread::msleep(30);
        const auto session=FakePcap::last();
        const auto command=Cdl5::command(1);
        FakePcap::inject(session,UDPDatagram(p.host.macAddress,p.deviceMAC,p.deviceIPv4,p.hostIPv4,
                                             p.devicePort,p.hostPort,command));
        until([&] { return FakePcap::sent().size()==static_cast<size_t>(gates==500?2001:1801); });
        QByteArray combined;
        for(const auto &packet:FakePcap::sent()) {
            check(packet.thread==app.thread(),"DMA to UDP sends only on owner thread");
            combined+=UDPDatagram(packet.bytes).bodyView();
        }
        check(combined==source,"whole 4k/5k/10k frame survives queued bridge unchanged");
        const auto count=FakePcap::sent().size();
        bridge.reset();app.processEvents();
        check(session->closed&&FakePcap::sent().size()==count,"closed context cannot send more UDP");
    }
    std::cout<<"PASS real DMA Handle -> queued Bridge -> Pcap send, 4k/5k/10k mocked hardware\n";
}
