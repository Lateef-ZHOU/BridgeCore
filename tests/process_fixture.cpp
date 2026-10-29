// Link-time C API substitutes exercise the production main without opening hardware.
#include <cstdio>
#include <cstdlib>
#include <QCoreApplication>
#include "FakeSerialPort.h"
#include "FakePcap.h"

namespace {
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
        std::fprintf(stderr, "TEST: all transport resources released\n");
    });
}
Q_COREAPP_STARTUP_FUNCTION(initializeFixture)
}
