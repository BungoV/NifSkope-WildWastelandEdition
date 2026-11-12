"""Lane VKCOMPUTE1 (2026-11-12): write a SPIR-V file as the C array glslangValidator --vn writes
(`const uint32_t <name>[] = { 0x..., };`, eight words a line), so a DXC-built kernel drops into the same
#include as a glslang one. Usage: python -I tools/spv_header.py <in .spv> <name> <out .h>"""
import struct
import sys


def main():
    src, name, out = sys.argv[1], sys.argv[2], sys.argv[3]
    data = open(src, 'rb').read()
    if len(data) % 4 or len(data) < 20 or struct.unpack_from('<I', data, 0)[0] != 0x07230203:
        print('spv_header: %s is not SPIR-V' % src)
        return 1
    words = struct.unpack('<%dI' % (len(data) // 4), data)
    lines = ['\t// DXC (tools/spv_header.py)', '\t #pragma once', 'const uint32_t %s[] = {' % name]
    for i in range(0, len(words), 8):
        lines.append('\t' + ','.join('0x%08x' % w for w in words[i:i + 8]) + ',')
    lines.append('};')
    with open(out, 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print('spv_header: %s %d words -> %s' % (name, len(words), out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
