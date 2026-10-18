#!/usr/bin/env python3
"""Aggregate runtime sweep subpoints into zone coverage maps."""

from __future__ import annotations

import argparse
import csv
import html
import json
import time
from collections import Counter
from pathlib import Path


SEVERITY = {"green": 0, "yellow": 1, "red": 2}


def safe_float(value, default: float = 0.0) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return default
    return number


def safe_int(value, default: int = 0) -> int:
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def read_csv(path: Path) -> list[dict]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def write_csv(path: Path, rows: list[dict]) -> None:
    fields = [
        "zone_id",
        "subpoint_status",
        "subpoint_confidence",
        "tested_points",
        "green_points",
        "yellow_points",
        "red_points",
        "worst_point_id",
        "worst_target_x",
        "worst_target_y",
        "worst_actual_x",
        "worst_actual_y",
        "worst_actual_z",
        "worst_reason",
        "old_final_status",
        "old_final_reason",
        "x0",
        "y0",
        "x1",
        "y1",
        "center_x",
        "center_y",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})


def contains(zone: dict, x: float, y: float) -> bool:
    x0 = safe_float(zone.get("x0"))
    y0 = safe_float(zone.get("y0"))
    x1 = safe_float(zone.get("x1"))
    y1 = safe_float(zone.get("y1"))
    return x0 <= x < x1 and y0 <= y < y1


def assign_points(zones: list[dict], sweep_paths: list[Path]) -> dict[str, list[dict]]:
    by_zone = {zone.get("zone_id", ""): [] for zone in zones}
    for path in sweep_paths:
        if not path.exists():
            continue
        for point in read_csv(path):
            x = safe_float(point.get("target_x"))
            y = safe_float(point.get("target_y"))
            matched = None
            for zone in zones:
                if contains(zone, x, y):
                    matched = zone.get("zone_id", "")
                    break
            if matched:
                row = dict(point)
                row["_source"] = str(path)
                by_zone.setdefault(matched, []).append(row)
    return by_zone


def summarize_zone(zone: dict, points: list[dict]) -> dict:
    counts = Counter(str(point.get("status", "") or "yellow").lower() for point in points)
    worst = None
    for point in points:
        if worst is None:
            worst = point
            continue
        point_status = str(point.get("status", "") or "yellow").lower()
        worst_status = str(worst.get("status", "") or "yellow").lower()
        if SEVERITY.get(point_status, 1) > SEVERITY.get(worst_status, 1):
            worst = point
        elif SEVERITY.get(point_status, 1) == SEVERITY.get(worst_status, 1):
            if safe_int(point.get("confidence"), 99) < safe_int(worst.get("confidence"), 99):
                worst = point

    if not points:
        status = "untested"
        confidence = 0
    elif counts.get("red", 0):
        status = "red"
        confidence = min(safe_int(point.get("confidence"), 35) for point in points if str(point.get("status", "")).lower() == "red")
    elif counts.get("yellow", 0):
        status = "yellow"
        confidence = min(safe_int(point.get("confidence"), 60) for point in points if str(point.get("status", "")).lower() == "yellow")
    else:
        status = "green"
        confidence = min(safe_int(point.get("confidence"), 85) for point in points) if points else 0

    return {
        "zone_id": zone.get("zone_id", ""),
        "subpoint_status": status,
        "subpoint_confidence": confidence,
        "tested_points": len(points),
        "green_points": counts.get("green", 0),
        "yellow_points": counts.get("yellow", 0),
        "red_points": counts.get("red", 0),
        "worst_point_id": worst.get("probe_id", "") if worst else "",
        "worst_target_x": worst.get("target_x", "") if worst else "",
        "worst_target_y": worst.get("target_y", "") if worst else "",
        "worst_actual_x": worst.get("x", "") if worst else "",
        "worst_actual_y": worst.get("y", "") if worst else "",
        "worst_actual_z": worst.get("z", "") if worst else "",
        "worst_reason": worst.get("reason", "") if worst else "not_tested",
        "old_final_status": zone.get("final_status", zone.get("status", "")),
        "old_final_reason": zone.get("final_reason", zone.get("reason", "")),
        "x0": zone.get("x0", ""),
        "y0": zone.get("y0", ""),
        "x1": zone.get("x1", ""),
        "y1": zone.get("y1", ""),
        "center_x": zone.get("center_x", ""),
        "center_y": zone.get("center_y", ""),
    }


def color_for_status(status: str) -> str:
    return {
        "green": "#25a244",
        "yellow": "#d9a404",
        "red": "#d64545",
        "untested": "#56616d",
    }.get(status, "#888888")


def write_geojson(path: Path, rows: list[dict]) -> None:
    features = []
    for row in rows:
        x0 = safe_float(row.get("x0"))
        y0 = safe_float(row.get("y0"))
        x1 = safe_float(row.get("x1"))
        y1 = safe_float(row.get("y1"))
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


def scale_point(x: float, y: float, min_x: float, max_y: float, width: float, height: float, size: int) -> tuple[float, float]:
    sx = (x - min_x) / width * size
    sy = (max_y - y) / height * size
    return (sx, sy)


