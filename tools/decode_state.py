#!/usr/bin/env python3
"""Decode a 20-byte Dual Start Button ButtonState packet from a hex string."""
import argparse
import struct

FLAGS = [
    (1 << 0, "pressed"),
    (1 << 1, "armed"),
    (1 << 2, "linked"),
    (1 << 3, "long_pressed"),
    (1 << 4, "connected"),
    (1 << 5, "error"),
]

TYPES = {
    1: "state",
    2: "heartbeat",
    3: "boot",
    4: "link",
    5: "error",
}


def decode(data: bytes) -> dict[str, object]:
    """Decode a 20-byte ButtonState packet into a field dict.

    Raises SystemExit on wrong-length input, matching CLI behavior.
    """
    if len(data) != 20:
        raise SystemExit(f"expected 20 bytes, got {len(data)}")
    version, typ, flags, slot, seq, uptime, device_hash, group, aux = struct.unpack("<BBBBHIIIH", data)
    active_flags = [name for bit, name in FLAGS if flags & bit]
    return {
        "version": version,
        "type": TYPES.get(typ, typ),
        "flags": active_flags,
        "slot": slot,
        "seq": seq,
        "uptime_ms": uptime,
        "device_hash": device_hash,
        "link_group_id": group,
        "aux": aux,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("hex", help="20-byte packet, e.g. 01 02 16 01 ...")
    args = parser.parse_args()
    data = bytes.fromhex(args.hex)
    print(decode(data))


if __name__ == "__main__":
    main()
