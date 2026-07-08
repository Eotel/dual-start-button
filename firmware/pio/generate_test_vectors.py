"""Generate a C header of protocol test vectors from the shared JSON files.

The files in ../../test-vectors are the single cross-implementation contract
(also consumed by the Python and JavaScript suites). This script turns them
into C arrays so the native Unity tests never hand-copy vector values.

It runs two ways:

* As a PlatformIO ``extra_scripts = pre:`` hook on the ``native`` env. SCons
  exec's this file with ``Import``/``env`` in its globals but no ``__file__``,
  so paths come from ``env["PROJECT_DIR"]``. The header is regenerated before
  every ``pio test -e native`` build.
* Standalone (``python generate_test_vectors.py``), for local regeneration or
  a sync check, where ``__file__`` is available.

The generated header lives under test/generated/ and is git-ignored.
"""
import json
import os


def _paths(project_dir):
    """Return (vectors_dir, out_path) for a firmware/pio project directory."""
    vectors_dir = os.path.normpath(os.path.join(project_dir, "..", "..", "test-vectors"))
    out_path = os.path.join(project_dir, "test", "generated", "test_vectors.h")
    return vectors_dir, out_path


def _load(vectors_dir, name):
    with open(os.path.join(vectors_dir, name), encoding="utf-8") as fh:
        return json.load(fh)


def _bytes_literal(hex_str):
    data = bytes.fromhex(hex_str)
    if not data:
        return "{0}", 0
    return "{" + ", ".join(f"0x{b:02x}" for b in data) + "}", len(data)


def _button_state_rows(cases):
    rows = []
    for c in cases:
        e = c["expect"]
        literal, _ = _bytes_literal(c["hex"])
        rows.append(
            "  {{ \"{name}\", {bytes}, {version}, {type}, {flags}, {link_slot}, "
            "{seq}, {uptime_ms}u, {device_hash}u, {link_group_id}u, {aux} }},".format(
                name=c["name"], bytes=literal, **e
            )
        )
    return rows


def _control_rows(cases):
    rows = []
    for c in cases:
        e = c["expect"]
        literal, _ = _bytes_literal(c["hex"])
        rows.append(
            "  {{ \"{name}\", {bytes}, {version}, {command}, {slot}, {flags}, "
            "{group_id}u, {value}u }},".format(name=c["name"], bytes=literal, **e)
        )
    return rows


def _raw_rows(cases):
    rows = []
    for c in cases:
        literal, length = _bytes_literal(c["hex"])
        rows.append(f'  {{ "{c["name"]}", {literal}, {length} }},')
    return rows


def render(vectors_dir):
    button_state = _load(vectors_dir, "button_state.json")
    control = _load(vectors_dir, "control_command.json")

    lines = [
        "// AUTO-GENERATED from ../../test-vectors/*.json by generate_test_vectors.py.",
        "// Do not edit by hand. Regenerate with: pio test -e native",
        "// (or: python firmware/pio/generate_test_vectors.py).",
        "#pragma once",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "",
        "typedef struct {",
        "  const char* name;",
        "  uint8_t bytes[20];",
        "  uint8_t version;",
        "  uint8_t type;",
        "  uint8_t flags;",
        "  uint8_t link_slot;",
        "  uint16_t seq;",
        "  uint32_t uptime_ms;",
        "  uint32_t device_hash;",
        "  uint32_t link_group_id;",
        "  uint16_t aux;",
        "} DsbButtonStateVector;",
        "",
        "static const DsbButtonStateVector kDsbButtonStateValid[] = {",
        *_button_state_rows(button_state["valid"]),
        "};",
        "static const size_t kDsbButtonStateValidCount =",
        "    sizeof(kDsbButtonStateValid) / sizeof(kDsbButtonStateValid[0]);",
        "",
        "typedef struct {",
        "  const char* name;",
        "  uint8_t bytes[12];",
        "  uint8_t version;",
        "  uint8_t command;",
        "  uint8_t slot;",
        "  uint8_t flags;",
        "  uint32_t group_id;",
        "  uint32_t value;",
        "} DsbControlVector;",
        "",
        "static const DsbControlVector kDsbControlValid[] = {",
        *_control_rows(control["valid"]),
        "};",
        "static const size_t kDsbControlValidCount =",
        "    sizeof(kDsbControlValid) / sizeof(kDsbControlValid[0]);",
        "",
        "// Malformed packets (wrong length or version). len is the real byte count;",
        "// bytes is padded to the widest invalid case.",
        "typedef struct {",
        "  const char* name;",
        "  uint8_t bytes[24];",
        "  size_t len;",
        "} DsbRawVector;",
        "",
        "static const DsbRawVector kDsbControlInvalid[] = {",
        *_raw_rows(control["invalid"]),
        "};",
        "static const size_t kDsbControlInvalidCount =",
        "    sizeof(kDsbControlInvalid) / sizeof(kDsbControlInvalid[0]);",
        "",
    ]
    return "\n".join(lines)


def write_header(project_dir):
    vectors_dir, out_path = _paths(project_dir)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as fh:
        fh.write(render(vectors_dir))
    return out_path


# SCons injects `Import`/`env` (but not `__file__`) when this runs as a
# PlatformIO extra_script; detect that by looking for `Import` in globals.
if "Import" in globals():
    Import("env")  # noqa: F821  (SCons builtin)
    write_header(env["PROJECT_DIR"])  # noqa: F821  (exported by Import)
elif __name__ == "__main__":
    print(f"wrote {write_header(os.path.dirname(os.path.abspath(__file__)))}")