def write_html(path: Path, rows: list[dict], points_by_zone: dict[str, list[dict]]) -> None:
    min_x = min(safe_float(row.get("x0")) for row in rows)
    max_x = max(safe_float(row.get("x1")) for row in rows)
    min_y = min(safe_float(row.get("y0")) for row in rows)
    max_y = max(safe_float(row.get("y1")) for row in rows)
    width = max_x - min_x
    height = max_y - min_y
    size = 980

    rects = []
    circles = []
    for row in rows:
        x0 = safe_float(row.get("x0"))
        y0 = safe_float(row.get("y0"))
        x1 = safe_float(row.get("x1"))
        y1 = safe_float(row.get("y1"))
        sx, sy = scale_point(x0, y1, min_x, max_y, width, height, size)
        sw = (x1 - x0) / width * size
        sh = (y1 - y0) / height * size
        status = str(row.get("subpoint_status", "untested"))
        title = (
            f"{row.get('zone_id')} | {status} | tested={row.get('tested_points')} | "
            f"red={row.get('red_points')} | old={row.get('old_final_status')} | {row.get('worst_reason')}"
        )
        rects.append(
            f"<rect x='{sx:.2f}' y='{sy:.2f}' width='{sw:.2f}' height='{sh:.2f}' "
            f"fill='{color_for_status(status)}' opacity='0.72' stroke='#1d242d' stroke-width='0.35'>"
            f"<title>{html.escape(title)}</title></rect>"
        )
        for point in points_by_zone.get(row.get("zone_id", ""), []):
            px, py = scale_point(safe_float(point.get("target_x")), safe_float(point.get("target_y")), min_x, max_y, width, height, size)
            point_status = str(point.get("status", "yellow")).lower()
            point_title = (
                f"{point.get('probe_id')} | {point_status} | target={point.get('target_x')},{point.get('target_y')} | "
                f"actual={point.get('x')},{point.get('y')},{point.get('z')} | {point.get('reason')}"
            )
            circles.append(
                f"<circle cx='{px:.2f}' cy='{py:.2f}' r='2.4' fill='{color_for_status(point_status)}' "
                f"stroke='#0b0d10' stroke-width='0.45'><title>{html.escape(point_title)}</title></circle>"
            )

    counts = Counter(row.get("subpoint_status", "untested") for row in rows)
    body = f"""<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<title>Runtime subpoint map</title>
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
<h1>Runtime subpoint map</h1>
<div class="legend">
  <span class="chip"><span class="sw" style="background:#25a244"></span>Green tested: {counts.get('green', 0)}</span>
  <span class="chip"><span class="sw" style="background:#d9a404"></span>Yellow uncertain: {counts.get('yellow', 0)}</span>
  <span class="chip"><span class="sw" style="background:#d64545"></span>Red bad: {counts.get('red', 0)}</span>
  <span class="chip"><span class="sw" style="background:#56616d"></span>Untested: {counts.get('untested', 0)}</span>
</div>
<svg viewBox="0 0 {size} {size}" role="img" aria-label="Runtime subpoint coverage">
{chr(10).join(rects)}
{chr(10).join(circles)}
</svg>
</main>
</body>
</html>
"""
    path.write_text(body, encoding="utf-8")


def write_report(path: Path, rows: list[dict], points_by_zone: dict[str, list[dict]]) -> None:
    counts = Counter(row.get("subpoint_status", "untested") for row in rows)
    tested_points = sum(len(points) for points in points_by_zone.values())
    red_rows = [row for row in rows if row.get("subpoint_status") == "red"]
    lines = [
        "# Runtime subpoint coverage",
        "",
        f"- Updated: `{time.strftime('%Y-%m-%d %H:%M:%S')}`",
        f"- Zones: `{len(rows)}`",
        f"- Runtime subpoints: `{tested_points}`",
        f"- Green zones: `{counts.get('green', 0)}`",
        f"- Yellow zones: `{counts.get('yellow', 0)}`",
        f"- Red zones: `{counts.get('red', 0)}`",
        f"- Untested zones: `{counts.get('untested', 0)}`",
        "",
        "## Red zones",
        "",
    ]
    for row in red_rows[:160]:
        lines.append(
            f"- `{row['zone_id']}` points=`{row['tested_points']}` red=`{row['red_points']}` "
            f"target=`{row['worst_target_x']},{row['worst_target_y']}` "
            f"actual=`{row['worst_actual_x']},{row['worst_actual_y']},{row['worst_actual_z']}` "
            f"old=`{row['old_final_status']}` reason=`{row['worst_reason']}`"
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
    points_by_zone = assign_points(zones, args.sweep)
    rows = [summarize_zone(zone, points_by_zone.get(zone.get("zone_id", ""), [])) for zone in zones]

    args.output.mkdir(parents=True, exist_ok=True)
    write_csv(args.output / "subpoint_zones.csv", rows)
    write_geojson(args.output / "subpoint_zones.geojson", rows)
    write_html(args.output / "subpoint_zone_map.html", rows, points_by_zone)
    write_report(args.output / "subpoint_report.md", rows, points_by_zone)
    print(f"[subpoint-map] wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
