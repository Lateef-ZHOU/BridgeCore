// Link-time C API substitutes exercise the production main without opening hardware.
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <QCoreApplication>
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
            std::fprintf(stderr, "TEST: exit signal queued before exec\n");
        }
    }
    return exitSignalFd;
}
Q_COREAPP_STARTUP_FUNCTION(initializeFixture)
}
