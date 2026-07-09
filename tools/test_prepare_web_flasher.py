import json
from pathlib import Path

from prepare_web_flasher import build_manifest, copy_web_assets, sanitize_tag


def test_sanitize_tag_keeps_release_friendly_chars():
    assert sanitize_tag(" v0.2.0 ") == "v0.2.0"
    assert sanitize_tag("release candidate/1") == "release-candidate-1"
    assert sanitize_tag("///") == "local"


def test_build_manifest_points_at_merged_esp32_image():
    manifest = build_manifest(version="v0.2.0", merged_name="firmware.bin")

    assert manifest["name"] == "Dual Start Button"
    assert manifest["version"] == "v0.2.0"
    assert manifest["new_install_improv_wait_time"] == 0
    build = manifest["builds"][0]
    assert build["chipFamily"] == "ESP32"
    assert build["improv"] is False
    assert build["parts"] == [{"path": "firmware.bin", "offset": 0}]

    json.dumps(manifest)


def test_copy_web_assets_requires_expected_static_files(tmp_path):
    source = tmp_path / "source"
    output = tmp_path / "output"
    source.mkdir()
    output.mkdir()
    for name in ("index.html", "app.js", "style.css"):
        (source / name).write_text(name, encoding="utf-8")

    copy_web_assets(Path(source), Path(output))

    assert sorted(path.name for path in output.iterdir()) == ["app.js", "index.html", "style.css"]
