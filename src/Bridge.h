#pragma once
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QObject>
#include <PcapHandle.h>
#include <SerialHandle.h>

// One serial protocol endpoint and one Ethernet UDP endpoint per process.
// Workers only enqueue owned bytes. All peer sends run on the owner thread.
template <typename Serial>
class Bridge final : public QObject
{
public:
    explicit Bridge(int maxUdpPayload)
        : maxUdpPayload_(maxUdpPayload), udp_(makeUdpInput(this)), serial_(makeSerialInput(this))
    {
        connect(&udp_, &PcapHandle::logMessage, this,
                [](const QString &message) { qInfo().noquote() << "UDP:" << message; });
        connect(&serial_, &Serial::logMessage, this,
                [](const QString &message) { qInfo().noquote() << "Serial:" << message; });
        // First-open failure exits the Handle thread; later I/O failures reconnect there.
        // Normal shutdown happens after exec returns, with no further event dispatch.
        connect(&udp_, &QThread::finished, this, [] { QCoreApplication::exit(1); });
        connect(&serial_, &QThread::finished, this, [] { QCoreApplication::exit(1); });
    }
    ~Bridge() override
    {
        // Both callback sources stop before either peer or the QObject context dies.
        serial_.stop();
        udp_.stop();
    }
    [[nodiscard]] bool start(const SerialParams &serial, const PcapParams &udp)
    {
        return udp_.start(udp) && serial_.start(serial);
    }

private:
    static DataCallback makeUdpInput(Bridge *self)
    {
        return [self](const QByteArray &bytes, RxTime) {
            // Pcap supplies a borrowed view, so a shallow QByteArray copy is insufficient.
            QMetaObject::invokeMethod(self,
                [self, owned = QByteArray(bytes.constData(), bytes.size())] {
                    (void)self->serial_.send(owned);
                }, Qt::QueuedConnection);
        };
    }
    static DataCallback makeSerialInput(Bridge *self)
    {
        return [self](const QByteArray &bytes, RxTime) {
            QMetaObject::invokeMethod(self,
                [self, owned = QByteArray(bytes.constData(), bytes.size())] {
                    if (owned.size() > self->maxUdpPayload_) {
                        qWarning() << "Serial payload exceeds interface UDP limit:" << owned.size()
                                   << ">" << self->maxUdpPayload_ << "bytes; dropped";
                        return;
                    }
                    (void)self->udp_.send(owned);
                }, Qt::QueuedConnection);
        };
    }
    const int maxUdpPayload_;
    PcapHandle udp_;
    Serial serial_;
};
