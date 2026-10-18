#!/usr/bin/env python3
"""ADB-side watcher for Xyron runtime collision/FPS monitor logs."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Iterable, Optional


DEFAULT_PACKAGE = "com.xyron.game"
DEFAULT_REMOTE_SUBDIR = "files/SAMP/xyron_monitor"


def run_adb(args: list[str], serial: Optional[str] = None, timeout: int = 20, binary: bool = False):
    command = ["adb"]
    if serial:
        command += ["-s", serial]
    command += args
    return subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
        text=not binary,
    )


def list_devices() -> list[str]:
    proc = run_adb(["devices"], timeout=20)
    devices: list[str] = []
    for line in proc.stdout.splitlines()[1:]:
        line = line.strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) >= 2 and parts[1] == "device":
            devices.append(parts[0])
    return devices


def choose_device(serial: Optional[str]) -> str:
    if serial:
        return serial
    devices = list_devices()
    if not devices:
        raise RuntimeError("Nenhum celular ADB conectado.")
    preferred = "192.168.3.155:39911"
    if preferred in devices:
        return preferred
    return devices[0]


def adb_cat(serial: str, remote_path: str) -> Optional[bytes]:
    proc = run_adb(["exec-out", "cat", remote_path], serial=serial, timeout=20, binary=True)
    if proc.returncode != 0:
        return None
    data = proc.stdout
    if not data:
        return None
    lowered = data[:160].lower()
    if lowered.startswith(b"cat: ") or b"no such file" in lowered or b"permission denied" in lowered:
        return None
    return data


def parse_json_lines(raw: bytes) -> list[dict]:
    rows: list[dict] = []
    for line in raw.decode("utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return rows


def event_key(event: dict) -> tuple:
    sample = event.get("sample") or {}
    native = sample.get("native") or {}
    return (
        event.get("wallTimeMs"),
        event.get("kind"),
        event.get("detail"),
        round(float(native.get("x", 0.0)), 2),
        round(float(native.get("y", 0.0)), 2),
        round(float(native.get("z", 0.0)), 2),
    )


def format_event(event: dict) -> str:
    sample = event.get("sample") or {}
    native = sample.get("native") or {}
    perf = sample.get("perf") or {}
    ts = event.get("wallTimeMs") or sample.get("wallTimeMs") or 0
    when = dt.datetime.fromtimestamp(ts / 1000).strftime("%Y-%m-%d %H:%M:%S") if ts else "sem-hora"
    probe = ""
    if native.get("sweptHit"):
        probe += f" swept={native.get('sweptModel')}:{native.get('sweptName')}@{float(native.get('sweptDistance', 0.0)):.2f}"
    if native.get("visualProbeHit"):
        probe += f" visual={native.get('visualProbeModel')}:{native.get('visualProbeName')} col={native.get('visualProbeUsesCollision')} data={native.get('visualProbeHasColData')}"
    if native.get("forwardHit"):
        probe += f" forward={native.get('forwardModel')}:{native.get('forwardName')}@{float(native.get('forwardDistance', 0.0)):.2f}"
    return (
        f"{when} | {event.get('severity', '?').upper()} | {event.get('kind', '?')} | "
        f"pos={float(native.get('x', 0.0)):.2f},{float(native.get('y', 0.0)):.2f},{float(native.get('z', 0.0)):.2f} "
        f"dz={float(native.get('dz', 0.0)):.2f} ground={float(native.get('groundZ', 0.0)):.2f} "
        f"veh={native.get('inVehicle')} model={native.get('vehicleModel')} "
        f"fps={perf.get('nativeFps', native.get('nativeFps', 0))}{probe} detail={event.get('detail', '')}"
    )


def write_report(output_dir: Path, events: Iterable[dict], latest: Optional[dict], serial: str, remote_dir: str) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Xyron runtime monitor",
        "",
        f"- Device: `{serial}`",
        f"- Remote: `{remote_dir}`",
        f"- Updated: `{dt.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}`",
        "",
    ]
    if latest:
        native = latest.get("native") or {}
        perf = latest.get("perf") or {}
        lines += [
            "## Latest",
            "",
            f"- Native ready: `{native.get('nativeReady')}`",
            f"- Position: `{float(native.get('x', 0.0)):.2f}, {float(native.get('y', 0.0)):.2f}, {float(native.get('z', 0.0)):.2f}`",
            f"- Ground/dz: `{float(native.get('groundZ', 0.0)):.2f} / {float(native.get('dz', 0.0)):.2f}`",
            f"- FPS: `{perf.get('nativeFps', native.get('nativeFps', 0))}`",
            f"- Swept probe: `hit={native.get('sweptHit')} model={native.get('sweptModel')} name={native.get('sweptName')} dist={float(native.get('sweptDistance', 0.0)):.2f}`",
            f"- Forward probe: `hit={native.get('forwardHit')} model={native.get('forwardModel')} name={native.get('forwardName')} dist={float(native.get('forwardDistance', 0.0)):.2f}`",
            f"- Visual path probe: `hit={native.get('visualProbeHit')} model={native.get('visualProbeModel')} name={native.get('visualProbeName')} usesCol={native.get('visualProbeUsesCollision')} colData={native.get('visualProbeHasColData')}`",
            "",
        ]
    else:
        lines += [
            "## Latest",
            "",
            "- `latest.json` ainda nao apareceu. Abra/entre no jogo para a Activity SAMP iniciar o monitor.",
            "",
        ]
    event_list = list(events)
    lines += ["## Events", ""]
    if event_list:
        for event in event_list[-80:]:
            lines.append("- " + format_event(event))
    else:
        lines.append("- Nenhum evento critico visto ainda.")
    (output_dir / "monitor_report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Watch Xyron runtime collision/FPS monitor over ADB.")
    parser.add_argument("--serial", help="ADB serial. Default picks connected device, preferring 192.168.3.155:39911.")
    parser.add_argument("--package", default=DEFAULT_PACKAGE)
    parser.add_argument("--output", default="")
    parser.add_argument("--interval", type=float, default=2.0)
    parser.add_argument("--duration", type=float, default=0.0, help="Seconds to run. 0 means forever.")
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args(argv)

    serial = choose_device(args.serial)
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    output_dir = Path(args.output) if args.output else Path("cerebro") / "runtime-monitor" / stamp
    output_dir.mkdir(parents=True, exist_ok=True)
    remote_dir = f"/sdcard/Android/data/{args.package}/{DEFAULT_REMOTE_SUBDIR}"
    latest_path = f"{remote_dir}/latest.json"
    events_path = f"{remote_dir}/events.jsonl"
    samples_path = f"{remote_dir}/samples.jsonl"

    print(f"[runtime-monitor] device={serial}")
    print(f"[runtime-monitor] remote={remote_dir}")
    print(f"[runtime-monitor] output={output_dir.resolve()}")

    seen: set[tuple] = set()
    all_events: list[dict] = []
    started = time.monotonic()

    while True:
        latest: Optional[dict] = None
        latest_raw = adb_cat(serial, latest_path)
        if latest_raw:
            (output_dir / "latest.json").write_bytes(latest_raw)
            try:
                latest = json.loads(latest_raw.decode("utf-8", errors="replace"))
            except json.JSONDecodeError:
                latest = None

        events_raw = adb_cat(serial, events_path)
        if events_raw:
            (output_dir / "events.jsonl").write_bytes(events_raw)
            events = parse_json_lines(events_raw)
            for event in events:
                key = event_key(event)
                if key in seen:
                    continue
                seen.add(key)
                all_events.append(event)
                print("[event] " + format_event(event))

        samples_raw = adb_cat(serial, samples_path)
        if samples_raw:
            (output_dir / "samples.jsonl").write_bytes(samples_raw)

        write_report(output_dir, all_events, latest, serial, remote_dir)

        if args.once:
            break
        if args.duration > 0 and time.monotonic() - started >= args.duration:
            break
        time.sleep(max(0.5, args.interval))

    print(f"[runtime-monitor] done. report={output_dir / 'monitor_report.md'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
