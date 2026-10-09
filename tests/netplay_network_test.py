#!/usr/bin/env python3
"""Launch independent NES peers over loopback TCP and compare complete states."""
import argparse
import json
import os
import socket
import struct
from pathlib import Path
import subprocess
import tempfile
import time


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("peer", type=Path)
    parser.add_argument("core", type=Path)
    parser.add_argument("rom", type=Path)
    parser.add_argument("artifacts", type=Path)
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in ("DEBUG", "TRACE", "TRACE_FILE", "INPUT", "VELOCE_TEST_OUT"):
        env.pop(key, None)

    def command(role, output, port_file, port=0, mismatch=False):
        return [str(args.peer.resolve()), role, str(args.core.resolve()), str(args.rom.resolve()),
                str(output.resolve()), str(port_file.resolve()), str(port), str(int(mismatch))]

    with tempfile.TemporaryDirectory(prefix="veloce-network-") as temporary:
        port_file = Path(temporary) / "port"
        baseline = subprocess.run(command("baseline", args.artifacts / "baseline", port_file), env=env,
                                  capture_output=True, text=True, timeout=30)
        require(baseline.returncode == 0, baseline.stdout + baseline.stderr)
        for name, packet, expected in (
                ("zero-length", struct.pack("!I", 0), "invalid message length"),
                ("oversized", struct.pack("!I", 65), "invalid message length"),
                ("unknown-type", struct.pack("!I", 1) + b"\x05", "unknown message type"),
                ("truncated", struct.pack("!I", 50) + b"\x01", "truncated message")):
            port_file.unlink(missing_ok=True)
            host = subprocess.Popen(command("host", args.artifacts / name, port_file), env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                deadline = time.monotonic() + 10
                while not port_file.exists() and host.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.01)
                require(port_file.exists(), f"{name}: listener did not start")
                with socket.create_connection(("127.0.0.1", int(port_file.read_text())), timeout=5) as raw:
                    # Receive the complete server hello, then fragment our
                    # deliberately invalid frame at every byte boundary.
                    hello = bytearray()
                    while len(hello) < 62:
                        chunk = raw.recv(62 - len(hello))
                        require(chunk, "server closed before sending hello")
                        hello.extend(chunk)
                    for byte in packet:
                        raw.sendall(bytes([byte]))
                        time.sleep(0.001)
                    raw.shutdown(socket.SHUT_WR)
                    while raw.recv(1024):
                        pass
                out, err = host.communicate(timeout=10)
                (args.artifacts / f"{name}.log").write_text(out + err)
                require(host.returncode == 1 and expected in err, f"{name}: {out}{err}")
            finally:
                if host.poll() is None:
                    host.kill()
                    host.communicate()
        for mismatch in (False, True):
            if port_file.exists():
                port_file.unlink()
            name = "mismatch" if mismatch else "network"
            host = subprocess.Popen(command("host", args.artifacts / f"{name}-host", port_file,
                                            mismatch=mismatch), env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, text=True)
            client = None
            try:
                deadline = time.monotonic() + 10
                while not port_file.exists() and host.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.01)
                if not port_file.exists():
                    if host.poll() is None:
                        host.kill()
                    out, err = host.communicate()
                    raise RuntimeError("host failed to create a live test listener: " + out + err)
                port = int(port_file.read_text().strip())
                require(0 < port <= 65535 and host.poll() is None, "host listener is not live")
                client = subprocess.Popen(command("client", args.artifacts / f"{name}-client", port_file,
                                                  port, mismatch), env=env, stdout=subprocess.PIPE,
                                          stderr=subprocess.PIPE, text=True)
                client_out, client_err = client.communicate(timeout=30)
                host_out, host_err = host.communicate(timeout=30)
                (args.artifacts / f"{name}.log").write_text(host_out + host_err + client_out + client_err)
                expected = 42 if mismatch else 0
                require(host.returncode == expected and client.returncode == expected,
                        f"{name} failed: {host_out}{host_err}{client_out}{client_err}")
                if mismatch:
                    require("EXPECTED_HANDSHAKE_REJECTED" in host_out and
                            "EXPECTED_HANDSHAKE_REJECTED" in client_out, "mismatch did not reject both peers")
            finally:
                for process in (client, host):
                    if process and process.poll() is None:
                        process.kill()
                        process.communicate()
        reference = (args.artifacts / "baseline.state").read_bytes()
        require(reference == (args.artifacts / "network-host.state").read_bytes() ==
                (args.artifacts / "network-client.state").read_bytes(), "independent TCP peers diverged")
        report = {"status": "pass", "console": "nes", "frames": 100,
                  "independent_processes": 2, "transport": "IPv4 TCP loopback",
                  "full_states_equal": True, "rom_mismatch_rejected": True,
                  "fragmented_invalid_frames_rejected": ["zero-length", "oversized", "unknown-type", "truncated"],
                  "state_size": len(reference)}
        (args.artifacts / "network.json").write_text(json.dumps(report, indent=2) + "\n")
        print("Network rollback: two independent TCP peers match complete NES baseline; ROM mismatch rejected")


if __name__ == "__main__":
    main()
