import os
import errno
import signal
import subprocess
import sys
import time

exe = sys.argv[1]
# Keep Qt diagnostics in captured stderr regardless of the host's journald logging backend.
os.environ["QT_FORCE_STDERR_LOGGING"] = "1"
listed = subprocess.run([exe, "--list-iface"], capture_output=True, text=True, check=True)
interface = next((line.split("\t") for line in listed.stdout.splitlines()
                  if len(line.split("\t")) >= 2 and line.split("\t")[1]), None)
if not interface:
    print("No Ethernet IPv4 available for CLI resolution; skipping process test")
    sys.exit(77)
iface, src = interface[0], interface[1].split(",")[0]
# Pcap uses an explicit packet source address independent of the system's IPv4 configuration.
base = [exe, "run", "--serialport", "/test-only", "--iface", iface, "--src", "192.0.2.123",
        "--srcPort", "2000", "--dst", "192.0.2.2", "--dstPort", "2001"]
for failure, message, error in (
        ("BRIDGE_TEST_GETIFADDRS_FAIL", "Cannot enumerate local interface addresses (getifaddrs)", errno.ENOMEM),
        ("BRIDGE_TEST_SOCKET_FAIL", "Cannot create interface MTU query socket", errno.EMFILE),
        ("BRIDGE_TEST_MTU_FAIL", "Cannot query MTU for", errno.ENODEV),
        ("BRIDGE_TEST_INET_NTOP_FAIL", "Cannot format IPv4 address for", errno.ENOSPC)):
    env = {**os.environ, failure: "1"}
    result = subprocess.run([exe, "--list-iface"], env=env, capture_output=True, text=True, timeout=4)
    assert result.returncode == 0, (failure, result.stderr)
    assert message in result.stderr, (failure, result.stderr)
    assert f"errno={error}" in result.stderr and os.strerror(error) in result.stderr
    if failure == "BRIDGE_TEST_GETIFADDRS_FAIL":
        assert not result.stdout
    elif failure in ("BRIDGE_TEST_SOCKET_FAIL", "BRIDGE_TEST_MTU_FAIL"):
        assert src in result.stdout
        assert all(line.endswith("MTU=0") for line in result.stdout.splitlines())
    else:
        assert result.stdout and all(not line.split("\t")[1] for line in result.stdout.splitlines())
    assert "all transport resources released" in result.stderr
    # Missing MTU rejects startup; unavailable display addresses do not block raw Ethernet use.
    result = subprocess.run([*base, "--device", "whlsDevice"],
                            env={**env, "BRIDGE_TEST_EARLY_SIGNAL": "INT"},
                            capture_output=True, text=True, timeout=4)
    assert result.returncode == (0 if failure == "BRIDGE_TEST_INET_NTOP_FAIL" else 2), (failure, result.stderr)
    assert message in result.stderr
    if failure == "BRIDGE_TEST_GETIFADDRS_FAIL":
        assert "Unknown --iface" in result.stderr
    elif failure != "BRIDGE_TEST_INET_NTOP_FAIL":
        assert "--iface must have a valid MTU" in result.stderr
    else:
        assert "exit signal queued before exec" in result.stderr
    assert "all transport resources released" in result.stderr
for device in ("whlsDevice", "sqafDevice"):
    for sig in (signal.SIGINT, signal.SIGTERM):
        proc = subprocess.Popen([*base, "--device", device], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        time.sleep(0.25)
        assert proc.poll() is None, "bridge must stay in its event loop"
        proc.send_signal(sig)
        out, err = proc.communicate(timeout=4)
        assert proc.returncode == 0, (device, sig, out, err)
        assert "all transport resources released" in err
    for early in ("INT", "TERM", "BOTH"):
        result = subprocess.run([*base, "--device", device],
                                env={**os.environ, "BRIDGE_TEST_EARLY_SIGNAL": early},
                                capture_output=True, text=True, timeout=4)
        assert result.returncode == 0, (device, early, result.stderr)
        assert "exit signal queued before exec" in result.stderr
        if early == "BOTH":
            assert "SIGINT and SIGTERM pending before exec" in result.stderr
        assert "all transport resources released" in result.stderr
    for mode in ("EINTR", "EAGAIN"):
        result = subprocess.run([*base, "--device", device],
                                env={**os.environ, "BRIDGE_TEST_EARLY_SIGNAL": "INT",
                                     "BRIDGE_TEST_SIGNAL_READ": mode},
                                capture_output=True, text=True, timeout=4)
        assert result.returncode == 0, (device, mode, result.stderr)
        assert f"injected {mode}" in result.stderr
        assert f"signal read recovered after {mode}" in result.stderr
        if mode == "EAGAIN":
            assert "event loop resumed after EAGAIN" in result.stderr
        assert "Cannot read exit signal descriptor" not in result.stderr
        assert "all transport resources released" in result.stderr
    for mode, message in (("ERROR", "errno=9"),
                          ("SHORT", "Unexpected exit signal descriptor read length: 127 bytes; expected 128")):
        result = subprocess.run([*base, "--device", device],
                                env={**os.environ, "BRIDGE_TEST_EARLY_SIGNAL": "INT",
                                     "BRIDGE_TEST_SIGNAL_READ": mode},
                                capture_output=True, text=True, timeout=4)
        assert result.returncode == 1, (device, mode, result.stderr)
        assert message in result.stderr, (device, mode, result.stderr)
        assert "all transport resources released" in result.stderr
    result = subprocess.run([*base, "--device", device],
                            env={**os.environ, "BRIDGE_TEST_SIGNALFD_FAIL": "1"},
                            capture_output=True, text=True, timeout=4)
    assert result.returncode == 1, (device, result.stderr)
    assert "Cannot create exit signal descriptor" in result.stderr
    assert "all transport resources released" in result.stderr
    for failure in ("BRIDGE_TEST_SERIAL_FAIL", "BRIDGE_TEST_PCAP_FAIL"):
        result = subprocess.run([*base, "--device", device], env={**os.environ, failure: "1"},
                                capture_output=True, text=True, timeout=4)
        assert result.returncode == 1, (device, failure, result.stderr)
        assert "all transport resources released" in result.stderr
print("32 process checks passed: explicit interface with unassigned source IPv4; diagnostics, both devices, exit signals, read recovery, signal setup and initial open failure")
