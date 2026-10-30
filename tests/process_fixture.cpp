// Link-time C API substitutes exercise the production main without opening hardware.
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <QCoreApplication>
#include <QMetaObject>
#include "FakeSerialPort.h"
#include "FakePcap.h"

namespace {
int exitSignalFd = -1;
void initializeFixture()
{
    FakeSerialPort::reset();
    FakePcap::reset();
    if (std::getenv("BRIDGE_TEST_SERIAL_FAIL")) FakeSerialPort::failOpen(true);
    if (std::getenv("BRIDGE_TEST_PCAP_FAIL")) FakePcap::failStage = 1;
    std::atexit([] {
        if (FakeSerialPort::livePorts() != 0) std::abort();
        for (const auto &session : FakePcap::sessions)
            if (!session->closed) std::abort();
        if (exitSignalFd >= 0 && (fcntl(exitSignalFd, F_GETFD) != -1 || errno != EBADF)) std::abort();
        std::fprintf(stderr, "TEST: all transport resources released\n");
    });
}

extern "C" int __real_signalfd(int fd, const sigset_t *mask, int flags);
extern "C" int __wrap_signalfd(int fd, const sigset_t *mask, int flags)
{
    if (std::getenv("BRIDGE_TEST_SIGNALFD_FAIL")) {
        errno = EMFILE;
        return -1;
    }
    exitSignalFd = __real_signalfd(fd, mask, flags);
    if (exitSignalFd >= 0) {
        if (const char *early = std::getenv("BRIDGE_TEST_EARLY_SIGNAL")) {
            // Production has blocked signals, but has not created its notifier or started workers/exec.
            const int signal = early[0] == 'I' ? SIGINT : SIGTERM;
            if (kill(getpid(), signal) != 0) std::abort();
            if (early[0] == 'B') {
                if (kill(getpid(), SIGINT) != 0) std::abort();
                sigset_t pending;
                if (sigpending(&pending) != 0 || sigismember(&pending, SIGINT) != 1
                    || sigismember(&pending, SIGTERM) != 1) std::abort();
                std::fprintf(stderr, "TEST: SIGINT and SIGTERM pending before exec\n");
            }
            std::fprintf(stderr, "TEST: exit signal queued before exec\n");
        }
    }
    return exitSignalFd;
}

extern "C" ssize_t __real_read(int fd, void *buffer, size_t count);
extern "C" ssize_t __wrap_read(int fd, void *buffer, size_t count)
{
    if (fd == exitSignalFd) {
        if (const char *mode = std::getenv("BRIDGE_TEST_SIGNAL_READ")) {
            if (std::strcmp(mode, "ERROR") == 0) {
                errno = EBADF;
                return -1;
            }
            if (std::strcmp(mode, "SHORT") == 0)
                return static_cast<ssize_t>(sizeof(signalfd_siginfo)) - 1;
            // Test-only fault state; production keeps no retry or exit flags.
            static bool injected = false;
            static bool retried = false;
            const bool interrupted = std::strcmp(mode, "EINTR") == 0;
            if (!injected) {
                injected = true;
                if (interrupted) {
                    // Returning to the event loop instead of retrying read must fail this test.
                    QMetaObject::invokeMethod(QCoreApplication::instance(), [] {
                        if (!retried) std::abort();
                    }, Qt::QueuedConnection);
                }
                else {
                    // Consume the first signal to make EAGAIN real on subsequent reads.
                    if (__real_read(fd, buffer, count) != static_cast<ssize_t>(sizeof(signalfd_siginfo)))
                        std::abort();
                    QMetaObject::invokeMethod(QCoreApplication::instance(), [] {
                        std::fprintf(stderr, "TEST: event loop resumed after EAGAIN\n");
                        if (kill(getpid(), SIGTERM) != 0) std::abort();
                    }, Qt::QueuedConnection);
                }
                std::fprintf(stderr, "TEST: injected %s\n", mode);
                errno = interrupted ? EINTR : EAGAIN;
                return -1;
            }
            const auto result = __real_read(fd, buffer, count);
            if (result == static_cast<ssize_t>(sizeof(signalfd_siginfo))) {
                retried = true;
                std::fprintf(stderr, "TEST: signal read recovered after %s\n", mode);
            }
            return result;
        }
    }
    return __real_read(fd, buffer, count);
}

extern "C" int __real_getifaddrs(ifaddrs **addresses);
extern "C" int __wrap_getifaddrs(ifaddrs **addresses)
{
    if (std::getenv("BRIDGE_TEST_GETIFADDRS_FAIL")) {
        errno = ENOMEM;
        return -1;
    }
    return __real_getifaddrs(addresses);
}

extern "C" int __real_socket(int domain, int type, int protocol);
extern "C" int __wrap_socket(int domain, int type, int protocol)
{
    if (std::getenv("BRIDGE_TEST_SOCKET_FAIL")) {
        errno = EMFILE;
        return -1;
    }
    return __real_socket(domain, type, protocol);
}

extern "C" int __real_ioctl(int fd, unsigned long request, ...);
extern "C" int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list args;
    va_start(args, request);
    auto *argument = va_arg(args, ifreq *);
    va_end(args);
    if (std::getenv("BRIDGE_TEST_MTU_FAIL")) {
        errno = ENODEV;
        return -1;
    }
    return __real_ioctl(fd, request, argument);
}

extern "C" const char *__real_inet_ntop(int family, const void *address, char *text, socklen_t size);
extern "C" const char *__wrap_inet_ntop(int family, const void *address, char *text, socklen_t size)
{
    if (std::getenv("BRIDGE_TEST_INET_NTOP_FAIL")) {
        errno = ENOSPC;
        return nullptr;
    }
    return __real_inet_ntop(family, address, text, size);
}
Q_COREAPP_STARTUP_FUNCTION(initializeFixture)
}
