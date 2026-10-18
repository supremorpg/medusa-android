#!/usr/bin/env python3
"""
Static GTA SA / SA:MP map asset auditor.

This tool does not run the game. It scans APK/ZIP/OBB files or extracted
folders, parses IDE/IPL definitions, indexes DFF/TXD/COL/IMG assets, and writes
reports that show which map models are likely invisible or missing collision.
"""

from __future__ import annotations

import argparse
import csv
import html
import io
import json
import os
import re
import struct
import sys
import time
import zipfile
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO, Dict, Iterable, List, Optional, Sequence, Set, Tuple


ASSET_EXTS = {
    ".ide",
    ".ipl",
    ".dff",
    ".txd",
    ".col",
    ".img",
    ".dir",
}

ARCHIVE_EXTS = {".apk", ".zip", ".obb", ".xapk"}
COL_MAGICS = (b"COLL", b"COL2", b"COL3", b"COL4")
IMG_V2_MAGICS = (b"VER2", b"VER3")
TEXT_DECODINGS = ("utf-8-sig", "cp1252", "latin-1")
NAME_RE = re.compile(r"^[A-Za-z0-9_.\-$]+$")


@dataclass
class ModelDef:
    model_id: int
    model_name: str
    txd_name: str
    kind: str
    source: str
    extra: List[str] = field(default_factory=list)


@dataclass
class Placement:
    model_id: int
    model_name: str
    interior: Optional[int]
    x: Optional[float]
    y: Optional[float]
    z: Optional[float]
    source: str


@dataclass
class ImgEntry:
    name: str
    source: str


@dataclass
class AuditResult:
    model_id: int
    model_name: str
    txd_name: str
    kind: str
    placement_count: int
    sample_position: str
    dff_status: str
    txd_status: str
    col_status: str
    risk: str
    source: str
    notes: str


class AssetIndex:
    def __init__(self) -> None:
        self.assets: Dict[str, Dict[str, List[str]]] = defaultdict(lambda: defaultdict(list))
        self.img_entries: Dict[str, Dict[str, List[str]]] = defaultdict(lambda: defaultdict(list))
        self.collision_names: Dict[str, List[str]] = defaultdict(list)
        self.model_defs: Dict[int, ModelDef] = {}
        self.duplicate_defs: Dict[int, List[ModelDef]] = defaultdict(list)
        self.placements: List[Placement] = []
        self.placement_names: Dict[int, Counter] = defaultdict(Counter)
        self.warnings: List[str] = []
        self.errors: List[str] = []
        self.inputs: List[str] = []
        self.file_count = 0
        self.archive_count = 0
        self.img_entry_count = 0
        self.col_chunk_count = 0

    def add_asset(self, name: str, source: str) -> None:
        ext = Path(name).suffix.lower()
        if ext not in ASSET_EXTS:
            return
        base = Path(name).stem.lower()
        if not base:
            return
        self.assets[ext][base].append(source)
        self.file_count += 1

    def add_img_entry(self, entry_name: str, source: str) -> None:
        clean = entry_name.replace("\\", "/").split("/")[-1].strip()
        ext = Path(clean).suffix.lower()
        if not ext:
            return
        base = Path(clean).stem.lower()
        self.img_entries[ext][base].append(source)
        self.img_entry_count += 1

    def add_collision_name(self, name: str, source: str) -> None:
        clean = normalize_model_name(name)
        if not clean:
            return
        self.collision_names[clean.lower()].append(source)

    def has_asset(self, ext: str, model_or_file_name: str) -> bool:
        key = normalize_model_name(model_or_file_name).lower()
        return bool(self.assets.get(ext, {}).get(key) or self.img_entries.get(ext, {}).get(key))

    def asset_sources(self, ext: str, model_or_file_name: str) -> List[str]:
        key = normalize_model_name(model_or_file_name).lower()
        return self.assets.get(ext, {}).get(key, []) + self.img_entries.get(ext, {}).get(key, [])

    def has_collision(self, model_name: str) -> bool:
        key = normalize_model_name(model_name).lower()
        return bool(self.collision_names.get(key) or self.assets.get(".col", {}).get(key) or self.img_entries.get(".col", {}).get(key))


