#pragma once
#include <cerrno>
#include <cstring>
#include <memory>
#include <utility>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <PcapInterfaces.h>
#include <QDebug>
#include <QStringList>

struct LocalInterface
{
    NetworkInterfaceInfo interface;
    QStringList addresses;
    int mtu = 0;
};

inline QList<LocalInterface> localInterfaces()
{
    ifaddrs *raw = nullptr;
    if (getifaddrs(&raw) != 0) {
        const int error = errno;
        qWarning().noquote() << "Cannot enumerate local interface addresses (getifaddrs): errno="
                             + QString::number(error) << std::strerror(error);
        return {};
    }
    const std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> addresses(raw, freeifaddrs);
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        const int error = errno;
        qWarning().noquote() << "Cannot create interface MTU query socket: errno="
                             + QString::number(error) << std::strerror(error);
    }
    QList<LocalInterface> result;
    for (const auto &interface : availableInterfaces()) {
        LocalInterface item{interface, {}, 0};
        for (const auto *entry = addresses.get(); entry; entry = entry->ifa_next) {
            if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET
                || QString::fromLocal8Bit(entry->ifa_name) != interface.pcapDeviceName)
                continue;
            char text[INET_ADDRSTRLEN]{};
            const auto *address = reinterpret_cast<const sockaddr_in *>(entry->ifa_addr);
            if (inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text)))
                item.addresses.append(QString::fromLatin1(text));
            else {
                const int error = errno;
                qWarning().noquote() << "Cannot format IPv4 address for" << interface.pcapDeviceName
                                    << "(inet_ntop): errno=" + QString::number(error) << std::strerror(error);
            }
        }
        if (fd >= 0) {
            ifreq request{};
            const auto name = interface.pcapDeviceName.toLocal8Bit();
            if (name.size() < IFNAMSIZ) {
                std::memcpy(request.ifr_name, name.constData(), static_cast<size_t>(name.size()));
                if (ioctl(fd, SIOCGIFMTU, &request) == 0) item.mtu = request.ifr_mtu;
                else {
                    const int error = errno;
                    qWarning().noquote() << "Cannot query MTU for" << interface.pcapDeviceName
                                        << "(SIOCGIFMTU): errno=" + QString::number(error) << std::strerror(error);
                }
            }
            else
                qWarning().noquote() << "Interface name is too long for MTU query:"
                                    << interface.pcapDeviceName << "(" << name.size()
                                    << "bytes; limit" << IFNAMSIZ - 1 << ")";
        }
        result.append(std::move(item));
    }
    if (fd >= 0) close(fd);
    return result;
}
