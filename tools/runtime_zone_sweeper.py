#!/usr/bin/env python3
"""Drive the APK through map scan points and record runtime collision evidence."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional


DEFAULT_PACKAGE = "com.xyron.game"
DEFAULT_REMOTE_MONITOR = "files/SAMP/xyron_monitor"
BAD_EVENT_KINDS = {
    "fall_or_missing_floor",
    "floating_invisible_support",
    "collision_flags_bad",
    "object_pass_through_suspect",
    "object_collision_probe_bad",
}


def run_adb(args: list[str], serial: Optional[str] = None, timeout: int = 30, binary: bool = False):
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
    devices = []
    for line in proc.stdout.splitlines()[1:]:
        parts = line.strip().split()
        if len(parts) >= 2 and parts[1] == "device":
            devices.append(parts[0])
    return devices


def choose_device(serial: Optional[str]) -> str:
    if serial:
        return serial
    devices = list_devices()
    if not devices:
        raise RuntimeError("No ADB device connected.")
    preferred = "192.168.3.155:43349"
    return preferred if preferred in devices else devices[0]


def adb_cat(serial: str, remote_path: str) -> Optional[bytes]:
    proc = run_adb(["exec-out", "cat", remote_path], serial=serial, timeout=20, binary=True)
    if proc.returncode != 0 or not proc.stdout:
        return None
    data = proc.stdout
    lowered = data[:160].lower()
    if lowered.startswith(b"cat: ") or b"no such file" in lowered:
        return None
    return data


def parse_jsonl(raw: Optional[bytes]) -> list[dict]:
    if not raw:
        return []
    rows: list[dict] = []
    for line in raw.decode("utf-8", errors="replace").splitlines():
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return rows


def parse_latest(raw: Optional[bytes]) -> Optional[dict]:
    if not raw:
        return None
    try:
        return json.loads(raw.decode("utf-8", errors="replace"))
    except json.JSONDecodeError:
        return None


def remote_monitor_paths(package: str) -> tuple[str, str, str]:
    root = f"/sdcard/Android/data/{package}/{DEFAULT_REMOTE_MONITOR}"
    return (f"{root}/latest.json", f"{root}/events.jsonl", f"{root}/samples.jsonl")


def remote_probe_results_path(package: str) -> str:
    return f"/sdcard/Android/data/{package}/{DEFAULT_REMOTE_MONITOR}/map_probe_results.jsonl"


def send_runtime_command(serial: str, package: str, command: str) -> None:
    remote_dir = f"/sdcard/Android/data/{package}/{DEFAULT_REMOTE_MONITOR}"
    remote_file = f"{remote_dir}/runtime_command.txt"
    remote_tmp = f"{remote_file}.tmp"
    shell_command = (
        f"mkdir -p {remote_dir} && rm -f {remote_file} {remote_tmp} && "
        f"printf '%s\\n' {shlex.quote(command)} > {remote_tmp} && "
        f"(chmod 666 {remote_tmp} 2>/dev/null || true) && mv -f {remote_tmp} {remote_file}"
    )
    proc = run_adb(["shell", shell_command], serial=serial, timeout=20)
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or proc.stdout.strip() or "adb command-file write failed")


def start_game(serial: str, package: str, server: str) -> None:
    host, port_text = server.rsplit(":", 1)
    component = f"{package}/.launcher.EntryActivity"
    run_adb(
        [
            "shell",
            "am",
            "start",
            "-W",
            "-n",
            component,
            "--ez",
            "auto_play",
            "true",
            "--es",
            "server_ip",
            host,
            "--ei",
            "server_port",
            str(int(port_text)),
        ],
        serial=serial,
        timeout=30,
    )


def is_package_running(serial: str, package: str) -> bool:
    proc = run_adb(["shell", "pidof", package], serial=serial, timeout=10)
    return proc.returncode == 0 and bool(proc.stdout.strip())


def safe_float(value, default: float = 0.0) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return default
    return number if math.isfinite(number) else default


def safe_int(value, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def native_from_latest(latest: Optional[dict]) -> dict:
    if not latest:
        return {}
    native = latest.get("native") or {}
    return native if isinstance(native, dict) else {}


def latest_wall_time_ms(latest: Optional[dict]) -> int:
    return safe_int((latest or {}).get("wallTimeMs"), 0)


def read_latest(serial: str, latest_path: str) -> Optional[dict]:
    return parse_latest(adb_cat(serial, latest_path))


def refresh_latest_sample(serial: str, latest_path: str, current: Optional[dict]) -> Optional[dict]:
    refreshed = read_latest(serial, latest_path)
    if not refreshed:
        return current
    if not current or latest_wall_time_ms(refreshed) >= latest_wall_time_ms(current):
        return refreshed
    return current


def latest_distance_to_target(latest: Optional[dict], target_x: float, target_y: float) -> float:
    native = native_from_latest(latest)
    if not native.get("nativeReady"):
        return float("inf")
    return math.hypot(safe_float(native.get("x")) - target_x, safe_float(native.get("y")) - target_y)


def latest_is_near_target(latest: Optional[dict], target_x: float, target_y: float, near_radius: float) -> bool:
    return latest_distance_to_target(latest, target_x, target_y) <= near_radius


def latest_is_fresh_near_target(
    latest: Optional[dict],
    target_x: float,
    target_y: float,
    near_radius: float,
    after_wall_time_ms: int,
) -> bool:
    return latest_wall_time_ms(latest) > after_wall_time_ms and latest_is_near_target(
        latest,
        target_x,
        target_y,
        near_radius,
    )


def wait_for_latest_after(
    serial: str,
    latest_path: str,
    after_wall_time_ms: int,
    timeout: float,
) -> Optional[dict]:
    latest: Optional[dict] = None
    deadline = time.monotonic() + max(0.5, timeout)
    while time.monotonic() < deadline:
        latest = read_latest(serial, latest_path)
        if latest_wall_time_ms(latest) > after_wall_time_ms:
            return latest
        time.sleep(0.5)
    return latest


def wait_for_settled_target_sample(
    serial: str,
    latest_path: str,
    target_x: float,
    target_y: float,
    timeout: float,
    near_radius: float,
    after_wall_time_ms: int,
    max_abs_dz: float = 3.25,
) -> Optional[dict]:
    best_near: Optional[dict] = None
    deadline = time.monotonic() + max(1.0, timeout)
    while time.monotonic() < deadline:
        latest = read_latest(serial, latest_path)
        native = native_from_latest(latest)
        fresh_enough = latest_wall_time_ms(latest) > after_wall_time_ms
        if fresh_enough and native.get("nativeReady") and latest_is_near_target(latest, target_x, target_y, near_radius):
            best_near = latest
            z = safe_float(native.get("z"))
            dz = safe_float(native.get("dz"), 9999.0)
            vertical_z = safe_float(native.get("verticalZ"), -9999.0)
            vertical_gap = z - vertical_z if vertical_z > -9000.0 else 9999.0
            if z > -40.0 and bool(native.get("verticalHit")) and (
                abs(dz) <= max_abs_dz or abs(vertical_gap) <= max_abs_dz
            ):
                return latest
        time.sleep(0.5)
    return best_near


def event_native(event: dict) -> dict:
    sample = event.get("sample") or {}
    native = sample.get("native") or {}
    return native if isinstance(native, dict) else {}


def load_route(path: Path, status_filter: set[str]) -> list[dict]:
    if path.suffix.lower() == ".json":
        rows = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(rows, list):
            raise ValueError("Route JSON must contain a list.")
        route = [row for row in rows if not status_filter or str(row.get("status", "")).lower() in status_filter]
        return route
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    route = []
    for row in rows:
        status = str(row.get("status", "")).lower()
        if status_filter and status not in status_filter:
            continue
        route.append(
            {
                "zone_id": row.get("zone_id", ""),
                "status": status,
                "confidence": int(safe_float(row.get("confidence"), 0)),
                "x": safe_float(row.get("center_x")),
                "y": safe_float(row.get("center_y")),
                "z": 80.0,
                "reason": row.get("reason", ""),
            }
        )
    return route


def filter_empty_zones(route: list[dict], zone_csv: Optional[Path]) -> list[dict]:
    if not zone_csv:
        return route
    with zone_csv.open("r", encoding="utf-8", newline="") as handle:
        rows = {row.get("zone_id", ""): row for row in csv.DictReader(handle)}
    filtered: list[dict] = []
    for point in route:
        zone_id = str(point.get("zone_id", ""))
        meta = rows.get(zone_id, {})
        placements = safe_int(meta.get("placements"), 0)
        runtime_events = safe_int(meta.get("runtime_events"), 0)
        samples = safe_int(meta.get("samples"), 0)
        if placements > 0 or runtime_events > 0 or samples > 0:
            filtered.append(point)
    return filtered


def filter_probe_bounds(route: list[dict], bounds: Optional[list[float]]) -> list[dict]:
    if not bounds:
        return route
    xmin, ymin, xmax, ymax = bounds
    return [
        point
        for point in route
        if xmin <= safe_float(point.get("x")) <= xmax
        and ymin <= safe_float(point.get("y")) <= ymax
    ]


def build_area_route(area: list[float], step: float, z: float) -> list[dict]:
    xmin, ymin, xmax, ymax = area
    if xmax <= xmin or ymax <= ymin:
        raise ValueError("Invalid --area; expected xmin ymin xmax ymax")
    route = []
    iy = 0
    y = ymin + step * 0.5
    while y < ymax:
        ix = 0
        x = xmin + step * 0.5
        while x < xmax:
            route.append({"zone_id": f"A{ix:02d}_{iy:02d}", "status": "area", "x": x, "y": y, "z": z, "reason": "manual_area"})
            ix += 1
            x += step
        iy += 1
        y += step
    return route


def parse_point_spec(value: str, default_area: int = 0) -> tuple[float, float, float, int]:
    parts = [part.strip() for part in value.replace(";", ",").split(",") if part.strip()]
    if len(parts) < 3:
        raise ValueError("expected X,Y,Z[,area]")
    x = safe_float(parts[0])
    y = safe_float(parts[1])
    z = safe_float(parts[2])
    area = safe_int(parts[3], default_area) if len(parts) >= 4 else default_area
    return (x, y, z, max(0, min(255, area)))


def parse_tap_spec(value: str) -> Optional[tuple[int, int]]:
    cleaned = (value or "").strip().lower()
    if not cleaned or cleaned in {"none", "off", "false", "0"}:
        return None
    parts = [part.strip() for part in cleaned.replace(";", ",").split(",") if part.strip()]
    if len(parts) != 2:
        raise ValueError("expected X,Y for --spawn-tap")
    return (safe_int(parts[0]), safe_int(parts[1]))


def tap_spawn_button(serial: str, spawn_tap: Optional[tuple[int, int]], wait_seconds: float) -> None:
    if not spawn_tap:
        return
    x, y = spawn_tap
    run_adb(["shell", "input", "tap", str(x), str(y)], serial=serial, timeout=10)
    time.sleep(max(0.0, min(wait_seconds, 20.0)))


def classify_point(native: dict, events: list[dict], target_x: float, target_y: float, near_radius: float) -> tuple[str, int, str]:
    reasons: list[str] = []
    native_ready = bool(native.get("nativeReady"))
    if not native_ready:
        return ("red", 5, "native_not_ready")

    x = safe_float(native.get("x"))
    y = safe_float(native.get("y"))
    z = safe_float(native.get("z"))
    dz = safe_float(native.get("dz"))
    vertical_z = safe_float(native.get("verticalZ"), -9999.0)
    fps = safe_float(native.get("nativeFps"))
    vertical_hit = bool(native.get("verticalHit"))
    on_ground = bool(native.get("onGround"))
    distance = math.hypot(x - target_x, y - target_y)
    vertical_gap = z - vertical_z if vertical_z > -9000.0 else 9999.0
    stable_vertical_surface = z >= -40.0 and vertical_hit and abs(vertical_gap) <= 3.25
    stable_native_ground = z >= -40.0 and on_ground and abs(dz) <= 3.25
    stable_final_floor = z >= -40.0 and (
        stable_native_ground or (vertical_hit and (abs(dz) <= 3.25 or stable_vertical_surface))
    )
    bad_events = []
    transient_events = []
    for event in events:
        kind = str(event.get("kind", ""))
        if kind not in BAD_EVENT_KINDS:
            continue
        if kind in {"object_collision_probe_bad", "fall_or_missing_floor", "floating_invisible_support"} and stable_final_floor:
            transient_events.append(event)
            continue
        bad_events.append(event)

    status = "green"
    confidence = 85
    if distance > near_radius:
        status = "yellow"
        confidence -= 25
        reasons.append(f"not_near={distance:.1f}")
    if z < -40.0 or (
        not stable_vertical_surface
        and not stable_native_ground
        and (dz < -10.0 or not vertical_hit)
    ):
        status = "red"
        confidence = min(confidence, 35)
        reasons.append(f"floor_bad z={z:.1f} dz={dz:.1f} vertical={vertical_hit} onGround={on_ground}")
    elif dz > 3.25 and not stable_vertical_surface and not stable_native_ground and not native.get("inVehicle"):
        status = "yellow" if status != "red" else status
        confidence -= 20
        reasons.append(f"floating_dz={dz:.1f}")
    if fps and fps < 28:
        status = "yellow" if status != "red" else status
        confidence -= 15
        reasons.append(f"low_fps={fps:.0f}")
    if bad_events:
        status = "red"
        confidence = min(confidence, 30)
        reasons.append("events=" + ",".join(sorted({str(e.get("kind", "")) for e in bad_events})))
    elif transient_events:
        if status == "green" and any(str(e.get("kind", "")) != "object_collision_probe_bad" for e in transient_events):
            status = "yellow"
        confidence = min(confidence, 82)
        reasons.append("transient_event_recovered=" + ",".join(sorted({str(e.get("kind", "")) for e in transient_events})))
    if not reasons:
        reasons.append("runtime_clean")
    return (status, max(1, min(99, int(confidence))), "; ".join(reasons))


def classify_probe_result(native: dict, events: list[dict]) -> tuple[str, int, str]:
    if not native:
        return ("red", 5, "probe_missing")

    native_status = str(native.get("status", "") or "")
    native_ready = bool(native.get("nativeReady"))
    vertical_hit = bool(native.get("verticalHit"))
    collision_loaded = bool(native.get("collisionLoaded"))
    forced_full_pass = bool(native.get("forcedFullPass"))
    uses_collision = bool(native.get("usesCollision"))
    visible = bool(native.get("visible"))
    has_rw = bool(native.get("hasRw"))
    model = safe_int(native.get("model"), -1)
    vertical_z = safe_float(native.get("verticalZ"), -9999.0)
    ground_z = safe_float(native.get("groundZ"), -9999.0)
    dz = safe_float(native.get("dz"), -9999.0)
    bad_events = [event for event in events if str(event.get("kind", "")) in BAD_EVENT_KINDS]

    status = "green"
    confidence = 92
    reasons: list[str] = []

    if not native_ready or native_status in {"invalid_target", "game_unavailable", "native_unavailable", "skipped_32bit"}:
        return ("red", 5, native_status or "native_not_ready")
    if native_status == "collision_slot_not_loaded":
        status = "red"
        confidence = min(confidence, 25)
        reasons.append("collision_slot_not_loaded")
    elif native_status == "hit_but_collision_slot_not_loaded":
        status = "yellow"
        confidence -= 25
        reasons.append("collision_slot_uncertain")
    elif native_status == "hit_entity_without_collision_flag":
        status = "red"
        confidence = min(confidence, 30)
        reasons.append("entity_without_collision_flag")
    elif native_status == "hit_invisible_or_unrendered_entity":
        status = "yellow"
        confidence -= 25
        reasons.append("invisible_or_unrendered_entity")
    elif native_status and native_status != "ok":
        status = "red"
        confidence = min(confidence, 35)
        reasons.append(native_status)

    if not collision_loaded:
        if vertical_hit and status != "red":
            status = "yellow"
            confidence -= 15
        else:
            status = "red"
            confidence = min(confidence, 25)
        reasons.append("collision_loaded=false")
    if forced_full_pass and status != "red":
        status = "yellow"
        confidence -= 18
        reasons.append("forced_full_collision_pass")
    if not vertical_hit:
        status = "red"
        confidence = min(confidence, 20)
        reasons.append("vertical_hit=false")
    if vertical_hit and vertical_z < -250.0:
        status = "red"
        confidence = min(confidence, 25)
        reasons.append(f"vertical_z={vertical_z:.1f}")
    if model >= 0 and not uses_collision:
        status = "red"
        confidence = min(confidence, 30)
        reasons.append(f"model={model}_uses_collision=false")
    if vertical_hit and model < 0:
        status = "yellow" if status != "red" else status
        confidence -= 15
        reasons.append("hit_without_model")
    if model >= 0 and not visible and not has_rw:
        status = "yellow" if status != "red" else status
        confidence -= 12
        reasons.append(f"model={model}_not_render_ready")
    if ground_z > -9000.0 and vertical_hit and abs(dz) > 12.0:
        status = "yellow" if status != "red" else status
        confidence -= 10
        reasons.append(f"ground_delta={dz:.1f}")
    if bad_events:
        status = "red"
        confidence = min(confidence, 30)
        reasons.append("events=" + ",".join(sorted({str(e.get("kind", "")) for e in bad_events})))
    if not reasons:
        reasons.append("probe_clean")
    return (status, max(1, min(99, int(confidence))), "; ".join(reasons))


def wait_for_command_result(
    serial: str,
    latest_path: str,
    target_x: float,
    target_y: float,
    timeout: float,
    near_radius: float,
    after_wall_time_ms: int = 0,
) -> Optional[dict]:
    latest: Optional[dict] = None
    deadline = time.monotonic() + max(1.0, timeout)
    while time.monotonic() < deadline:
        latest = read_latest(serial, latest_path)
        native = native_from_latest(latest)
        if native.get("nativeReady"):
            x = safe_float(native.get("x"))
            y = safe_float(native.get("y"))
            fresh_enough = after_wall_time_ms <= 0 or latest_wall_time_ms(latest) > after_wall_time_ms
            if fresh_enough and math.hypot(x - target_x, y - target_y) <= near_radius:
                return latest
        time.sleep(1.0)
    return latest


def recover_player(
    serial: str,
    package: str,
    latest_path: str,
    recovery_point: tuple[float, float, float, int],
    timeout: float,
    near_radius: float,
    ground_offset: float,
    settle: float,
    spawn_tap: Optional[tuple[int, int]],
    respawn_wait: float,
) -> tuple[bool, Optional[dict]]:
    rx, ry, rz, area = recovery_point
    baseline = read_latest(serial, latest_path)
    send_runtime_command(serial, package, f"xyron_tp:{rx:.3f},{ry:.3f},{rz:.3f},{area}")
    latest = wait_for_command_result(serial, latest_path, rx, ry, timeout, near_radius, latest_wall_time_ms(baseline))
    time.sleep(max(0.0, min(settle, 2.0)))
    latest = refresh_latest_sample(serial, latest_path, latest)
    native = native_from_latest(latest)
    if not native.get("nativeReady") and str(native.get("reason", "")) in {
        "local_player_inactive",
        "native_snapshot_warmup",
        "local_player_missing",
    }:
        tap_spawn_button(serial, spawn_tap, respawn_wait)
        baseline = read_latest(serial, latest_path)
        send_runtime_command(serial, package, f"xyron_tp:{rx:.3f},{ry:.3f},{rz:.3f},{area}")
        latest = wait_for_command_result(serial, latest_path, rx, ry, timeout, near_radius, latest_wall_time_ms(baseline))
        time.sleep(max(0.0, min(settle, 2.0)))
        latest = refresh_latest_sample(serial, latest_path, latest)
        native = native_from_latest(latest)
    vertical_z = safe_float(native.get("verticalZ"), -9999.0)
    ground_z = safe_float(native.get("groundZ"), -9999.0)
    snap_z = vertical_z if vertical_z > -9000 else ground_z
    if bool(native.get("verticalHit")) and -200.0 < snap_z < 2000.0:
        rz = snap_z + ground_offset
        baseline = latest
        send_runtime_command(serial, package, f"xyron_tp:{rx:.3f},{ry:.3f},{rz:.3f},{area}")
        latest = wait_for_command_result(serial, latest_path, rx, ry, timeout, near_radius, latest_wall_time_ms(baseline))
        time.sleep(max(0.0, min(settle, 2.0)))
        latest = refresh_latest_sample(serial, latest_path, latest)
        native = native_from_latest(latest)
        if not native.get("nativeReady") and str(native.get("reason", "")) in {
            "local_player_inactive",
            "native_snapshot_warmup",
            "local_player_missing",
        }:
            tap_spawn_button(serial, spawn_tap, respawn_wait)
            baseline = read_latest(serial, latest_path)
            send_runtime_command(serial, package, f"xyron_tp:{rx:.3f},{ry:.3f},{rz:.3f},{area}")
            latest = wait_for_command_result(serial, latest_path, rx, ry, timeout, near_radius, latest_wall_time_ms(baseline))
            time.sleep(max(0.0, min(settle, 2.0)))
            latest = refresh_latest_sample(serial, latest_path, latest)
            native = native_from_latest(latest)

    if not native.get("nativeReady"):
        return (False, latest)
    distance = math.hypot(safe_float(native.get("x")) - rx, safe_float(native.get("y")) - ry)
    ok = distance <= near_radius and safe_float(native.get("z")) > -40.0 and bool(native.get("verticalHit"))
    return (ok, latest)


def wait_for_probe_result(
    serial: str,
    probe_path: str,
    probe_id: str,
    before_count: int,
    timeout: float,
) -> Optional[dict]:
    deadline = time.monotonic() + max(1.0, timeout)
    latest: Optional[dict] = None
    while time.monotonic() < deadline:
        rows = parse_jsonl(adb_cat(serial, probe_path))
        new_rows = rows[before_count:] if len(rows) >= before_count else rows
        for row in reversed(new_rows):
            latest = row
            if str(row.get("zoneId", "")) == probe_id:
                return row
        time.sleep(0.5)
    return latest


def probe_status(probe_result: Optional[dict]) -> str:
    native = (probe_result or {}).get("native") or {}
    return str(native.get("status", "") or "")


def write_report(output_dir: Path, rows: list[dict]) -> None:
    counts: dict[str, int] = {}
    for row in rows:
        counts[row["status"]] = counts.get(row["status"], 0) + 1
    lines = [
        "# Runtime zone sweep",
        "",
        f"- Updated: `{time.strftime('%Y-%m-%d %H:%M:%S')}`",
        f"- Green: `{counts.get('green', 0)}`",
        f"- Yellow: `{counts.get('yellow', 0)}`",
        f"- Red: `{counts.get('red', 0)}`",
        "",
        "## Bad/uncertain points",
        "",
    ]
    bad = [row for row in rows if row["status"] != "green"]
    for row in bad[:80]:
        lines.append(
            f"- `{row['zone_id']}` status=`{row['status']}` conf=`{row['confidence']}` "
            f"target=`{row['target_x']:.1f},{row['target_y']:.1f}` actual=`{row['x']:.1f},{row['y']:.1f},{row['z']:.1f}` "
            f"dz=`{row['dz']:.1f}` fps=`{row['fps']:.0f}` native=`{row.get('native_status', '')}` "
            f"model=`{row.get('model', -1)}` reason=`{row['reason']}`"
        )
    if not bad:
        lines.append("- No bad point in this sweep.")
    (output_dir / "runtime_sweep_report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Probe or teleport through map zones and record runtime monitor evidence.")
    parser.add_argument("--serial", default="")
    parser.add_argument("--package", default=DEFAULT_PACKAGE)
    parser.add_argument("--mode", choices=("probe", "teleport"), default="probe")
    parser.add_argument("--route", type=Path, default=None)
    parser.add_argument("--zone-csv", type=Path, default=None)
    parser.add_argument("--skip-empty-zones", action="store_true")
    parser.add_argument("--probe-bounds", type=float, nargs=4, default=None, metavar=("XMIN", "YMIN", "XMAX", "YMAX"))
    parser.add_argument("--area", type=float, nargs=4, default=None, metavar=("XMIN", "YMIN", "XMAX", "YMAX"))
    parser.add_argument("--step", type=float, default=250.0)
    parser.add_argument("--z", type=float, default=80.0)
    parser.add_argument("--ground-offset", type=float, default=1.25)
    parser.add_argument("--settle", type=float, default=4.0)
    parser.add_argument("--command-timeout", type=float, default=25.0)
    parser.add_argument("--max-points", type=int, default=0)
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--near-radius", type=float, default=90.0)
    parser.add_argument("--recovery-point", default="1958.3783,1343.1572,30.0,0")
    parser.add_argument("--recover-before-each-point", action="store_true")
    parser.add_argument("--no-recover-after-bad", action="store_true")
    parser.add_argument("--no-preprobe-ground", action="store_true", help="Disable collision probe before teleporting to a point.")
    parser.add_argument("--spawn-tap", default="1356,1132", help="Physical screen X,Y to tap when the local player is inactive; use 'none' to disable.")
    parser.add_argument("--respawn-wait", type=float, default=8.0)
    parser.add_argument("--status", default="red,yellow", help="Comma-separated status filter for route files.")
    parser.add_argument("--start-game", action="store_true")
    parser.add_argument("--server", default="147.185.221.224:2704")
    parser.add_argument("--no-restart-on-missing", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--screenshot-on-red", action="store_true")
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args(argv)

    if not args.route and not args.area:
        raise SystemExit("Use --route route_priority.json or --area xmin ymin xmax ymax")
    if args.step <= 0:
        raise SystemExit("--step must be positive")

    serial = choose_device(args.serial or None)
    route = build_area_route(args.area, args.step, args.z) if args.area else load_route(
        args.route,
        {item.strip().lower() for item in args.status.split(",") if item.strip()},
    )
    if args.skip_empty_zones:
        route = filter_empty_zones(route, args.zone_csv)
    route = filter_probe_bounds(route, args.probe_bounds)
    if args.offset > 0:
        route = route[args.offset :]
    if args.max_points > 0:
        route = route[: args.max_points]

    stamp = time.strftime("%Y%m%d-%H%M%S")
    output_dir = args.output or Path("cerebro/runtime-zone-sweeps") / stamp
    output_dir.mkdir(parents=True, exist_ok=True)
    latest_path, events_path, _ = remote_monitor_paths(args.package)
    probe_path = remote_probe_results_path(args.package)
    recovery_point = parse_point_spec(args.recovery_point)
    spawn_tap = parse_tap_spec(args.spawn_tap)

    print(f"[zone-sweeper] device={serial}")
    print(f"[zone-sweeper] mode={args.mode} points={len(route)} offset={args.offset} output={output_dir.resolve()}")
    if args.dry_run:
        (output_dir / "planned_route.json").write_text(json.dumps(route, indent=2), encoding="utf-8")
        print("[zone-sweeper] dry-run only; planned_route.json written")
        return 0

    if args.start_game:
        start_game(serial, args.package, args.server)
        time.sleep(8.0)

    rows: list[dict] = []
    jsonl_path = output_dir / "runtime_sweep.jsonl"
    csv_path = output_dir / "runtime_sweep.csv"
    fields = [
        "zone_id",
        "probe_id",
        "mode",
        "target_x",
        "target_y",
        "target_z",
        "x",
        "y",
        "z",
        "ground_z",
        "dz",
        "fps",
        "vertical_hit",
        "collision_loaded",
        "forced_full_pass",
        "native_status",
        "model",
        "type",
        "entity_area",
        "uses_collision",
        "visible",
        "has_rw",
        "events_new",
        "status",
        "confidence",
        "reason",
        "recovered_before",
        "recovered_after",
        "sample_wall_time_ms",
        "distance_to_target",
    ]
    with csv_path.open("w", encoding="utf-8", newline="") as csv_handle, jsonl_path.open("w", encoding="utf-8") as jsonl_handle:
        writer = csv.DictWriter(csv_handle, fieldnames=fields)
        writer.writeheader()
        csv_handle.flush()
        for index, point in enumerate(route, start=1):
            target_x = safe_float(point.get("x"))
            target_y = safe_float(point.get("y"))
            target_z = safe_float(point.get("z"), args.z)
            zone_id = str(point.get("zone_id", f"P{index:04d}"))
            probe_id = f"{zone_id}__{args.offset + index:04d}"
            point_command_after_ms = 0

            recovered_before = ""
            if args.mode == "teleport" and args.recover_before_each_point:
                if not args.no_restart_on_missing and not is_package_running(serial, args.package):
                    print(f"[zone-sweeper] {zone_id} app not running before recovery; restarting")
                    start_game(serial, args.package, args.server)
                    time.sleep(8.0)
                    tap_spawn_button(serial, spawn_tap, args.respawn_wait)
                ok, _ = recover_player(
                    serial,
                    args.package,
                    latest_path,
                    recovery_point,
                    args.command_timeout,
                    args.near_radius,
                    args.ground_offset,
                    args.settle,
                    spawn_tap,
                    args.respawn_wait,
                )
                recovered_before = "ok" if ok else "failed"
                print(f"[zone-sweeper] {zone_id} recovery-before={recovered_before}")

            before_events = parse_jsonl(adb_cat(serial, events_path))

            print(f"[zone-sweeper] {index}/{len(route)} {zone_id} {args.mode} {target_x:.1f},{target_y:.1f},{target_z:.1f}")
            if args.mode == "probe":
                before_probes = parse_jsonl(adb_cat(serial, probe_path))
                send_runtime_command(
                    serial,
                    args.package,
                    f"xyron_probe:{probe_id},{target_x:.3f},{target_y:.3f},{target_z:.3f},0",
                )
                probe_result = wait_for_probe_result(
                    serial,
                    probe_path,
                    probe_id,
                    len(before_probes),
                    args.command_timeout,
                )
                if (
                    (probe_result is None or probe_status(probe_result) in {"game_not_connected", "game_unavailable"})
                    and not args.no_restart_on_missing
                ):
                    app_running = is_package_running(serial, args.package)
                    print(
                        f"[zone-sweeper] {zone_id} probe retry status={probe_status(probe_result) or 'missing'} "
                        f"app_running={app_running}; restarting once"
                    )
                    start_game(serial, args.package, args.server)
                    time.sleep(8.0)
                    before_probes = parse_jsonl(adb_cat(serial, probe_path))
                    send_runtime_command(
                        serial,
                        args.package,
                        f"xyron_probe:{probe_id}_retry,{target_x:.3f},{target_y:.3f},{target_z:.3f},0",
                    )
                    probe_result = wait_for_probe_result(
                        serial,
                        probe_path,
                        f"{probe_id}_retry",
                        len(before_probes),
                        args.command_timeout,
                    )
                time.sleep(max(0.0, args.settle))
                native = (probe_result or {}).get("native") or {}
                latest = {"probe": probe_result}
            else:
                left_target_after_settle = False
                arrived_latest: Optional[dict] = None
                if not args.no_preprobe_ground:
                    before_probes = parse_jsonl(adb_cat(serial, probe_path))
                    ground_probe_id = f"{probe_id}_ground"
                    send_runtime_command(
                        serial,
                        args.package,
                        f"xyron_probe:{ground_probe_id},{target_x:.3f},{target_y:.3f},{target_z:.3f},0",
                    )
                    ground_probe = wait_for_probe_result(
                        serial,
                        probe_path,
                        ground_probe_id,
                        len(before_probes),
                        min(args.command_timeout, 12.0),
                    )
                    ground_native = (ground_probe or {}).get("native") or {}
                    probe_vertical_z = safe_float(ground_native.get("verticalZ"), -9999.0)
                    if bool(ground_native.get("verticalHit")) and -200.0 < probe_vertical_z < 2000.0:
                        target_z = probe_vertical_z + args.ground_offset
                        print(f"[zone-sweeper] {zone_id} preprobe-ground z={target_z:.2f} model={safe_int(ground_native.get('model'), -1)}")
                    else:
                        print(f"[zone-sweeper] {zone_id} preprobe-ground unavailable status={ground_native.get('status', '')}")

                baseline = read_latest(serial, latest_path)
                command_after_ms = latest_wall_time_ms(baseline)
                point_command_after_ms = command_after_ms
                send_runtime_command(serial, args.package, f"xyron_tp:{target_x:.3f},{target_y:.3f},{target_z:.3f},0")
                latest = wait_for_command_result(
                    serial,
                    latest_path,
                    target_x,
                    target_y,
                    args.command_timeout,
                    args.near_radius,
                    command_after_ms,
                )
                if latest_is_fresh_near_target(latest, target_x, target_y, args.near_radius, command_after_ms):
                    arrived_latest = latest
                time.sleep(max(0.0, args.settle))
                settled_latest = wait_for_latest_after(
                    serial,
                    latest_path,
                    latest_wall_time_ms(latest),
                    max(0.5, min(args.settle + 2.0, 8.0)),
                )
                settled_latest = refresh_latest_sample(serial, latest_path, settled_latest or latest)
                if arrived_latest and not latest_is_near_target(settled_latest, target_x, target_y, args.near_radius):
                    left_target_after_settle = True
                    latest = arrived_latest
                else:
                    latest = settled_latest
                native = native_from_latest(latest)

                vertical_z = safe_float(native.get("verticalZ"), -9999.0)
                ground_z = safe_float(native.get("groundZ"), -9999.0)
                snap_z = vertical_z if vertical_z > -9000 else ground_z
                if (
                    latest_is_near_target(latest, target_x, target_y, args.near_radius)
                    and bool(native.get("verticalHit"))
                    and -200.0 < snap_z < 2000.0
                ):
                    target_z = snap_z + args.ground_offset
                    print(f"[zone-sweeper] {zone_id} ground-snap z={target_z:.2f}")
                    baseline = latest
                    command_after_ms = latest_wall_time_ms(baseline)
                    point_command_after_ms = command_after_ms
                    send_runtime_command(serial, args.package, f"xyron_tp:{target_x:.3f},{target_y:.3f},{target_z:.3f},0")
                    latest = wait_for_settled_target_sample(
                        serial,
                        latest_path,
                        target_x,
                        target_y,
                        args.command_timeout,
                        args.near_radius,
                        command_after_ms,
                    )
                    if latest_is_fresh_near_target(latest, target_x, target_y, args.near_radius, command_after_ms):
                        arrived_latest = latest
                    time.sleep(max(0.0, args.settle))
                    settled_latest = wait_for_latest_after(
                        serial,
                        latest_path,
                        latest_wall_time_ms(latest),
                        max(0.5, min(args.settle + 2.0, 8.0)),
                    )
                    settled_latest = refresh_latest_sample(serial, latest_path, settled_latest or latest)
                    if arrived_latest and not latest_is_near_target(settled_latest, target_x, target_y, args.near_radius):
                        left_target_after_settle = True
                        latest = arrived_latest
                    else:
                        latest = settled_latest
                    native = native_from_latest(latest)

            after_events = parse_jsonl(adb_cat(serial, events_path))
            new_events = after_events[len(before_events) :] if len(after_events) >= len(before_events) else after_events
            if args.mode == "probe":
                status, confidence, reason = classify_probe_result(native, new_events)
            else:
                status, confidence, reason = classify_point(native, new_events, target_x, target_y, args.near_radius)
                if point_command_after_ms > 0 and latest_wall_time_ms(latest) <= point_command_after_ms:
                    if latest_is_near_target(latest, target_x, target_y, args.near_radius):
                        if status == "green":
                            status = "yellow"
                        confidence = min(confidence, 72)
                        reason = f"monitor_timestamp_uncertain_near_target; {reason}"
                    else:
                        status = "red"
                        confidence = min(confidence, 8)
                        reason = f"stale_monitor_after_command; {reason}"
                if left_target_after_settle and status == "green":
                    status = "yellow"
                    confidence = min(confidence, 72)
                    reason = f"{reason}; left_target_after_settle"
            row = {
                "zone_id": zone_id,
                "probe_id": probe_id,
                "mode": args.mode,
                "target_x": target_x,
                "target_y": target_y,
                "target_z": target_z,
                "x": safe_float(native.get("x")),
                "y": safe_float(native.get("y")),
                "z": safe_float(native.get("z")),
                "ground_z": safe_float(native.get("groundZ")),
                "dz": safe_float(native.get("dz")),
                "fps": safe_float(native.get("nativeFps")),
                "vertical_hit": bool(native.get("verticalHit")),
                "collision_loaded": bool(native.get("collisionLoaded")),
                "forced_full_pass": bool(native.get("forcedFullPass")),
                "native_status": str(native.get("status", "")),
                "model": safe_int(native.get("model"), -1),
                "type": safe_int(native.get("type"), -1),
                "entity_area": safe_int(native.get("entityArea"), -1),
                "uses_collision": bool(native.get("usesCollision")),
                "visible": bool(native.get("visible")),
                "has_rw": bool(native.get("hasRw")),
                "events_new": len(new_events),
                "status": status,
                "confidence": confidence,
                "reason": reason,
                "recovered_before": recovered_before,
                "recovered_after": "",
                "sample_wall_time_ms": latest_wall_time_ms(latest),
                "distance_to_target": math.hypot(safe_float(native.get("x")) - target_x, safe_float(native.get("y")) - target_y),
            }

            if args.screenshot_on_red and status == "red":
                shot = output_dir / f"{zone_id}_red.png"
                proc = run_adb(["exec-out", "screencap", "-p"], serial=serial, timeout=20, binary=True)
                if proc.returncode == 0 and proc.stdout:
                    shot.write_bytes(proc.stdout)
                    print(f"[zone-sweeper] screenshot={shot}")

            if args.mode == "teleport" and status != "green" and not args.no_recover_after_bad:
                ok, _ = recover_player(
                    serial,
                    args.package,
                    latest_path,
                    recovery_point,
                    args.command_timeout,
                    args.near_radius,
                    args.ground_offset,
                    args.settle,
                    spawn_tap,
                    args.respawn_wait,
                )
                row["recovered_after"] = "ok" if ok else "failed"
                print(f"[zone-sweeper] {zone_id} recovery-after={row['recovered_after']}")

            rows.append(row)
            writer.writerow(row)
            csv_handle.flush()
            jsonl_handle.write(json.dumps({"point": point, "latest": latest, "events": new_events, "result": row}, ensure_ascii=False) + "\n")
            jsonl_handle.flush()
            write_report(output_dir, rows)

    write_report(output_dir, rows)
    print(f"[zone-sweeper] done report={output_dir / 'runtime_sweep_report.md'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