def normalize_model_name(value: str) -> str:
    value = (value or "").strip().strip("\x00").strip()
    if not value:
        return ""
    value = value.replace("\\", "/").split("/")[-1]
    stem = Path(value).stem if "." in value else value
    return stem.strip().strip("\x00")


def strip_comment(line: str) -> str:
    return line.split("#", 1)[0].strip()


def split_csvish(line: str) -> List[str]:
    return [part.strip() for part in line.split(",")]


def decode_text(raw: bytes, source: str, index: AssetIndex) -> str:
    for encoding in TEXT_DECODINGS:
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            continue
    index.warnings.append(f"Texto com encoding estranho, usando latin-1 com replace: {source}")
    return raw.decode("latin-1", errors="replace")


def safe_int(value: str) -> Optional[int]:
    value = value.strip()
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
        return float(value.strip())
    except ValueError:
        return None


def parse_ide_text(text: str, source: str, index: AssetIndex) -> None:
    section = ""
    for raw_line in text.splitlines():
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

        parts = split_csvish(line)
        model_id = safe_int(parts[0]) if parts else None
        if model_id is None or len(parts) < 2:
            continue

        model_name = normalize_model_name(parts[1])
        txd_name = normalize_model_name(parts[2]) if len(parts) > 2 else model_name
        if not model_name:
            continue

        kind = section or "unknown"
        model_def = ModelDef(
            model_id=model_id,
            model_name=model_name,
            txd_name=txd_name,
            kind=kind,
            source=source,
            extra=parts[3:],
        )
        if model_id in index.model_defs:
            index.duplicate_defs[model_id].append(model_def)
        else:
            index.model_defs[model_id] = model_def


def parse_ipl_text(text: str, source: str, index: AssetIndex) -> None:
    section = ""
    for raw_line in text.splitlines():
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
        if len(parts) < 2:
            continue
        model_id = safe_int(parts[0])
        if model_id is None:
            continue
        model_name = normalize_model_name(parts[1])
        interior = safe_int(parts[2]) if len(parts) > 2 else None
        x = safe_float(parts[3]) if len(parts) > 3 else None
        y = safe_float(parts[4]) if len(parts) > 4 else None
        z = safe_float(parts[5]) if len(parts) > 5 else None
        index.placements.append(Placement(model_id, model_name, interior, x, y, z, source))
        if model_name:
            index.placement_names[model_id][model_name] += 1


def printable_name(raw: bytes) -> str:
    raw = raw.split(b"\x00", 1)[0]
    try:
        name = raw.decode("ascii", errors="ignore").strip()
    except UnicodeDecodeError:
        return ""
    name = normalize_model_name(name)
    if 2 <= len(name) <= 32 and NAME_RE.match(name):
        return name
    return ""


def printable_archive_entry_name(raw: bytes) -> str:
    raw = raw.split(b"\x00", 1)[0]
    try:
        name = raw.decode("ascii", errors="ignore").strip()
    except UnicodeDecodeError:
        return ""
    name = name.replace("\\", "/").split("/")[-1].strip().strip("\x00")
    if 3 <= len(name) <= 40 and NAME_RE.match(name) and Path(name).suffix:
        return name
    return ""


def parse_col_bytes(raw: bytes, source: str, index: AssetIndex) -> None:
    basename = normalize_model_name(source)
    if basename:
        index.add_collision_name(basename, source)

    offset = 0
    while True:
        found = find_next_magic(raw, offset, COL_MAGICS)
        if found < 0:
            break
        index.col_chunk_count += 1
        for name_offset in (found + 8, found + 12, found + 16):
            name = printable_name(raw[name_offset : name_offset + 32])
            if name:
                index.add_collision_name(name, source)
                break
        offset = found + 4


def find_next_magic(raw: bytes, start: int, magics: Sequence[bytes]) -> int:
    best = -1
    for magic in magics:
        pos = raw.find(magic, start)
        if pos >= 0 and (best < 0 or pos < best):
            best = pos
    return best


