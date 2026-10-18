#!/usr/bin/env python3
"""Build a green/yellow/red collision and visibility zone map.

The scanner combines static map asset audit data (IDE/IPL/DFF/COL status) with
runtime monitor evidence (falls, invisible support, pass-through probes, FPS
jank). It does not need the phone connected; use runtime_zone_sweeper.py to add
new live samples.
"""

from __future__ import annotations

import argparse
import csv
import html
import json
import math
import time
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional


DEFAULT_AUDIT_DIR = Path("cerebro/map-audit-workspace")
DEFAULT_RUNTIME_BASE = Path("cerebro/runtime-monitor")
DEFAULT_MAP_INPUTS = [
    Path("../cerebro/device-data/maps-all"),
]
DEFAULT_BOUNDS = (-3200.0, -3200.0, 3200.0, 3200.0)
BAD_EVENT_KINDS = {
    "fall_or_missing_floor",
    "floating_invisible_support",
    "collision_flags_bad",
    "object_pass_through_suspect",
    "object_collision_probe_bad",
}


@dataclass
class Placement:
    model_id: int
    model_name: str
    interior: Optional[int]
    x: float
    y: float
    z: float
    source: str


@dataclass
class Zone:
    ix: int
    iy: int
    x0: float
    y0: float
    x1: float
    y1: float
    placements: int = 0
    static_ok: int = 0
    static_collision: int = 0
    static_invisible: int = 0
    static_missing_ide: int = 0
    static_missing_visual: int = 0
    static_unknown: int = 0
    samples: int = 0
    fps_sum: float = 0.0
    runtime_events: int = 0
    runtime_critical: int = 0
    runtime_warning: int = 0
    floor_events: int = 0
    support_events: int = 0
    pass_events: int = 0
    probe_events: int = 0
    fps_events: int = 0
    top_models: Counter = field(default_factory=Counter)
    top_events: Counter = field(default_factory=Counter)
    status: str = "yellow"
    confidence: int = 0
    reason: str = ""

    @property
    def zone_id(self) -> str:
        return f"Z{self.ix:02d}_{self.iy:02d}"

    @property
    def cx(self) -> float:
        return (self.x0 + self.x1) * 0.5

    @property
    def cy(self) -> float:
        return (self.y0 + self.y1) * 0.5

    @property
    def static_bad_score(self) -> int:
        return (
            self.static_collision * 2
            + self.static_invisible * 5
            + self.static_missing_ide * 4
            + self.static_missing_visual * 3
            + self.static_unknown
        )

    @property
    def avg_fps(self) -> float:
        return self.fps_sum / self.samples if self.samples else 0.0


def strip_comment(line: str) -> str:
    return line.split("#", 1)[0].strip()


def split_csvish(line: str) -> list[str]:
    return [part.strip() for part in line.split(",")]


def safe_int(value: str) -> Optional[int]:
    value = (value or "").strip()
    if not value:
        return None
    try:
        return int(value, 0)
    except ValueError:
        try:
            return int(float(value))
        except ValueError:
            return None


def safe_float(value: str) -> Optional[float]:
    try:
        number = float((value or "").strip())
    except ValueError:
        return None
    return number if math.isfinite(number) else None


def normalize_model_name(value: str) -> str:
    value = (value or "").strip().strip("\x00")
    if not value:
        return ""
    value = value.replace("\\", "/").split("/")[-1]
    return Path(value).stem if "." in value else value


def read_text(path: Path) -> str:
    raw = path.read_bytes()
    for encoding in ("utf-8-sig", "cp1252", "latin-1"):
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1", errors="replace")


def iter_ipl_files(inputs: Iterable[Path]) -> Iterable[Path]:
    for root in inputs:
        if not root.exists():
            continue
        if root.is_file() and root.suffix.lower() == ".ipl":
            yield root
            continue
        if root.is_dir():
            yield from sorted(root.rglob("*.ipl"))


