import os
import signal
import subprocess
import sys
import time

exe = sys.argv[1]
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
    for failure in ("BRIDGE_TEST_SERIAL_FAIL", "BRIDGE_TEST_PCAP_FAIL"):
        result = subprocess.run([*base, "--device", device], env={**os.environ, failure: "1"},
                                capture_output=True, text=True, timeout=4)
        assert result.returncode == 1, (device, failure, result.stderr)
        assert "all transport resources released" in result.stderr
print("8 process checks passed: both devices, SIGINT/SIGTERM, initial open failure")
