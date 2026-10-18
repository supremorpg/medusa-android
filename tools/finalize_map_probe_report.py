#!/usr/bin/env python3
"""Merge static zone scan and runtime probe sweeps into a final map report."""

from __future__ import annotations

import argparse
import csv
import json
import time
from collections import Counter
from pathlib import Path
from typing import Optional


def safe_int(value, default: int = 0) -> int:
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def safe_float(value, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def read_csv(path: Path) -> list[dict]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def load_probe_rows(paths: list[Path]) -> dict[str, dict]:
    rows: dict[str, dict] = {}
    for path in paths:
        if not path.exists():
            continue
        for row in read_csv(path):
            zone_id = row.get("zone_id", "")
            if zone_id:
                rows[zone_id] = row
    return rows


def final_status_for(zone: dict, probe: Optional[dict]) -> tuple[str, int, str, str]:
    has_target = (
        safe_int(zone.get("placements")) > 0
        or safe_int(zone.get("runtime_events")) > 0
        or safe_int(zone.get("samples")) > 0
    )
    if probe:
        native_status = probe.get("native_status", "")
        if native_status == "game_not_connected":
            return ("yellow", 35, "runtime_probe_inconclusive_game_not_connected", native_status)
        status = probe.get("status", "") or "yellow"
        confidence = safe_int(probe.get("confidence"), 50)
        reason = probe.get("reason", "") or native_status or "runtime_probe"
        return (status, confidence, reason, native_status)
    if not has_target:
        return ("green", 100, "no_static_or_runtime_target", "not_probed_no_target")
    static_status = zone.get("status", "yellow") or "yellow"
    confidence = min(safe_int(zone.get("confidence"), 40), 60)
    return (static_status, confidence, "not_runtime_probed_static_only", "not_probed")


def color_for_status(status: str, no_target: bool = False) -> str:
    if no_target:
        return "#77b77a"
    return {"green": "#25a244", "yellow": "#d9a404", "red": "#d64545"}.get(status, "#888888")


def write_csv(path: Path, rows: list[dict]) -> None:
    fields = [
        "zone_id",
        "final_status",
        "final_confidence",
        "final_reason",
        "probe_native_status",
        "probe_model",
        "probe_collision_loaded",
        "probe_vertical_hit",
        "probe_x",
        "probe_y",
        "probe_z",
        "x0",
        "y0",
        "x1",
        "y1",
        "center_x",
        "center_y",
        "placements",
        "static_status",
        "static_reason",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})


def write_geojson(path: Path, rows: list[dict]) -> None:
    features = []
    for row in rows:
        x0 = safe_float(row["x0"])
        y0 = safe_float(row["y0"])
        x1 = safe_float(row["x1"])
        y1 = safe_float(row["y1"])
        props = {k: v for k, v in row.items() if k not in {"x0", "y0", "x1", "y1"}}
        features.append(
            {
                "type": "Feature",
                "properties": props,
                "geometry": {
                    "type": "Polygon",
                    "coordinates": [[[x0, y0], [x1, y0], [x1, y1], [x0, y1], [x0, y0]]],
                },
            }
        )
    path.write_text(json.dumps({"type": "FeatureCollection", "features": features}, indent=2), encoding="utf-8")


def write_html(path: Path, rows: list[dict]) -> None:
    min_x = min(safe_float(row["x0"]) for row in rows)
    max_x = max(safe_float(row["x1"]) for row in rows)
    min_y = min(safe_float(row["y0"]) for row in rows)
    max_y = max(safe_float(row["y1"]) for row in rows)
    width = max_x - min_x
    height = max_y - min_y
    svg_size = 980

    rects = []
    for row in rows:
        x0 = safe_float(row["x0"])
        y0 = safe_float(row["y0"])
        x1 = safe_float(row["x1"])
        y1 = safe_float(row["y1"])
        sx = (x0 - min_x) / width * svg_size
        sy = (max_y - y1) / height * svg_size
        sw = (x1 - x0) / width * svg_size
        sh = (y1 - y0) / height * svg_size
        no_target = row["final_reason"] == "no_static_or_runtime_target"
        color = color_for_status(row["final_status"], no_target)
        opacity = "0.38" if no_target else "0.82"
        title = (
            f"{row['zone_id']} | {row['final_status']} | conf={row['final_confidence']} | "
            f"native={row['probe_native_status']} | model={row['probe_model']} | {row['final_reason']}"
        )
        rects.append(
            f"<rect x='{sx:.2f}' y='{sy:.2f}' width='{sw:.2f}' height='{sh:.2f}' "
            f"fill='{color}' opacity='{opacity}' stroke='#1d242d' stroke-width='0.35'>"
            f"<title>{title}</title></rect>"
        )

    counts = Counter(row["final_status"] for row in rows)
    body = f"""<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<title>Final map probe</title>
<style>
body {{ margin: 0; font-family: Arial, sans-serif; background: #121417; color: #eef2f4; }}
main {{ max-width: 1180px; margin: 0 auto; padding: 24px; }}
.legend {{ display: flex; gap: 14px; flex-wrap: wrap; margin: 10px 0 18px; }}
.chip {{ display: inline-flex; align-items: center; gap: 7px; font-size: 14px; }}
.sw {{ width: 16px; height: 16px; border-radius: 3px; display: inline-block; }}
svg {{ width: 100%; height: auto; background: #20252b; border: 1px solid #343b44; }}
</style>
</head>
<body>
<main>
<h1>Final map probe</h1>
<div class="legend">
  <span class="chip"><span class="sw" style="background:#25a244"></span>Verde: {counts.get('green', 0)}</span>
  <span class="chip"><span class="sw" style="background:#d9a404"></span>Amarelo: {counts.get('yellow', 0)}</span>
  <span class="chip"><span class="sw" style="background:#d64545"></span>Vermelho: {counts.get('red', 0)}</span>
</div>
<svg viewBox="0 0 {svg_size} {svg_size}" role="img" aria-label="Mapa final por zonas">
{chr(10).join(rects)}
</svg>
</main>
</body>
</html>
"""
    path.write_text(body, encoding="utf-8")