def parse_ipl_file(path: Path) -> list[Placement]:
    placements: list[Placement] = []
    section = ""
    for raw_line in read_text(path).splitlines():
        line = strip_comment(raw_line)
        if not line:
            continue
        lower = line.lower()
        if lower == "end":
            section = ""
            continue
        if "," not in line:
            section = lower
            continue
        if section not in ("inst", "cars", ""):
            continue
        parts = split_csvish(line)
        if len(parts) < 6:
            continue
        model_id = safe_int(parts[0])
        x = safe_float(parts[3])
        y = safe_float(parts[4])
        z = safe_float(parts[5])
        if model_id is None or x is None or y is None or z is None:
            continue
        placements.append(
            Placement(
                model_id=model_id,
                model_name=normalize_model_name(parts[1]) if len(parts) > 1 else "",
                interior=safe_int(parts[2]) if len(parts) > 2 else None,
                x=x,
                y=y,
                z=z,
                source=str(path),
            )
        )
    return placements


def load_audit_rows(audit_dir: Path) -> dict[int, dict[str, str]]:
    path = audit_dir / "models.csv"
    rows: dict[int, dict[str, str]] = {}
    if not path.exists():
        return rows
    with path.open("r", encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle):
            model_id = safe_int(row.get("model_id", ""))
            if model_id is not None:
                rows[model_id] = row
    return rows


def latest_runtime_dir(base: Path) -> Optional[Path]:
    if not base.exists():
        return None
    candidates = [
        path
        for path in base.iterdir()
        if path.is_dir() and ((path / "events.jsonl").exists() or (path / "samples.jsonl").exists())
    ]
    if not candidates:
        return None
    return max(candidates, key=lambda path: path.stat().st_mtime)


def read_jsonl(path: Path) -> list[dict]:
    if not path.exists():
        return []
    rows: list[dict] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return rows


