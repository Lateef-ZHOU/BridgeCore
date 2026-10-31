import subprocess
import sys

exe = sys.argv[1]

def check(args, status, fragment):
    result = subprocess.run([exe, *args], capture_output=True, text=True, timeout=5)
    assert result.returncode == status, (args, result.returncode, result.stderr)
    assert fragment in result.stdout + result.stderr, (args, result.stdout, result.stderr)

check(["--help"], 0, "--srcPort")
check(["--list-serialdevice"], 0,
      "sqafDevice\tPelco-D\t9600 baud, data=8, parity=None, stop=1\n"
      "whlsDevice\tWHLS\t115200 baud, data=8, parity=None, stop=1\n")
check(["--list-serialport"], 0, "")
check(["--list-iface"], 0, "")
check([], 2, "Expected run")
check(["run"], 2, "Missing --device")
check(["--list-iface", "--device", "whlsDevice"], 2, "List commands")
check(["--list-iface", "--list-serialport"], 2, "exactly one")
base = ["run", "--device", "whlsDevice", "--serialport", "/does-not-exist", "--iface", "nonexistent-test-iface", "--src", "192.0.2.123", "--srcPort", "2000", "--dst", "192.0.2.124", "--dstPort", "2001"]
for option, value, message in [("--device", "wrong", "Unsupported"), ("--src", "256.0.0.1", "Invalid --src"), ("--src", "0.0.0.0", "Invalid --src"), ("--dst", "0.0.0.0", "Invalid --dst"), ("--dst", "224.0.0.1", "Invalid --dst"), ("--srcPort", "0", "Invalid --srcPort"), ("--dstPort", "65536", "Invalid --dstPort")]:
    args = base.copy()
    args[args.index(option) + 1] = value
    check(args, 2, message)
dma = ["run", "--device", "cdl5Device", "--iface", "nonexistent-test-iface", "--src", "192.0.2.123", "--srcPort", "55200", "--dst", "192.0.2.124", "--dstPort", "55200"]
check(dma, 2, "requires --dstMac")
check(dma + ["--serialport", "/dev/ttyUSB0"], 2, "does not use --serialport")
check(dma + ["--dstMac", "02:00:00:00:00:02"], 2, "Unknown --iface")
check(["--list-iface", "--dma-device", "/fake"], 2, "List commands")
check(["run", "--device", "whlsDevice"], 2, "Missing --iface")
check(["--list-iface", "--iface", "eth0"], 2, "List commands")
check(base, 2, "Unknown --iface")
print("22 CLI checks passed (no hardware opened)")
