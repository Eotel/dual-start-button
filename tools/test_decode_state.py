"""Tests for tools/decode_state.py driven by the shared golden vectors.

The vector files in test-vectors/ are the cross-implementation protocol
contract (also consumed by the C++ and JavaScript suites). These tests only
exercise the public surface: decode(), the CLI, and the vector files.
"""

import json
import struct
import subprocess
import sys
from pathlib import Path

import pytest

from decode_state import decode

REPO_ROOT = Path(__file__).resolve().parent.parent
VECTORS = REPO_ROOT / "test-vectors"
SCRIPT = Path(__file__).with_name("decode_state.py")

# Field-name contract from SPEC.md section 8, defined here independently of
# decode_state's internal tables so the test genuinely pins decode's output.
TYPE_NAMES = {1: "state", 2: "heartbeat", 3: "boot", 4: "link", 5: "error"}
FLAG_BITS = [
    (1 << 0, "pressed"),
    (1 << 1, "armed"),
    (1 << 2, "linked"),
    (1 << 3, "long_pressed"),
    (1 << 4, "connected"),
    (1 << 5, "error"),
]

# The exact packet documented in AGENTS.md and SPEC.md section 8.
AGENTS_MD_SAMPLE = "01 02 16 01 2a 00 10 27 00 00 78 56 34 12 ef cd ab 00 00 00"


def _load(name: str) -> dict:
    return json.loads((VECTORS / name).read_text())


BUTTON_STATE = _load("button_state.json")
CONTROL = _load("control_command.json")


def _ids(cases: list[dict]) -> list[str]:
    return [c["name"] for c in cases]


@pytest.mark.parametrize("case", BUTTON_STATE["valid"], ids=_ids(BUTTON_STATE["valid"]))
def test_button_state_hex_matches_expected_fields(case: dict) -> None:
    """Repacking the expected fields must reproduce the vector hex.

    Catches hand-authored hex mistakes in the vector file.
    """
    e = case["expect"]
    packed = struct.pack(
        "<BBBBHIIIH",
        e["version"],
        e["type"],
        e["flags"],
        e["link_slot"],
        e["seq"],
        e["uptime_ms"],
        e["device_hash"],
        e["link_group_id"],
        e["aux"],
    )
    assert packed == bytes.fromhex(case["hex"])


@pytest.mark.parametrize("case", BUTTON_STATE["valid"], ids=_ids(BUTTON_STATE["valid"]))
def test_button_state_decode_matches(case: dict) -> None:
    e = case["expect"]
    expected = {
        "version": e["version"],
        "type": TYPE_NAMES[e["type"]],
        "flags": [name for bit, name in FLAG_BITS if e["flags"] & bit],
        "slot": e["link_slot"],
        "seq": e["seq"],
        "uptime_ms": e["uptime_ms"],
        "device_hash": e["device_hash"],
        "link_group_id": e["link_group_id"],
        "aux": e["aux"],
    }
    assert decode(bytes.fromhex(case["hex"])) == expected


@pytest.mark.parametrize("case", BUTTON_STATE["invalid"], ids=_ids(BUTTON_STATE["invalid"]))
def test_button_state_wrong_length_rejected(case: dict) -> None:
    with pytest.raises(ValueError):
        decode(bytes.fromhex(case["hex"]))


@pytest.mark.parametrize("case", CONTROL["valid"], ids=_ids(CONTROL["valid"]))
def test_control_hex_matches_expected_fields(case: dict) -> None:
    e = case["expect"]
    packed = struct.pack(
        "<BBBBII",
        e["version"],
        e["command"],
        e["slot"],
        e["flags"],
        e["group_id"],
        e["value"],
    )
    assert packed == bytes.fromhex(case["hex"])


@pytest.mark.parametrize("case", CONTROL["invalid"], ids=_ids(CONTROL["invalid"]))
def test_control_invalid_rejected_by_contract(case: dict) -> None:
    # Firmware decodeControlCommand accepts a packet only when it is exactly
    # 12 bytes and version == 1; every invalid vector must fail that gate.
    data = bytes.fromhex(case["hex"])
    accepted = len(data) == 12 and data[0] == 1
    assert not accepted


def test_cli_decodes_agents_md_sample() -> None:
    result = subprocess.run(
        [sys.executable, str(SCRIPT), AGENTS_MD_SAMPLE],
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0
    assert "'type': 'heartbeat'" in result.stdout
    assert "'device_hash': 305419896" in result.stdout
    assert "'link_group_id': 11259375" in result.stdout


def test_cli_wrong_length_exits_nonzero() -> None:
    result = subprocess.run(
        [sys.executable, str(SCRIPT), "01 02 03"],
        capture_output=True,
        text=True,
    )
    assert result.returncode != 0
    assert "expected 20 bytes, got 3" in result.stderr
