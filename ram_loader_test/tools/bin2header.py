#!/usr/bin/env python3
"""
bin2header.py - Convert a binary file to a C header with an array.

Usage:
    python3 bin2header.py <input.bin> <output.h> <array_name>

Generates a C header file containing the binary data as a const uint8_t
array and a length constant.
"""

import sys
import os


def bin2header(input_path, output_path, array_name):
    with open(input_path, "rb") as f:
        data = f.read()

    with open(output_path, "w") as f:
        guard = f"_{array_name.upper()}_H_"
        f.write(f"/* Auto-generated from {os.path.basename(input_path)} */\n")
        f.write(f"/* DO NOT EDIT - regenerated at build time */\n\n")
        f.write(f"#ifndef {guard}\n")
        f.write(f"#define {guard}\n\n")
        f.write(f"#include <stdint.h>\n\n")
        f.write(f"static const uint8_t {array_name}[] = {{\n")

        # Write 16 bytes per line
        for i in range(0, len(data), 16):
            chunk = data[i : i + 16]
            hex_values = ", ".join(f"0x{b:02x}" for b in chunk)
            f.write(f"    {hex_values},\n")

        f.write(f"}};\n\n")
        f.write(
            f"static const uint32_t {array_name}_len = sizeof({array_name});\n\n"
        )
        f.write(f"#endif /* {guard} */\n")

    print(f"Generated {output_path}: {len(data)} bytes -> {array_name}[]")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.h> <array_name>")
        sys.exit(1)

    bin2header(sys.argv[1], sys.argv[2], sys.argv[3])
