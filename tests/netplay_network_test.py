#!/usr/bin/env python3
"""Launch independent core peers over loopback TCP and compare complete states."""
import argparse
import importlib.util
import re
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


def ctr_expected():
    # Independent arithmetic reference: supplied quantum, load/store offsets
    # and HID bit assignment, without importing guest/CPU stimulus helpers.
    quantum=100000; total=100*quantum; period=7
    values=[]
    for frame in range(100):
        buttons=(frame*7+3)&4095
        pressed=sum(1<<n for n,bit in enumerate((0,1,7,6,11,10,8,9,5,4,2,3)) if buttons&(1<<bit))
        values.append(4095-pressed)
    count=lambda store:(total-1-store)//period+1
    accumulated=sum(values[(9+period*k)//quantum] for k in range(count(11)))&0xffffffff
    last=values[(9+period*(count(12)-1))//quantum]
    return (accumulated,last,count(14))*3+(0,0,0)

def grade_ctr_registers(path):
    content=path.read_text();lines=content.splitlines()
    require(content.endswith("\n") and len(lines)==14 and re.fullmatch(r"#VELOCE NETWORK 1 core=3DS frames=100 role=(baseline|host|client)",lines[0]) and
            re.fullmatch(r"INFO rollbacks=\d+ replay_audio_chunks=\d+",lines[1]),"invalid 3DS register stream")
    expected=[f"REG address={4096+n*4} value={value}" for n,value in enumerate(ctr_expected())]
    require(lines[2:]==expected,"3DS guest register oracle mismatch")


def wiiu_expected():
    # Original guest publishes independent context triples and high-RAM readbacks.
    # Arithmetic uses supplied attempt positions; no CPU/builder helper is used.
    quantum=100000;total=100*quantum;period=11
    values=[(frame*7+3)&4095 for frame in range(100)]
    count=lambda store:(total-1-store)//period+1
    accumulated=sum(values[(10+period*k)//quantum] for k in range(count(12)))&0xffffffff
    last=values[(10+period*(count(13)-1))//quantum]
    far=values[(10+period*(count(19)-1))//quantum]&1
    return (accumulated,last,count(15))*3+(far,far,far)

def grade_wiiu_registers(path):
    content=path.read_text();lines=content.splitlines()
    require(content.endswith("\n") and len(lines)==14 and re.fullmatch(r"#VELOCE NETWORK 1 core=Wii U frames=100 role=(baseline|host|client)",lines[0]) and
            re.fullmatch(r"INFO rollbacks=\d+ replay_audio_chunks=\d+",lines[1]),"invalid Wii U register stream")
    expected=[f"REG address={4096+n*4} value={value}" for n,value in enumerate(wiiu_expected())]
    require(lines[2:]==expected,"Wii U guest register oracle mismatch")

def n64_expected():
    # Independent SI wire assignment; both two-player masks exercise 32 bits.
    virtual=(0,1,20,6,8,9,10,11,4,5,21,22,23,24)
    native=(15,14,13,12,11,10,9,8,5,4,3,2,1,0)
    values=[]
    for frame in range(100):
        row=[]
        for multiplier,offset in ((2246822519,0x13579bdf),(2654435761,0x2468ace1)):
            mask=(frame*multiplier+offset)&0xffffffff
            wire=sum(1<<b for a,b in zip(virtual,native) if mask&(1<<a))
            row.append(wire<<16)
        values.extend(row+[0,0,4096,0])
    return values

def grade_n64_registers(path):
    content=path.read_text();lines=content.splitlines()
    require(content.endswith("\n") and len(lines)==602 and re.fullmatch(r"#VELOCE NETWORK 1 core=N64 frames=100 role=(baseline|host|client)",lines[0]) and
            re.fullmatch(r"INFO rollbacks=\d+ replay_audio_chunks=\d+",lines[1]),"invalid N64 register stream")
    expected=[f"REG address={8192+n*4} value={value}" for n,value in enumerate(n64_expected())]
    require(lines[2:]==expected,"N64 guest register oracle mismatch")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("peer", type=Path)
    parser.add_argument("core", type=Path)
    parser.add_argument("rom", type=Path)
    parser.add_argument("artifacts", type=Path)
    parser.add_argument("--netplay-plugin", type=Path)
    parser.add_argument("--console",default="nes")
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in ("DEBUG", "TRACE", "TRACE_FILE", "INPUT", "VELOCE_TEST_OUT"):
        env.pop(key, None)

    def command(role, output, port_file, port=0, mismatch=False, rom_path=None):
        argv = [str(args.peer.resolve()), role, str(args.core.resolve()), str((rom_path or args.rom).resolve()),
                str(output.resolve()), str(port_file.resolve()), str(port), str(int(mismatch))]
        if args.netplay_plugin:
            argv.append(str(args.netplay_plugin.resolve()))
        return argv

    with tempfile.TemporaryDirectory(prefix="veloce-network-") as temporary:
        port_file = Path(temporary) / "port"
        baseline = subprocess.run(command("baseline", args.artifacts / "baseline", port_file), env=env,
                                  capture_output=True, text=True, timeout=30)
        require(baseline.returncode == 0, baseline.stdout + baseline.stderr)
        (args.artifacts / "baseline.log").write_text(baseline.stdout + baseline.stderr)
        if args.console in ("3ds","wiiu"):
            grade_registers=grade_ctr_registers if args.console=="3ds" else grade_wiiu_registers
            expected_values=ctr_expected if args.console=="3ds" else wiiu_expected
            platform="3DS" if args.console=="3ds" else "Wii U"
            grade_registers(args.artifacts / "baseline.result")
            spec=importlib.util.spec_from_file_location("ctr_probe_builder",args.rom.parent / "build.py")
            builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
            require(builder.build()==args.rom.read_bytes(),platform+" probe archive differs")
            mutant=Path(temporary)/("wrong-input"+args.rom.suffix);mutant.write_bytes(builder.build(wrong_input=True))
            control=subprocess.run(command("baseline",args.artifacts / "wrong-input",port_file,rom_path=mutant),env=env,capture_output=True,text=True,timeout=30)
            require(control.returncode==0,control.stdout+control.stderr)
            (args.artifacts / "wrong-input.log").write_text(control.stdout+control.stderr)
            try: grade_registers(args.artifacts / "wrong-input.result")
            except RuntimeError as error:
                require(str(error)==platform+" guest register oracle mismatch","control failed for unrelated reason")
                actual=(args.artifacts / "wrong-input.result").read_text().splitlines()[2:]
                expected=[f"REG address={4096+n*4} value={value}" for n,value in enumerate(expected_values())]
                require(len(actual)==12 and sum(a!=b for a,b in zip(actual,expected))==6,"wrong-input control domain changed")
            else: raise RuntimeError("executed original wrong-input control escaped")
            original=(args.artifacts / "baseline.result").read_text()
            records=original.splitlines()
            for field in range(12):
                altered=records.copy();line=altered[field+2];value=int(line.split(" value=")[1]);altered[field+2]=line.split(" value=")[0]+f" value={value^1}"
                path=args.artifacts / f"field-{field}.result";path.write_text("\n".join(altered)+"\n")
                try:grade_registers(path)
                except RuntimeError:pass
                else:raise RuntimeError("original register field escaped")
            for name,content in (("truncated",original.rsplit("\n",2)[0]+"\n"),("wrong-header",original.replace("frames=100","frames=99")),("trailing",original+"extra\n"),("unterminated",original.rstrip("\n"))):
                path=args.artifacts / f"stream-{name}.result";path.write_text(content)
                try:grade_registers(path)
                except RuntimeError:pass
                else:raise RuntimeError("invalid original register stream escaped")
        if args.console=="n64":
            grade_n64_registers(args.artifacts / "baseline.result")
            spec=importlib.util.spec_from_file_location("n64_probe_builder",args.rom.parent / "build.py")
            builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
            require(builder.build()==args.rom.read_bytes(),"N64 probe archive differs")
            mutant=Path(temporary)/"wrong-capture.z64";mutant.write_bytes(builder.build(wrong_capture=True))
            control=subprocess.run(command("baseline",args.artifacts / "wrong-capture",port_file,rom_path=mutant),env=env,capture_output=True,text=True,timeout=30)
            require(control.returncode==0,control.stdout+control.stderr)
            (args.artifacts / "wrong-capture.log").write_text(control.stdout+control.stderr)
            try:grade_n64_registers(args.artifacts / "wrong-capture.result")
            except RuntimeError as error:
                require(str(error)=="N64 guest register oracle mismatch","control failed for unrelated reason")
                actual=(args.artifacts / "wrong-capture.result").read_text().splitlines()[2:]
                expected=[f"REG address={8192+n*4} value={v}" for n,v in enumerate(n64_expected())]
                require(len(actual)==600 and sum(a!=b for a,b in zip(actual,expected))==100,"wrong-capture domain changed")
            else:raise RuntimeError("executed N64 control escaped")
            original=(args.artifacts / "baseline.result").read_text();records=original.splitlines()
            for field in range(6):
                altered=records.copy();line=altered[field+2];value=int(line.split(" value=")[1]);altered[field+2]=line.split(" value=")[0]+f" value={value^1}"
                path=args.artifacts / f"field-{field}.result";path.write_text("\n".join(altered)+"\n")
                try:grade_n64_registers(path)
                except RuntimeError:pass
                else:raise RuntimeError("N64 field mutation escaped")
            for name,content in (("truncated",original.rsplit("\n",2)[0]+"\n"),("wrong-header",original.replace("frames=100","frames=99")),("trailing",original+"extra\n"),("unterminated",original.rstrip("\n"))):
                path=args.artifacts / f"stream-{name}.result";path.write_text(content)
                try:grade_n64_registers(path)
                except RuntimeError:pass
                else:raise RuntimeError("invalid N64 register stream escaped")
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
        if args.console in ("nds","dsi"):
            # Original probe: eight setup attempts then fixed seven/nine-attempt
            # loops. Derive each published sum from the load's frame, not the
            # store's frame, including transfers crossing frame boundaries.
            keys=[]; xy=[]
            for frame in range(100):
                buttons=(frame*7+3)&4095
                pressed=sum(1<<n for n,bit in enumerate((0,1,7,6,11,10,8,9,5,4)) if buttons&(1<<bit))
                keys.append(1023-pressed); xy.append(0x7c|(3-((buttons>>2)&3)))
            def count(total,period,store): return (total-1-store)//period+1
            def accumulated(values,total,quantum,period,load,store):
                return sum(values[(load+period*k)//quantum] for k in range(count(total,period,store)))&0xffffffff
            def last(values,total,quantum,period,load,store):
                return values[(load+period*(count(total,period,store)-1))//quantum]
            want=(accumulated(keys,20000000,200000,7,8,10),last(keys,20000000,200000,7,8,11),count(20000000,7,13),0,
                  accumulated(keys,10000000,100000,9,8,10),last(keys,10000000,100000,9,8,11),
                  accumulated(xy,10000000,100000,9,12,14),last(xy,10000000,100000,9,12,15))
            for name in ("baseline","network-host","network-client"):
                lines=(args.artifacts/(name+".result")).read_text().splitlines()
                records=[line for line in lines if line.startswith("REG address=")]
                expected=[f"REG address={4096+n*4} value={value}" for n,value in enumerate(want)]
                require(records==expected,"DS guest register oracle mismatch: "+name)
        if args.console in ("3ds","wiiu"):
            for name in ("baseline","network-host","network-client"):grade_registers(args.artifacts / (name+".result"))
        if args.console=="n64":
            for name in ("baseline","network-host","network-client"):grade_n64_registers(args.artifacts / (name+".result"))
        report = {"status": "pass", "console": args.console, "frames": 100,
                  "independent_processes": 2, "transport": "IPv4 TCP loopback",
                  "full_states_equal": True, "rom_mismatch_rejected": True,
                  "fragmented_invalid_frames_rejected": ["zero-length", "oversized", "unknown-type", "truncated"],
                  "state_size": len(reference)}
        if args.netplay_plugin:
            report["production_netplay_plugin"] = True
            if args.console in ("nds","dsi"): report.update(controller_mode=f"one host-owned {args.console.upper()} keypad, joining viewer",guest_register_words=8,guest_registers_match_independent_oracle=True,wireless_link_emulation=False)
            if args.console in ("nds","dsi"): report["frontend_contract"]="fresh core after different prior histories; poll while paused; host owns keypad; viewer predicts/replays; disconnect on core replacement"
            if args.console not in ("nds","dsi"): report["frontend_contract"] = "fresh core after different prior histories; poll while paused; own one live frame; suppress replay audio; disconnect on core replacement"
        if args.console in ("3ds","wiiu"):
            report.update(controller_mode="one host-owned "+platform+" digital input latch, joining viewer",guest_register_words=12,
                          guest_registers_match_independent_oracle=True,wireless_link_emulation=False,
                          executed_wrong_input_rejected=True,wrong_input_mismatched_words=6,field_mutations_rejected=12,invalid_register_streams_rejected=4,
                          frontend_contract="fresh core after different prior histories; poll while paused; host owns keypad; viewer predicts/replays; disconnect on core replacement")
        if args.console=="n64":report.update(controller_mode="two independently owned digital pads; ports 2/3 neutral",guest_register_words=600,guest_registers_match_independent_oracle=True,executed_wrong_capture_rejected=True,wrong_capture_mismatched_words=100,field_mutations_rejected=6,invalid_register_streams_rejected=4,native_analog_emulation=False)
        if args.console=="wiiu":report.update(state_storage="canonical nonzero RAM pages; history slots grow to actual serialized size",native_gamepad_emulation=False)
        (args.artifacts / "network.json").write_text(json.dumps(report, indent=2) + "\n")
        print(f"Network rollback: two independent TCP peers match complete {args.console} baseline; ROM mismatch rejected")


if __name__ == "__main__":
    main()