def zone_index(x: float, y: float, bounds: tuple[float, float, float, float], cell: float) -> Optional[tuple[int, int]]:
    xmin, ymin, xmax, ymax = bounds
    if x < xmin or x >= xmax or y < ymin or y >= ymax:
        return None
    return (int((x - xmin) // cell), int((y - ymin) // cell))


def build_zones(bounds: tuple[float, float, float, float], cell: float) -> dict[tuple[int, int], Zone]:
    xmin, ymin, xmax, ymax = bounds
    nx = int(math.ceil((xmax - xmin) / cell))
    ny = int(math.ceil((ymax - ymin) / cell))
    zones: dict[tuple[int, int], Zone] = {}
    for ix in range(nx):
        for iy in range(ny):
            x0 = xmin + ix * cell
            y0 = ymin + iy * cell
            zones[(ix, iy)] = Zone(
                ix=ix,
                iy=iy,
                x0=x0,
                y0=y0,
                x1=min(x0 + cell, xmax),
                y1=min(y0 + cell, ymax),
            )
    return zones


def static_bucket(risk: str) -> str:
    risk = (risk or "").upper()
    if risk == "CRITICAL_COLLISION":
        return "collision"
    if risk == "CRITICAL_INVISIBLE":
        return "invisible"
    if risk == "MISSING_IDE":
        return "missing_ide"
    if risk == "MISSING_VISUAL_ASSET":
        return "missing_visual"
    if risk == "UNKNOWN_COLLISION":
        return "unknown"
    return "ok"


def add_static_evidence(
    zones: dict[tuple[int, int], Zone],
    placements: Iterable[Placement],
    audit_rows: dict[int, dict[str, str]],
    bounds: tuple[float, float, float, float],
    cell: float,
    max_world_z: float,
) -> None:
    for placement in placements:
        if abs(placement.z) > max_world_z:
            continue
        key = zone_index(placement.x, placement.y, bounds, cell)
        if key not in zones:
            continue
        zone = zones[key]
        row = audit_rows.get(placement.model_id, {})
        risk = row.get("risk", "MISSING_IDE")
        model_name = row.get("model_name") or placement.model_name
        bucket = static_bucket(risk)
        zone.placements += 1
        if bucket == "collision":
            zone.static_collision += 1
        elif bucket == "invisible":
            zone.static_invisible += 1
        elif bucket == "missing_ide":
            zone.static_missing_ide += 1
        elif bucket == "missing_visual":
            zone.static_missing_visual += 1
        elif bucket == "unknown":
            zone.static_unknown += 1
        else:
            zone.static_ok += 1
        if bucket != "ok":
            zone.top_models[f"{placement.model_id}:{model_name}:{risk}"] += 1


def native_from_row(row: dict) -> dict:
    sample = row.get("sample") or row
    native = sample.get("native") or {}
    return native if isinstance(native, dict) else {}


def add_runtime_events(
    zones: dict[tuple[int, int], Zone],
    events: Iterable[dict],
    bounds: tuple[float, float, float, float],
    cell: float,
) -> None:
    for event in events:
        native = native_from_row(event)
        x = safe_float(str(native.get("x", "")))
        y = safe_float(str(native.get("y", "")))
        if x is None or y is None:
            continue
        key = zone_index(x, y, bounds, cell)
        if key not in zones:
            continue
        zone = zones[key]
        kind = str(event.get("kind", "unknown"))
        severity = str(event.get("severity", "")).lower()
        zone.runtime_events += 1
        zone.top_events[kind] += 1
        if severity in {"critical", "error"} or kind in BAD_EVENT_KINDS:
            zone.runtime_critical += 1
        elif severity in {"warning", "warn"}:
            zone.runtime_warning += 1
        if kind == "fall_or_missing_floor":
            zone.floor_events += 1
        elif kind == "floating_invisible_support":
            zone.support_events += 1
        elif kind == "object_pass_through_suspect":
            zone.pass_events += 1
        elif kind == "object_collision_probe_bad":
            zone.probe_events += 1
        elif kind == "fps_or_jank_drop":
            zone.fps_events += 1


def add_runtime_samples(
    zones: dict[tuple[int, int], Zone],
    samples: Iterable[dict],
    bounds: tuple[float, float, float, float],
    cell: float,
) -> None:
    for sample in samples:
        native = native_from_row(sample)
        if not native.get("nativeReady"):
            continue
        x = safe_float(str(native.get("x", "")))
        y = safe_float(str(native.get("y", "")))
        if x is None or y is None:
            continue
        key = zone_index(x, y, bounds, cell)
        if key not in zones:
            continue
        zone = zones[key]
        zone.samples += 1
        fps = safe_float(str((sample.get("perf") or {}).get("nativeFps", native.get("nativeFps", 0))))
        if fps is not None:
            zone.fps_sum += fps


def classify(zone: Zone, min_samples: int) -> None:
    specific_bad_runtime = zone.floor_events + zone.support_events + zone.pass_events + zone.probe_events
    bad_runtime = max(zone.runtime_critical, specific_bad_runtime)
    reasons: list[str] = []
    if bad_runtime:
        zone.status = "red"
        reasons.append(f"runtime_bad={bad_runtime}")
    elif zone.static_invisible or zone.static_missing_ide:
        zone.status = "red"
        reasons.append("static_invisible_or_missing_ide")
    elif zone.static_collision >= 10:
        zone.status = "red"
        reasons.append(f"many_static_collision_risks={zone.static_collision}")
    elif zone.samples < min_samples:
        zone.status = "yellow"
        reasons.append(f"low_runtime_coverage={zone.samples}/{min_samples}")
    elif zone.static_collision or zone.static_missing_visual or zone.static_unknown:
        zone.status = "yellow"
        reasons.append("static_risk_needs_runtime_check")
    elif zone.runtime_warning or zone.fps_events:
        zone.status = "yellow"
        reasons.append("runtime_warning_or_fps")
    else:
        zone.status = "green"
        reasons.append("covered_and_clean")

    confidence = 35
    if zone.samples >= min_samples:
        confidence += 35
    elif zone.samples:
        confidence += 15
    if zone.placements:
        confidence += 5
    if zone.static_bad_score == 0:
        confidence += 15
    else:
        confidence -= min(35, zone.static_bad_score * 2)
    if bad_runtime:
        confidence -= min(55, bad_runtime * 20)
    else:
        confidence += 10
    if zone.status == "green":
        confidence = max(confidence, 80)
    if zone.status == "red":
        confidence = min(confidence, 45)
    if zone.samples == 0:
        confidence = min(confidence, 60)
    zone.confidence = max(1, min(99, int(confidence)))
    zone.reason = "; ".join(reasons)


def write_csv_report(path: Path, zones: Iterable[Zone]) -> None:
    fields = [
        "zone_id",
        "status",
        "confidence",
        "x0",
        "y0",
        "x1",
        "y1",
        "center_x",
        "center_y",
        "samples",
        "avg_fps",
        "runtime_events",
        "runtime_critical",
        "floor_events",
        "support_events",
        "pass_events",
        "probe_events",
        "fps_events",
        "placements",
        "static_collision",
        "static_invisible",
        "static_missing_ide",
        "static_missing_visual",
        "static_unknown",
        "reason",
        "top_models",
        "top_events",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for zone in zones:
            writer.writerow(
                {
                    "zone_id": zone.zone_id,
                    "status": zone.status,
                    "confidence": zone.confidence,
                    "x0": f"{zone.x0:.2f}",
                    "y0": f"{zone.y0:.2f}",
                    "x1": f"{zone.x1:.2f}",
                    "y1": f"{zone.y1:.2f}",
                    "center_x": f"{zone.cx:.2f}",
                    "center_y": f"{zone.cy:.2f}",
                    "samples": zone.samples,
                    "avg_fps": f"{zone.avg_fps:.1f}",
                    "runtime_events": zone.runtime_events,
                    "runtime_critical": zone.runtime_critical,
                    "floor_events": zone.floor_events,
                    "support_events": zone.support_events,
                    "pass_events": zone.pass_events,
                    "probe_events": zone.probe_events,
                    "fps_events": zone.fps_events,
                    "placements": zone.placements,
                    "static_collision": zone.static_collision,
                    "static_invisible": zone.static_invisible,
                    "static_missing_ide": zone.static_missing_ide,
                    "static_missing_visual": zone.static_missing_visual,
                    "static_unknown": zone.static_unknown,
                    "reason": zone.reason,
                    "top_models": " | ".join(f"{k} x{v}" for k, v in zone.top_models.most_common(5)),
                    "top_events": " | ".join(f"{k} x{v}" for k, v in zone.top_events.most_common(5)),
                }
            )


def write_geojson(path: Path, zones: Iterable[Zone]) -> None:
    features = []
    for zone in zones:
        features.append(
            {
                "type": "Feature",
                "properties": {
                    "zone_id": zone.zone_id,
                    "status": zone.status,
                    "confidence": zone.confidence,
                    "samples": zone.samples,
                    "runtime_critical": zone.runtime_critical,
                    "static_collision": zone.static_collision,
                    "static_invisible": zone.static_invisible,
                    "reason": zone.reason,
                },
                "geometry": {
                    "type": "Polygon",
                    "coordinates": [
                        [
                            [zone.x0, zone.y0],
                            [zone.x1, zone.y0],
                            [zone.x1, zone.y1],
                            [zone.x0, zone.y1],
                            [zone.x0, zone.y0],
                        ]
                    ],
                },
            }
        )
    path.write_text(json.dumps({"type": "FeatureCollection", "features": features}, indent=2), encoding="utf-8")


def color_for_status(status: str) -> str:
    return {"green": "#25a244", "yellow": "#d9a404", "red": "#d64545"}.get(status, "#888888")


def write_html_map(path: Path, zones: list[Zone], bounds: tuple[float, float, float, float]) -> None:
    xmin, ymin, xmax, ymax = bounds
    width = 1100
    height = 1100

    def sx(x: float) -> float:
        return (x - xmin) / (xmax - xmin) * width

    def sy(y: float) -> float:
        return height - ((y - ymin) / (ymax - ymin) * height)

    rects = []
    for zone in zones:
        x = sx(zone.x0)
        y = sy(zone.y1)
        w = max(1.0, sx(zone.x1) - sx(zone.x0))
        h = max(1.0, sy(zone.y0) - sy(zone.y1))
        title = (
            f"{zone.zone_id} {zone.status} conf={zone.confidence} "
            f"samples={zone.samples} runtime_bad={zone.runtime_critical} "
            f"static_col={zone.static_collision} static_inv={zone.static_invisible} "
            f"{zone.reason}"
        )
        rects.append(
            f"<rect x='{x:.2f}' y='{y:.2f}' width='{w:.2f}' height='{h:.2f}' "
            f"fill='{color_for_status(zone.status)}' opacity='0.74' stroke='#20242a' stroke-width='0.35'>"
            f"<title>{html.escape(title)}</title></rect>"
        )

    body = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Xyron map zone scan</title>
<style>
body {{ margin: 0; background: #101318; color: #e7ecef; font-family: Arial, sans-serif; }}
main {{ padding: 18px; }}
.legend {{ display: flex; gap: 14px; margin: 12px 0 18px; flex-wrap: wrap; }}
.chip {{ display: inline-flex; align-items: center; gap: 8px; }}
.swatch {{ width: 18px; height: 18px; border-radius: 3px; display: inline-block; }}
svg {{ background: #171b22; border: 1px solid #303844; max-width: 100%; height: auto; }}
small {{ color: #aeb8c2; }}
</style>
</head>
<body>
<main>
<h1>Xyron map zone scan</h1>
<small>Hover a square to see evidence. Coordinates are GTA world X/Y; north is up.</small>
<div class="legend">
<span class="chip"><span class="swatch" style="background:#25a244"></span>green: covered and clean</span>
<span class="chip"><span class="swatch" style="background:#d9a404"></span>yellow: needs more runtime coverage</span>
<span class="chip"><span class="swatch" style="background:#d64545"></span>red: runtime/static problem found</span>
</div>
<svg viewBox="0 0 {width} {height}" role="img" aria-label="map zones">
{''.join(rects)}
</svg>
</main>
</body>
</html>
"""
    path.write_text(body, encoding="utf-8")


def write_routes(output_dir: Path, zones: list[Zone], default_z: float) -> None:
    def point(zone: Zone) -> dict:
        return {
            "zone_id": zone.zone_id,
            "status": zone.status,
            "confidence": zone.confidence,
            "x": round(zone.cx, 3),
            "y": round(zone.cy, 3),
            "z": default_z,
            "reason": zone.reason,
        }

    priority = sorted(
        [z for z in zones if z.status != "green"],
        key=lambda z: (
            0 if z.status == "red" else 1,
            -z.runtime_critical,
            -z.static_bad_score,
            z.confidence,
        ),
    )
    all_zones = sorted(zones, key=lambda z: (z.iy, z.ix))
    (output_dir / "route_priority.json").write_text(json.dumps([point(z) for z in priority], indent=2), encoding="utf-8")
    (output_dir / "route_all.json").write_text(json.dumps([point(z) for z in all_zones], indent=2), encoding="utf-8")


def write_markdown_report(
    path: Path,
    zones: list[Zone],
    placements_count: int,
    runtime_dir: Optional[Path],
    output_dir: Path,
    bounds: tuple[float, float, float, float],
    cell_size: float,
) -> None:
    counts = Counter(zone.status for zone in zones)
    red = [zone for zone in zones if zone.status == "red"]
    yellow = [zone for zone in zones if zone.status == "yellow"]
    green = [zone for zone in zones if zone.status == "green"]
    top_red = sorted(red, key=lambda z: (-z.runtime_critical, -z.static_bad_score, z.confidence))[:20]
    top_yellow = sorted(yellow, key=lambda z: (-z.static_bad_score, z.samples, z.confidence))[:20]

    lines = [
        "# Xyron map zone scan",
        "",
        f"- Generated: `{time.strftime('%Y-%m-%d %H:%M:%S')}`",
        f"- Bounds: `{bounds[0]:.0f},{bounds[1]:.0f}` to `{bounds[2]:.0f},{bounds[3]:.0f}`",
        f"- Cell size: `{cell_size:.0f}`",
        f"- Static placements read: `{placements_count}`",
        f"- Runtime dir: `{runtime_dir if runtime_dir else 'none'}`",
        "",
        "## Summary",
        "",
        f"- Green: `{counts.get('green', 0)}`",
        f"- Yellow: `{counts.get('yellow', 0)}`",
        f"- Red: `{counts.get('red', 0)}`",
        "",
        "## Red zones first",
        "",
    ]
    if top_red:
        for zone in top_red:
            lines.append(
                f"- `{zone.zone_id}` center=`{zone.cx:.1f},{zone.cy:.1f}` conf=`{zone.confidence}` "
                f"runtimeBad=`{zone.runtime_critical}` floor=`{zone.floor_events}` support=`{zone.support_events}` "
                f"staticCol=`{zone.static_collision}` staticInv=`{zone.static_invisible}` reason=`{zone.reason}`"
            )
    else:
        lines.append("- No red zone from current evidence.")
    lines += ["", "## Yellow zones needing coverage", ""]
    if top_yellow:
        for zone in top_yellow:
            lines.append(
                f"- `{zone.zone_id}` center=`{zone.cx:.1f},{zone.cy:.1f}` conf=`{zone.confidence}` "
                f"samples=`{zone.samples}` staticScore=`{zone.static_bad_score}` reason=`{zone.reason}`"
            )
    else:
        lines.append("- No yellow zone from current evidence.")
    lines += [
        "",
        "## Files",
        "",
        f"- `zones.csv` has every cell and evidence counters.",
        f"- `zones.geojson` can be opened in GIS/map tools.",
        f"- `zone_map.html` is the clickable color map.",
        f"- `route_priority.json` is the first route for runtime sweeps.",
        "",
        "## Next runtime sweep",
        "",
        "```bat",
        f"python tools\\runtime_zone_sweeper.py --route \"{output_dir / 'route_priority.json'}\" --max-points 12 --settle 4 --screenshot-on-red",
        "```",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def collect_placements(inputs: list[Path]) -> list[Placement]:
    placements: list[Placement] = []
    for path in iter_ipl_files(inputs):
        placements.extend(parse_ipl_file(path))
    return placements


def parse_bounds(values: Optional[list[float]]) -> tuple[float, float, float, float]:
    if not values:
        return DEFAULT_BOUNDS
    if len(values) != 4:
        raise ValueError("--bounds expects xmin ymin xmax ymax")
    xmin, ymin, xmax, ymax = values
    if xmax <= xmin or ymax <= ymin:
        raise ValueError("Invalid bounds; xmax/ymax must be greater than xmin/ymin")
    return (xmin, ymin, xmax, ymax)


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Create a GTA SA map zone collision/visibility confidence scan.")
    parser.add_argument("--audit-dir", type=Path, default=DEFAULT_AUDIT_DIR)
    parser.add_argument("--runtime-dir", type=Path, default=None)
    parser.add_argument("--runtime-base", type=Path, default=DEFAULT_RUNTIME_BASE)
    parser.add_argument("--map-input", type=Path, action="append", default=[])
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument("--cell-size", type=float, default=250.0)
    parser.add_argument("--bounds", type=float, nargs=4, default=None, metavar=("XMIN", "YMIN", "XMAX", "YMAX"))
    parser.add_argument("--max-world-z", type=float, default=450.0)
    parser.add_argument("--min-samples", type=int, default=2)
    parser.add_argument("--route-z", type=float, default=80.0)
    args = parser.parse_args(argv)

    if args.cell_size <= 0:
        raise SystemExit("--cell-size must be positive")

    stamp = time.strftime("%Y%m%d-%H%M%S")
    output_dir = args.output or Path("cerebro/map-zone-scan") / stamp
    output_dir.mkdir(parents=True, exist_ok=True)

    bounds = parse_bounds(args.bounds)
    map_inputs = args.map_input or DEFAULT_MAP_INPUTS
    runtime_dir = args.runtime_dir or latest_runtime_dir(args.runtime_base)

    audit_rows = load_audit_rows(args.audit_dir)
    placements = collect_placements(map_inputs)
    zones_by_key = build_zones(bounds, args.cell_size)
    add_static_evidence(zones_by_key, placements, audit_rows, bounds, args.cell_size, args.max_world_z)

    if runtime_dir:
        add_runtime_events(zones_by_key, read_jsonl(runtime_dir / "events.jsonl"), bounds, args.cell_size)
        add_runtime_samples(zones_by_key, read_jsonl(runtime_dir / "samples.jsonl"), bounds, args.cell_size)

    zones = sorted(zones_by_key.values(), key=lambda z: (z.iy, z.ix))
    for zone in zones:
        classify(zone, args.min_samples)

    write_csv_report(output_dir / "zones.csv", zones)
    write_geojson(output_dir / "zones.geojson", zones)
    write_html_map(output_dir / "zone_map.html", zones, bounds)
    write_routes(output_dir, zones, args.route_z)
    write_markdown_report(output_dir / "zone_report.md", zones, len(placements), runtime_dir, output_dir, bounds, args.cell_size)
    config = {
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "audit_dir": str(args.audit_dir),
        "runtime_dir": str(runtime_dir) if runtime_dir else "",
        "map_inputs": [str(path) for path in map_inputs],
        "output": str(output_dir),
        "bounds": bounds,
        "cell_size": args.cell_size,
        "max_world_z": args.max_world_z,
        "min_samples": args.min_samples,
    }
    (output_dir / "scan_config.json").write_text(json.dumps(config, indent=2), encoding="utf-8")

    counts = Counter(zone.status for zone in zones)
    print(f"[map-zone-scan] output={output_dir.resolve()}")
    print(f"[map-zone-scan] zones={len(zones)} green={counts.get('green', 0)} yellow={counts.get('yellow', 0)} red={counts.get('red', 0)}")
    print(f"[map-zone-scan] report={output_dir / 'zone_report.md'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
