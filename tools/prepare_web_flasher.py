#!/usr/bin/env python3
"""Prepare ESP Web Tools artifacts from a PlatformIO ESP32 build."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

DEFAULT_ENV = "m5stick_c"
DEFAULT_FLASH_MODE = "dio"
DEFAULT_FLASH_FREQ = "40m"
DEFAULT_FLASH_SIZE = "4MB"

WEB_ASSETS = ("index.html", "app.js", "style.css")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def sanitize_tag(tag: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9._-]+", "-", tag.strip())
    return cleaned.strip("-") or "local"


def build_manifest(*, version: str, merged_name: str) -> dict[str, object]:
    return {
        "name": "Dual Start Button",
        "version": version,
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 0,
        "builds": [
            {
                "chipFamily": "ESP32",
                "improv": False,
                "parts": [{"path": merged_name, "offset": 0}],
            }
        ],
    }


def require_file(path: Path, label: str) -> Path:
    if not path.is_file():
        raise FileNotFoundError(f"{label} not found: {path}")
    return path


def find_boot_app0(project_dir: Path, env: str) -> Path:
    candidates = [
        project_dir / ".pio" / "build" / env / "boot_app0.bin",
        Path.home()
        / ".platformio"
        / "packages"
        / "framework-arduinoespressif32"
        / "tools"
        / "partitions"
        / "boot_app0.bin",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate

    package_root = Path.home() / ".platformio" / "packages"
    matches = sorted(
        package_root.glob("framework-arduinoespressif32*/tools/partitions/boot_app0.bin")
    )
    if matches:
        return matches[0]

    raise FileNotFoundError("boot_app0.bin not found under PlatformIO build or package directories")


def find_esptool() -> Path:
    package_path = Path.home() / ".platformio" / "packages" / "tool-esptoolpy" / "esptool.py"
    if package_path.is_file():
        return package_path

    found = shutil.which("esptool.py")
    if found:
        return Path(found)

    raise FileNotFoundError("esptool.py not found; run a PlatformIO ESP32 build first")


def collect_parts(project_dir: Path, env: str) -> list[tuple[str, Path]]:
    build_dir = project_dir / ".pio" / "build" / env
    return [
        ("0x1000", require_file(build_dir / "bootloader.bin", "bootloader")),
        ("0x8000", require_file(build_dir / "partitions.bin", "partition table")),
        ("0xe000", find_boot_app0(project_dir, env)),
        ("0x10000", require_file(build_dir / "firmware.bin", "firmware image")),
    ]


def merge_esp32_image(
    *,
    esptool: Path,
    parts: list[tuple[str, Path]],
    output: Path,
    flash_mode: str,
    flash_freq: str,
    flash_size: str,
) -> None:
    command = [
        sys.executable,
        str(esptool),
        "--chip",
        "esp32",
        "merge_bin",
        "-o",
        str(output),
        "--flash_mode",
        flash_mode,
        "--flash_freq",
        flash_freq,
        "--flash_size",
        flash_size,
    ]
    for offset, part_path in parts:
        command.extend([offset, str(part_path)])
    subprocess.run(command, check=True)


def copy_web_assets(web_source: Path, output_dir: Path) -> None:
    for asset in WEB_ASSETS:
        shutil.copyfile(require_file(web_source / asset, asset), output_dir / asset)


def write_manifest(output_dir: Path, version: str, merged_name: str) -> Path:
    manifest_path = output_dir / "manifest.json"
    manifest = build_manifest(version=version, merged_name=merged_name)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest_path


def parse_args() -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--env", default=DEFAULT_ENV, help="PlatformIO environment to package")
    parser.add_argument("--tag", default="local", help="Version/tag to include in artifact names")
    parser.add_argument("--project-dir", type=Path, default=root / "firmware" / "pio")
    parser.add_argument("--web-source", type=Path, default=root / "web-flasher")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--flash-mode", default=DEFAULT_FLASH_MODE)
    parser.add_argument("--flash-freq", default=DEFAULT_FLASH_FREQ)
    parser.add_argument("--flash-size", default=DEFAULT_FLASH_SIZE)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    version = sanitize_tag(args.tag)
    output_dir = args.output_dir or repo_root() / "dist" / "web-flasher" / args.env
    output_dir.mkdir(parents=True, exist_ok=True)

    merged_name = f"dual-start-button-{args.env}-{version}-merged.bin"
    merged_path = output_dir / merged_name
    parts = collect_parts(args.project_dir, args.env)
    merge_esp32_image(
        esptool=find_esptool(),
        parts=parts,
        output=merged_path,
        flash_mode=args.flash_mode,
        flash_freq=args.flash_freq,
        flash_size=args.flash_size,
    )
    copy_web_assets(args.web_source, output_dir)
    manifest_path = write_manifest(output_dir, version, merged_name)

    print(f"wrote {merged_path}")
    print(f"wrote {manifest_path}")
    print(f"copied web assets to {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