def parse_img_v2_stream(stream: BinaryIO, size: int, source: str, index: AssetIndex) -> bool:
    header = stream.read(8)
    if len(header) < 8 or header[:4] not in IMG_V2_MAGICS:
        return False
    entry_count = struct.unpack("<I", header[4:8])[0]
    max_entries_by_size = max(0, (size - 8) // 32)
    if entry_count > max_entries_by_size or entry_count > 2_000_000:
        index.warnings.append(f"IMG parece invalido ou enorme demais: {source} entries={entry_count}")
        return True

    for _ in range(entry_count):
        entry = stream.read(32)
        if len(entry) < 32:
            break
        name = printable_archive_entry_name(entry[8:32])
        if name:
            index.add_img_entry(name, source)
    return True


def parse_img_v2_bytes(raw: bytes, source: str, index: AssetIndex) -> bool:
    if len(raw) < 8 or raw[:4] not in IMG_V2_MAGICS:
        return False
    entry_count = struct.unpack("<I", raw[4:8])[0]
    max_entries_by_size = max(0, (len(raw) - 8) // 32)
    if entry_count > max_entries_by_size or entry_count > 2_000_000:
        index.warnings.append(f"IMG parece invalido ou enorme demais: {source} entries={entry_count}")
        return True
    pos = 8
    for _ in range(entry_count):
        entry = raw[pos : pos + 32]
        if len(entry) < 32:
            break
        name = printable_archive_entry_name(entry[8:32])
        if name:
            index.add_img_entry(name, source)
        pos += 32
    return True


def parse_dir_bytes(raw: bytes, source: str, index: AssetIndex) -> None:
    if len(raw) < 32:
        return
    count = len(raw) // 32
    for i in range(count):
        entry = raw[i * 32 : i * 32 + 32]
        name = printable_archive_entry_name(entry[8:32])
        if name:
            index.add_img_entry(name, source)


def iter_files(root: Path) -> Iterable[Path]:
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in {".git", ".gradle", "build", ".idea", "__pycache__"}]
        for filename in filenames:
            yield Path(dirpath) / filename


def scan_path(path: Path, index: AssetIndex, max_archive_bytes: int) -> None:
    index.inputs.append(str(path))
    if not path.exists():
        index.errors.append(f"Input nao existe: {path}")
        return
    if path.is_dir():
        for file_path in iter_files(path):
            scan_file(file_path, index, max_archive_bytes)
        return
    scan_file(path, index, max_archive_bytes)


def scan_file(path: Path, index: AssetIndex, max_archive_bytes: int) -> None:
    suffix = path.suffix.lower()
    source = str(path)
    if zipfile.is_zipfile(path):
        scan_zip(path, index, max_archive_bytes)
        return

    index.add_asset(path.name, source)
    try:
        if suffix == ".ide":
            parse_ide_text(decode_text(path.read_bytes(), source, index), source, index)
        elif suffix == ".ipl":
            parse_ipl_text(decode_text(path.read_bytes(), source, index), source, index)
        elif suffix == ".col":
            size = path.stat().st_size
            if size > max_archive_bytes:
                index.warnings.append(f"COL ignorado por tamanho: {source} ({size} bytes)")
            else:
                parse_col_bytes(path.read_bytes(), source, index)
        elif suffix == ".dir":
            size = path.stat().st_size
            if size > max_archive_bytes:
                index.warnings.append(f"DIR ignorado por tamanho: {source} ({size} bytes)")
            else:
                parse_dir_bytes(path.read_bytes(), source, index)
        elif suffix == ".img":
            size = path.stat().st_size
            with path.open("rb") as stream:
                parsed = parse_img_v2_stream(stream, size, source, index)
            if not parsed:
                sibling_dir = path.with_suffix(".dir")
                if sibling_dir.exists():
                    parse_dir_bytes(sibling_dir.read_bytes(), str(sibling_dir), index)
                else:
                    index.warnings.append(f"IMG v1 sem .dir ao lado; nomes internos nao auditados: {source}")
            if size <= max_archive_bytes:
                parse_col_bytes(path.read_bytes(), source, index)
    except OSError as exc:
        index.errors.append(f"Falha lendo {source}: {exc}")


def scan_zip(path: Path, index: AssetIndex, max_archive_bytes: int) -> None:
    index.archive_count += 1
    source_root = str(path)
    try:
        with zipfile.ZipFile(path) as archive:
            scan_zip_archive(archive, source_root, index, max_archive_bytes, depth=0)
    except (OSError, zipfile.BadZipFile) as exc:
        index.errors.append(f"Falha lendo arquivo compactado {source_root}: {exc}")


def scan_zip_archive(
    archive: zipfile.ZipFile,
    source_root: str,
    index: AssetIndex,
    max_archive_bytes: int,
    depth: int,
) -> None:
    infos = [info for info in archive.infolist() if not info.is_dir()]
    infos_by_name = {info.filename.replace("\\", "/").lower(): info for info in infos}

    for info in infos:
        entry_name = info.filename.replace("\\", "/")
        ext = Path(entry_name).suffix.lower()
        source = f"{source_root}!{entry_name}"
        index.add_asset(entry_name, source)

        if ext in ARCHIVE_EXTS and depth < 2:
            if info.file_size > max_archive_bytes:
                index.warnings.append(f"Arquivo compactado aninhado ignorado por tamanho: {source} ({info.file_size} bytes)")
                continue
            try:
                raw_nested = archive.read(info)
                nested_buffer = io.BytesIO(raw_nested)
                if zipfile.is_zipfile(nested_buffer):
                    nested_buffer.seek(0)
                    index.archive_count += 1
                    with zipfile.ZipFile(nested_buffer) as nested_archive:
                        scan_zip_archive(nested_archive, source, index, max_archive_bytes, depth + 1)
            except (OSError, zipfile.BadZipFile, RuntimeError) as exc:
                index.warnings.append(f"Falha lendo compactado aninhado {source}: {exc}")
            continue

        if ext not in ASSET_EXTS:
            continue
        if ext in (".dff", ".txd"):
            continue

        if ext == ".img":
            try:
                with archive.open(info) as stream:
                    parsed = parse_img_v2_stream(stream, info.file_size, source, index)
            except (OSError, RuntimeError, zipfile.BadZipFile) as exc:
                index.warnings.append(f"Falha lendo IMG dentro de ZIP {source}: {exc}")
                continue
            if not parsed:
                parse_matching_dir_from_archive(archive, infos_by_name, entry_name, source_root, index, max_archive_bytes)
            continue

        if info.file_size > max_archive_bytes:
            index.warnings.append(f"Entrada ignorada por tamanho: {source} ({info.file_size} bytes)")
            continue

        raw = archive.read(info)
        if ext == ".ide":
            parse_ide_text(decode_text(raw, source, index), source, index)
        elif ext == ".ipl":
            parse_ipl_text(decode_text(raw, source, index), source, index)
        elif ext == ".col":
            parse_col_bytes(raw, source, index)
        elif ext == ".dir":
            parse_dir_bytes(raw, source, index)


def parse_matching_dir_from_archive(
    archive: zipfile.ZipFile,
    infos_by_name: Dict[str, zipfile.ZipInfo],
    img_entry_name: str,
    source_root: str,
    index: AssetIndex,
    max_archive_bytes: int,
) -> None:
    dir_entry_name = str(Path(img_entry_name).with_suffix(".dir")).replace("\\", "/")
    dir_info = infos_by_name.get(dir_entry_name.lower())
    if not dir_info:
        index.warnings.append(f"IMG v1 dentro de ZIP sem DIR separado auditavel: {source_root}!{img_entry_name}")
        return
    clean_dir_name = dir_info.filename.replace("\\", "/")
    source = f"{source_root}!{clean_dir_name}"
    if dir_info.file_size > max_archive_bytes:
        index.warnings.append(f"DIR correspondente ignorado por tamanho: {source} ({dir_info.file_size} bytes)")
        return
    parse_dir_bytes(archive.read(dir_info), source, index)


def build_results(index: AssetIndex) -> List[AuditResult]:
    placements_by_id: Dict[int, List[Placement]] = defaultdict(list)
    for placement in index.placements:
        placements_by_id[placement.model_id].append(placement)

    all_ids = set(index.model_defs) | set(placements_by_id)
    results: List[AuditResult] = []
    for model_id in sorted(all_ids):
        model_def = index.model_defs.get(model_id)
        placements = placements_by_id.get(model_id, [])
        placement_count = len(placements)
        source = model_def.source if model_def else ""
        kind = model_def.kind if model_def else "ipl_only"

        if model_def:
            model_name = model_def.model_name
            txd_name = model_def.txd_name
        else:
            model_name = index.placement_names[model_id].most_common(1)[0][0] if index.placement_names[model_id] else ""
            txd_name = ""

        sample_position = format_sample_position(placements)
        dff_status = compute_dff_status(index, model_name)
        txd_status = compute_txd_status(index, txd_name, model_name)
        col_status = compute_col_status(index, model_name, txd_name, kind)
        risk, notes = classify_risk(model_def, kind, model_name, txd_name, dff_status, txd_status, col_status, placement_count)

        if model_id in index.duplicate_defs:
            notes = append_note(notes, f"ID duplicado em {len(index.duplicate_defs[model_id]) + 1} IDEs")

        results.append(
            AuditResult(
                model_id=model_id,
                model_name=model_name,
                txd_name=txd_name,
                kind=kind,
                placement_count=placement_count,
                sample_position=sample_position,
                dff_status=dff_status,
                txd_status=txd_status,
                col_status=col_status,
                risk=risk,
                source=source,
                notes=notes,
            )
        )
    return results


def format_sample_position(placements: List[Placement]) -> str:
    for placement in placements:
        if placement.x is None or placement.y is None or placement.z is None:
            continue
        return f"{placement.x:.2f},{placement.y:.2f},{placement.z:.2f}"
    return ""


def compute_dff_status(index: AssetIndex, model_name: str) -> str:
    if not model_name or model_name.lower() in {"null", "nomodelfile"}:
        return "not_expected"
    return "present" if index.has_asset(".dff", model_name) else "missing"


def compute_txd_status(index: AssetIndex, txd_name: str, model_name: str) -> str:
    name = txd_name or model_name
    if not name or name.lower() in {"null", "nomodelfile", "generic"}:
        return "unknown_or_shared"
    if has_android_texdb_assets(index):
        return "unknown_texdb"
    return "present" if index.has_asset(".txd", name) else "missing_or_shared"


def compute_col_status(index: AssetIndex, model_name: str, txd_name: str, kind: str) -> str:
    lower_kind = kind.lower()
    if lower_kind in {"cars", "peds", "ped", "hier"}:
        return "not_static_map"
    if not model_name or model_name.lower() in {"null", "nomodelfile"}:
        return "not_expected"
    if is_lod_model(model_name, txd_name):
        return "not_expected_lod"
    if is_decal_model(model_name, txd_name):
        return "not_expected_decal"
    if not index.assets.get(".col") and not index.img_entries.get(".col") and not index.collision_names:
        return "unknown_no_col_sources"
    return "present" if index.has_collision(model_name) else "missing_or_unknown"


def has_android_texdb_assets(index: AssetIndex) -> bool:
    has_txd = bool(index.assets.get(".txd") or index.img_entries.get(".txd"))
    has_many_img_models = len(index.img_entries.get(".dff", {})) > 100
    return has_many_img_models and not has_txd


def is_lod_model(model_name: str, txd_name: str) -> bool:
    model = model_name.lower()
    txd = txd_name.lower()
    return (
        model.startswith("lod")
        or model.startswith("lod_")
        or "_lod" in model
        or "lod" in txd
    )


def is_decal_model(model_name: str, txd_name: str) -> bool:
    model = model_name.lower()
    txd = txd_name.lower()
    return model.startswith("tag_") or txd.startswith("tags_")


def classify_risk(
    model_def: Optional[ModelDef],
    kind: str,
    model_name: str,
    txd_name: str,
    dff_status: str,
    txd_status: str,
    col_status: str,
    placement_count: int,
) -> Tuple[str, str]:
    notes = ""
    if model_def is None:
        return "MISSING_IDE", "IPL referencia ID sem definicao IDE"
    if dff_status == "missing" and placement_count > 0:
        return "CRITICAL_INVISIBLE", "Existe no mapa, mas DFF nao foi encontrado"
    if col_status == "missing_or_unknown" and placement_count > 0 and kind.lower() in {"objs", "tobj"}:
        return "CRITICAL_COLLISION", "Existe no mapa e pode estar sem COL"
    if dff_status == "missing":
        return "MISSING_VISUAL_ASSET", "IDE define modelo, mas DFF nao foi encontrado"
    if txd_status == "missing_or_shared" and txd_name and txd_name.lower() not in {"generic", "nomodelfile"}:
        return "WARN_TEXTURE", "TXD nao encontrado; pode ser compartilhado ou faltar textura"
    if col_status in {"not_expected_lod", "not_expected_decal"}:
        return "OK", "Colisao normalmente nao esperada para LOD/decal"
    if col_status == "unknown_no_col_sources" and placement_count > 0 and kind.lower() in {"objs", "tobj"}:
        return "UNKNOWN_COLLISION", "Nenhuma fonte COL foi achada para confirmar colisao"
    return "OK", notes


def append_note(notes: str, addition: str) -> str:
    return f"{notes}; {addition}" if notes else addition


def write_reports(index: AssetIndex, results: List[AuditResult], output_dir: Path, html_limit: int) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    issue_counts = Counter(result.risk for result in results)
    extension_counts = {
        ext: len(names) for ext, names in sorted(index.assets.items())
    }
    img_extension_counts = {
        ext: len(names) for ext, names in sorted(index.img_entries.items())
    }
    summary = {
        "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "inputs": index.inputs,
        "files_indexed": index.file_count,
        "archives_indexed": index.archive_count,
        "ide_models": len(index.model_defs),
        "ipl_placements": len(index.placements),
        "collision_names": len(index.collision_names),
        "col_chunks": index.col_chunk_count,
        "img_entries": index.img_entry_count,
        "asset_counts": extension_counts,
        "img_asset_counts": img_extension_counts,
        "risk_counts": dict(issue_counts),
        "warnings": index.warnings[:250],
        "errors": index.errors[:250],
        "top_issues": [result.__dict__ for result in results if result.risk != "OK"][:250],
    }
    (output_dir / "summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=True), encoding="utf-8")
    write_csv(output_dir / "models.csv", results)
    write_csv(output_dir / "missing_collision.csv", [r for r in results if r.risk in {"CRITICAL_COLLISION", "UNKNOWN_COLLISION"}])
    write_csv(output_dir / "missing_visual.csv", [r for r in results if r.risk in {"CRITICAL_INVISIBLE", "MISSING_VISUAL_ASSET", "MISSING_IDE"}])
    write_csv(
        output_dir / "critical_map_issues.csv",
        sorted(
            [r for r in results if r.risk in {"CRITICAL_COLLISION", "CRITICAL_INVISIBLE", "MISSING_IDE"}],
            key=lambda r: (risk_rank(r.risk), -r.placement_count, r.model_id),
        ),
    )
    write_html(output_dir / "map_audit.html", summary, results, html_limit)


def write_csv(path: Path, results: Sequence[AuditResult]) -> None:
    fields = [
        "risk",
        "model_id",
        "kind",
        "model_name",
        "txd_name",
        "placement_count",
        "sample_position",
        "dff_status",
        "txd_status",
        "col_status",
        "notes",
        "source",
    ]
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for result in results:
            row = {field: getattr(result, field) for field in fields}
            writer.writerow(row)


def write_html(path: Path, summary: dict, results: Sequence[AuditResult], html_limit: int) -> None:
    sorted_results = sorted(results, key=lambda r: (risk_rank(r.risk), -r.placement_count, r.model_id))
    visible_results = sorted_results[:html_limit]
    risk_counts = summary.get("risk_counts", {})
    cards = "".join(
        f"<div class='card'><b>{escape(key)}</b><span>{value}</span></div>"
        for key, value in sorted(risk_counts.items())
    )
    rows = "\n".join(result_to_row(result) for result in visible_results)
    warnings = "\n".join(f"<li>{escape(msg)}</li>" for msg in summary.get("warnings", [])[:30])
    errors = "\n".join(f"<li>{escape(msg)}</li>" for msg in summary.get("errors", [])[:30])
    html_text = f"""<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<title>GTA Map Audit</title>
<style>
body {{ font-family: Arial, sans-serif; margin: 24px; color: #1b1b1f; background: #f7f8fa; }}
h1 {{ margin: 0 0 8px; font-size: 28px; }}
.sub {{ color: #555; margin-bottom: 18px; }}
.cards {{ display: flex; flex-wrap: wrap; gap: 10px; margin: 18px 0; }}
.card {{ background: white; border: 1px solid #d8dbe2; border-radius: 8px; padding: 10px 12px; min-width: 150px; }}
.card b {{ display: block; font-size: 12px; color: #555; }}
.card span {{ font-size: 22px; }}
input {{ width: min(720px, 100%); padding: 10px 12px; border: 1px solid #c7cad1; border-radius: 6px; margin: 14px 0; }}
table {{ width: 100%; border-collapse: collapse; background: white; border: 1px solid #d8dbe2; }}
th, td {{ padding: 8px 10px; border-bottom: 1px solid #eceef2; font-size: 13px; text-align: left; }}
th {{ position: sticky; top: 0; background: #eceff4; z-index: 1; }}
tr.issue-critical_collision, tr.issue-critical_invisible, tr.issue-missing_ide {{ background: #fff0f0; }}
tr.issue-warn_texture, tr.issue-unknown_collision, tr.issue-missing_visual_asset {{ background: #fff9e6; }}
tr.issue-ok {{ background: #f4fff5; }}
.mono {{ font-family: Consolas, monospace; }}
.lists {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 18px; }}
pre {{ white-space: pre-wrap; }}
</style>
</head>
<body>
<h1>GTA Map Audit</h1>
<div class="sub">Relatorio estatico de modelos, textura, visual e colisao. Linhas criticas merecem teste no jogo.</div>
<div class="cards">
<div class="card"><b>IDE models</b><span>{summary.get("ide_models", 0)}</span></div>
<div class="card"><b>IPL placements</b><span>{summary.get("ipl_placements", 0)}</span></div>
<div class="card"><b>Collision names</b><span>{summary.get("collision_names", 0)}</span></div>
<div class="card"><b>IMG entries</b><span>{summary.get("img_entries", 0)}</span></div>
{cards}
</div>
<input id="search" placeholder="Filtrar por ID, modelo, risco, textura, status..." oninput="filterRows()">
<table id="results">
<thead><tr>
<th>Risk</th><th>ID</th><th>Kind</th><th>Model</th><th>TXD</th><th>Placements</th><th>Sample XYZ</th><th>DFF</th><th>TXD</th><th>COL</th><th>Notes</th>
</tr></thead>
<tbody>
{rows}
</tbody>
</table>
<div class="lists">
<section><h2>Warnings</h2><ul>{warnings}</ul></section>
<section><h2>Errors</h2><ul>{errors}</ul></section>
</div>
<script>
function filterRows() {{
  const q = document.getElementById('search').value.toLowerCase();
  for (const row of document.querySelectorAll('#results tbody tr')) {{
    row.style.display = row.textContent.toLowerCase().includes(q) ? '' : 'none';
  }}
}}
</script>
</body>
</html>
"""
    path.write_text(html_text, encoding="utf-8")


def risk_rank(risk: str) -> int:
    order = {
        "CRITICAL_INVISIBLE": 0,
        "CRITICAL_COLLISION": 1,
        "MISSING_IDE": 2,
        "MISSING_VISUAL_ASSET": 3,
        "UNKNOWN_COLLISION": 4,
        "WARN_TEXTURE": 5,
        "OK": 9,
    }
    return order.get(risk, 8)


def result_to_row(result: AuditResult) -> str:
    cls = "issue-" + result.risk.lower()
    cells = [
        result.risk,
        str(result.model_id),
        result.kind,
        result.model_name,
        result.txd_name,
        str(result.placement_count),
        result.sample_position,
        result.dff_status,
        result.txd_status,
        result.col_status,
        result.notes,
    ]
    return "<tr class='{0}'>".format(cls) + "".join(f"<td>{escape(value)}</td>" for value in cells) + "</tr>"


def escape(value: object) -> str:
    return html.escape(str(value), quote=True)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Audit GTA SA / SA:MP map objects, visual assets and collision assets.")
    parser.add_argument("--input", "-i", action="append", required=True, help="APK/ZIP/OBB file or extracted folder. Can be used more than once.")
    parser.add_argument("--output", "-o", default="cerebro/map-audit", help="Output folder for summary.json, CSV and HTML reports.")
    parser.add_argument("--html-limit", type=int, default=5000, help="Max rows rendered in HTML. CSV always contains all rows.")
    parser.add_argument("--max-archive-bytes", type=int, default=256 * 1024 * 1024, help="Max bytes to read for IMG/COL entries inside archives.")
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)
    index = AssetIndex()
    for raw_input in args.input:
        scan_path(Path(raw_input), index, args.max_archive_bytes)

    add_context_warnings(index)
    results = build_results(index)
    output_dir = Path(args.output)
    write_reports(index, results, output_dir, args.html_limit)

    counts = Counter(result.risk for result in results)
    print(f"[map-audit] modelos IDE: {len(index.model_defs)}")
    print(f"[map-audit] colocacoes IPL: {len(index.placements)}")
    print(f"[map-audit] entradas IMG: {index.img_entry_count}")
    print(f"[map-audit] nomes COL: {len(index.collision_names)}")
    print(f"[map-audit] riscos: {dict(counts)}")
    if index.errors:
        print(f"[map-audit] erros: {len(index.errors)}")
    if index.warnings:
        print(f"[map-audit] avisos: {len(index.warnings)}")
    print(f"[map-audit] relatorios em: {output_dir.resolve()}")
    return 0 if not index.errors else 2


def add_context_warnings(index: AssetIndex) -> None:
    has_dff = bool(index.assets.get(".dff") or index.img_entries.get(".dff"))
    has_txd = bool(index.assets.get(".txd") or index.img_entries.get(".txd"))
    has_android_texdb = has_android_texdb_assets(index)
    has_map_defs = bool(index.model_defs or index.placements)
    if not has_map_defs:
        index.warnings.append("Nenhum IDE/IPL foi encontrado. Aponte a ferramenta para a pasta/OBB/APK que contem o mapa completo.")
    if index.model_defs and not has_dff:
        index.warnings.append("Foram achadas definicoes IDE, mas nenhum DFF. Isso parece pacote de definicoes sem os modelos visuais.")
    if index.model_defs and not has_txd and has_android_texdb:
        index.warnings.append("Nenhum TXD classico foi achado; parece pacote Android com texdb, entao textura fica como desconhecida via texdb.")
    elif index.model_defs and not has_txd:
        index.warnings.append("Foram achadas definicoes IDE, mas nenhum TXD. Isso parece pacote de definicoes sem as texturas.")
    if index.placements and not index.model_defs:
        index.warnings.append("Foram achados IPLs, mas nenhuma IDE. O relatorio vai marcar IDs como MISSING_IDE.")
    if index.placements and not (index.assets.get(".col") or index.img_entries.get(".col") or index.collision_names):
        index.warnings.append("Foram achados objetos no mapa, mas nenhuma fonte COL. Colisao fica como desconhecida.")


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
