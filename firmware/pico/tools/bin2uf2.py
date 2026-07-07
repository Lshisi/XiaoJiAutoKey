#!/usr/bin/env python3
import struct
import sys
from pathlib import Path


UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
RP2040_FAMILY_ID = 0xE48BFF56
FLASH_BASE = 0x10000000
PAYLOAD_SIZE = 256


def block(payload: bytes, target_addr: int, block_no: int, num_blocks: int) -> bytes:
    payload = payload.ljust(PAYLOAD_SIZE, b"\x00")
    header = struct.pack(
        "<IIIIIIII",
        UF2_MAGIC_START0,
        UF2_MAGIC_START1,
        UF2_FLAG_FAMILY_ID_PRESENT,
        target_addr,
        PAYLOAD_SIZE,
        block_no,
        num_blocks,
        RP2040_FAMILY_ID,
    )
    return header + payload + bytes(512 - len(header) - PAYLOAD_SIZE - 4) + struct.pack("<I", UF2_MAGIC_END)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: bin2uf2.py input.bin output.uf2", file=sys.stderr)
        return 2

    src = Path(sys.argv[1])
    dst = Path(sys.argv[2])
    data = src.read_bytes()
    num_blocks = (len(data) + PAYLOAD_SIZE - 1) // PAYLOAD_SIZE
    with dst.open("wb") as out:
        for block_no in range(num_blocks):
            start = block_no * PAYLOAD_SIZE
            out.write(block(data[start:start + PAYLOAD_SIZE], FLASH_BASE + start, block_no, num_blocks))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
