#!/usr/bin/env python3
"""Extract the OV5640 init register table from the ST official driver
(stm32-ov5640: ov5640.c + ov5640_reg.h) and emit a compact C table.

Reads the two files from the current directory (or paths given as argv),
resolves register-name references to addresses, applies the QVGA window
override, and prints the final {addr, value} list as C source.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OV5640_C = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "..", "..", "..",
                                                              "Users", "201209", "AppData", "Local", "Temp", "ov5640.c")
REG_H = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", "..", "..", "..",
                                                           "Users", "201209", "AppData", "Local", "Temp", "ov5640_reg.h")


def load_default_paths():
    """Try to locate the ST files in TEMP automatically."""
    import glob
    for pat in [os.path.join(os.environ.get("TEMP", ""), "ov5640.c"),
                os.path.join(os.environ.get("TEMP", ""), "dsh-*", "ov5640.c")]:
        hits = glob.glob(pat)
        if hits:
            return hits[0]
    return OV5640_C


def main():
    c_path = load_default_paths()
    if not os.path.exists(c_path):
        print(f"ov5640.c not found ({c_path}); pass paths as argv")
        return 1
    reg_path = REG_H
    if not os.path.exists(reg_path):
        reg_path = os.path.join(os.environ.get("TEMP", ""), "ov5640_reg.h")
    if not os.path.exists(reg_path):
        print(f"ov5640_reg.h not found; pass path as argv")
        return 1

    # 1. register address map
    regs = {}
    with open(reg_path, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"\s*#define\s+(OV5640_\w+)\s+((?:0x[0-9A-Fa-f]+|\d+)(?:UL|U)?)", line)
            if m:
                val = m.group(2).replace("UL", "").replace("U", "")
                regs[m.group(1)] = int(val, 0)

    # 2. extract common table entries from ov5640.c
    c_src = open(c_path, encoding="utf-8").read()
    # find OV5640_Common[][2] = { ... };
    m_common = re.search(r"OV5640_Common\[\]\[2\]\s*=\s*\{(.*?)\n\s*\};", c_src, re.S)
    if not m_common:
        print("common table not found")
        return 1
    entries = []
    for line in m_common.group(1).splitlines():
        mm = re.search(r"\{\s*((?:0x[0-9A-Fa-f]+)|(OV5640_\w+))\s*,\s*(0x[0-9A-Fa-f]+)\s*\}", line)
        if mm:
            reg = mm.group(1)
            addr = int(reg, 0) if reg.startswith("0x") else regs.get(reg)
            if addr is None:
                print(f"UNKNOWN REG {reg} (line: {line.strip()})")
                return 1
            entries.append((addr, int(mm.group(3), 0)))

    # 3. QVGA window override (DVPHO/DVPVO -> 320x240)
    m_qvga = re.search(r"OV5640_QVGA\[\]\[2\]\s*=\s*\{(.*?)\n\s*\};", c_src, re.S)
    if not m_qvga:
        print("QVGA table not found")
        return 1
    override = {}
    for line in m_qvga.group(1).splitlines():
        mm = re.search(r"\{\s*(OV5640_\w+)\s*,\s*(0x[0-9A-Fa-f]+)\s*\}", line)
        if mm:
            override[regs[mm.group(1)]] = int(mm.group(2), 0)

    # 4. apply overrides
    for addr, val in override.items():
        for i, (a, v) in enumerate(entries):
            if a == addr:
                entries[i] = (a, val)
                break
        else:
            entries.append((addr, val))

    # 5. emit C
    print(f"// {len(entries)} registers (ST stm32-ov5640, RGB565, QVGA 320x240)")
    print("static const uint16_t s_ov5640_init[][2] = {")
    for i in range(0, len(entries), 4):
        row = ", ".join(f"{{0x{a:04X}, 0x{v:02X}}}" for a, v in entries[i:i + 4])
        print(f"    {row},")
    print("};")
    print(f"// {len(entries)} entries total")
    return 0


if __name__ == "__main__":
    sys.exit(main())
