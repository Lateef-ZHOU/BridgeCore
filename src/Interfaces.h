#pragma once
#include <algorithm>
#include <cstring>
#include <memory>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <PcapInterfaces.h>
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
    if (getifaddrs(&raw) != 0) return {};
    const std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> addresses(raw, freeifaddrs);
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
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
        }
        if (fd >= 0) {
            ifreq request{};
            const auto name = interface.pcapDeviceName.toLocal8Bit();
            if (name.size() < IFNAMSIZ) {
                std::memcpy(request.ifr_name, name.constData(), static_cast<size_t>(name.size()));
                if (ioctl(fd, SIOCGIFMTU, &request) == 0) item.mtu = request.ifr_mtu;
            }
        }
        result.append(std::move(item));
    }
    if (fd >= 0) close(fd);
    return result;
}
