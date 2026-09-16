#!/usr/bin/env python3
"""
Brainfuino Test Program Generator
Generates unrolled sequential number streams in Brainfuck to visually test
large Flash ROM streaming integrity and verify that 0 bytes were dropped.
"""

import sys

def generate_test_program(target_bytes=12000, output_path="tests/test_sequence_12k.bf"):
    blocks = []
    total_bytes = 0
    num = 1
    
    # Initialize cells
    header = "++++++++++>+<[-]>[-]"
    blocks.append(header)
    total_bytes += len(header)
    
    while total_bytes < target_bytes:
        tag = f"{num:04d} "
        part = []
        cur = 0
        for ch in tag:
            diff = ord(ch) - cur
            if diff > 0:
                part.append("+" * diff)
            else:
                part.append("-" * (-diff))
            part.append(".")
            cur = ord(ch)
        part.append("[-]\n")
        block_str = "".join(part)
        blocks.append(block_str)
        total_bytes += len(block_str)
        num += 1
        
    code = "".join(blocks)
    with open(output_path, "w") as f:
        f.write(code)
    print(f"Generated {len(code)} bytes with {num - 1} numbers into {output_path}")

if __name__ == "__main__":
    size = int(sys.argv[1]) if len(sys.argv) > 1 else 12000
    out = sys.argv[2] if len(sys.argv) > 2 else "tests/test_sequence_12k.bf"
    generate_test_program(size, out)
