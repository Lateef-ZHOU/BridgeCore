import os
import signal
import subprocess
import sys
import time

exe = sys.argv[1]
# Keep Qt diagnostics in captured stderr regardless of the host's journald logging backend.
os.environ["QT_FORCE_STDERR_LOGGING"] = "1"
listed = subprocess.run([exe, "--list-iface"], capture_output=True, text=True, check=True)
src = next((line.split("\t")[1].split(",")[0] for line in listed.stdout.splitlines()
            if len(line.split("\t")) >= 2 and line.split("\t")[1]), None)
if not src:
    print("No Ethernet IPv4 available for CLI resolution; skipping process test")
    sys.exit(77)
base = [exe, "run", "--serialport", "/test-only", "--src", src,
        "--srcPort", "2000", "--dst", "192.0.2.2", "--dstPort", "2001"]
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
print("20 process checks passed: both devices, single/combined exit signals, read diagnostics, signal setup and initial open failure")