def write_report(path: Path, rows: list[dict], probe_rows: dict[str, dict]) -> None:
    counts = Counter(row["final_status"] for row in rows)
    native_counts = Counter(row["probe_native_status"] for row in rows if row["probe_native_status"])
    no_target = sum(1 for row in rows if row["final_reason"] == "no_static_or_runtime_target")
    red_rows = [row for row in rows if row["final_status"] == "red"]
    lines = [
        "# Final map probe report",
        "",
        f"- Updated: `{time.strftime('%Y-%m-%d %H:%M:%S')}`",
        f"- Total zones: `{len(rows)}`",
        f"- Runtime-probed zones: `{len(probe_rows)}`",
        f"- Empty/no-target zones: `{no_target}`",
        f"- Green: `{counts.get('green', 0)}`",
        f"- Yellow: `{counts.get('yellow', 0)}`",
        f"- Red: `{counts.get('red', 0)}`",
        "",
        "## Native status counts",
        "",
    ]
    for key, count in native_counts.most_common():
        lines.append(f"- `{key}`: `{count}`")
    lines += ["", "## First red zones", ""]
    for row in red_rows[:120]:
        lines.append(
            f"- `{row['zone_id']}` native=`{row['probe_native_status']}` "
            f"model=`{row['probe_model']}` target=`{row['probe_x']},{row['probe_y']}` "
            f"reason=`{row['final_reason']}`"
        )
    if not red_rows:
        lines.append("- No red zones.")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--zones", type=Path, required=True)
    parser.add_argument("--sweep", type=Path, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    zones = read_csv(args.zones)
    probe_rows = load_probe_rows(args.sweep)
    final_rows: list[dict] = []
    for zone in zones:
        zone_id = zone.get("zone_id", "")
        probe = probe_rows.get(zone_id)
        status, confidence, reason, native_status = final_status_for(zone, probe)
        final_rows.append(
            {
                "zone_id": zone_id,
                "final_status": status,
                "final_confidence": confidence,
                "final_reason": reason,
                "probe_native_status": native_status,
                "probe_model": probe.get("model", "") if probe else "",
                "probe_collision_loaded": probe.get("collision_loaded", "") if probe else "",
                "probe_vertical_hit": probe.get("vertical_hit", "") if probe else "",
                "probe_x": probe.get("target_x", "") if probe else "",
                "probe_y": probe.get("target_y", "") if probe else "",
                "probe_z": probe.get("target_z", "") if probe else "",
                "x0": zone.get("x0", ""),
                "y0": zone.get("y0", ""),
                "x1": zone.get("x1", ""),
                "y1": zone.get("y1", ""),
                "center_x": zone.get("center_x", ""),
                "center_y": zone.get("center_y", ""),
                "placements": zone.get("placements", ""),
                "static_status": zone.get("status", ""),
                "static_reason": zone.get("reason", ""),
            }
        )

    args.output.mkdir(parents=True, exist_ok=True)
    write_csv(args.output / "final_zones.csv", final_rows)
    write_geojson(args.output / "final_zones.geojson", final_rows)
    write_html(args.output / "final_zone_map.html", final_rows)
    write_report(args.output / "final_report.md", final_rows, probe_rows)
    print(f"[final-map] wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
