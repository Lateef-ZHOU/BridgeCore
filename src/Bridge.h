#pragma once
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QObject>
#include <PcapHandle.h>

// One device protocol endpoint and one Ethernet UDP endpoint per process.
// Workers only enqueue owned bytes. All peer sends run on the owner thread.
template <typename Endpoint>
class Bridge final : public QObject
{
public:
    explicit Bridge(int maxUdpPayload)
        : maxUdpPayload_(maxUdpPayload), udp_(makeUdpInput(this)), endpoint_(makeEndpointInput(this))
    {
        connect(&udp_, &PcapHandle::logMessage, this,
                [](const QString &message) { qInfo().noquote() << "UDP:" << message; });
        connect(&endpoint_, &Endpoint::logMessage, this,
                [](const QString &message) { qInfo().noquote() << "Device:" << message; });
        // First-open failure exits the Handle thread; later I/O failures reconnect there.
        // Normal shutdown happens after exec returns, with no further event dispatch.
        connect(&udp_, &QThread::finished, this, [] { QCoreApplication::exit(1); });
        connect(&endpoint_, &QThread::finished, this, [] { QCoreApplication::exit(1); });
    }
    ~Bridge() override
    {
        // Both callback sources stop before either peer or the QObject context dies.
        endpoint_.stop();
        udp_.stop();
    }
    // True means both asynchronous start requests were accepted, not that either side is ready.
    // A rejection does not roll back an accepted first request; the caller destroys this bridge.
    [[nodiscard]] bool start(const typename Endpoint::ParamType &parameters, const PcapParams &udp)
    {
        return udp_.start(udp) && endpoint_.start(parameters);
    }

private:
    static DataCallback makeUdpInput(Bridge *self)
    {
        return [self](const QByteArray &bytes, RxTime) {
            // Pcap supplies a borrowed view, so a shallow QByteArray copy is insufficient.
            QMetaObject::invokeMethod(self,
                [self, owned = QByteArray(bytes.constData(), bytes.size())] {
                    (void)self->endpoint_.send(owned);
                }, Qt::QueuedConnection);
        };
    }
    static DataCallback makeEndpointInput(Bridge *self)
    {
        return [self](const QByteArray &bytes, RxTime) {
            // The supported framers and DMA reader deliver owning arrays; retain their shared storage.
            QMetaObject::invokeMethod(self,
                [self, owned = bytes] {
                    if (owned.size() > self->maxUdpPayload_) {
                        qWarning() << "Device payload exceeds interface UDP limit:" << owned.size()
                                   << ">" << self->maxUdpPayload_ << "bytes; dropped";
                        return;
                    }
                    (void)self->udp_.send(owned);
                }, Qt::QueuedConnection);
        };
    }
    const int maxUdpPayload_;
    PcapHandle udp_;
    Endpoint endpoint_;
};
