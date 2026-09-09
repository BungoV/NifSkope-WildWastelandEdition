"""Fixtures for tests/spells/lodgen_resources.sh.

Two mod folders holding the SAME relative texture with different bytes, a Data
folder with two plugin files for names to resolve against, and a plugins.txt in
Mod Organizer's shape: '*' marks an enabled plugin, an unstarred line is
disabled, '#' is a comment.

The DDS files are real 4x4 BC1 headers so nothing downstream has to pretend;
only their single block differs, which is what makes the two hashes distinct.

USAGE  python lodgen_resources_fixture.py <workdir>
"""
import os
import struct
import sys


def dds_bc1(block):
    """A 4x4 DXT1 DDS: 128-byte header then one 8-byte block."""
    header = bytearray(128)
    header[0:4] = b'DDS '
    struct.pack_into('<I', header, 4, 124)              # dwSize
    struct.pack_into('<I', header, 8, 0x1007)           # CAPS|HEIGHT|WIDTH|PIXELFORMAT
    struct.pack_into('<I', header, 12, 4)               # height
    struct.pack_into('<I', header, 16, 4)               # width
    struct.pack_into('<I', header, 20, 8)               # pitch (one block)
    struct.pack_into('<I', header, 28, 1)               # mip count
    struct.pack_into('<I', header, 76, 32)              # pf size
    struct.pack_into('<I', header, 80, 0x4)             # DDPF_FOURCC
    header[84:88] = b'DXT1'
    struct.pack_into('<I', header, 108, 0x1000)         # DDSCAPS_TEXTURE
    return bytes(header) + block


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)


def main():
    w = sys.argv[1]
    rel = 'textures/lod/x_d.dds'
    # two blocks that differ in every byte, so no hash collision is plausible
    write(os.path.join(w, 'modA', *rel.split('/')),
          dds_bc1(b'\x00\x00\xff\xff\x00\x00\x00\x00'))
    write(os.path.join(w, 'modB', *rel.split('/')),
          dds_bc1(b'\xff\xff\x00\x00\xaa\xaa\xaa\xaa'))

    data = os.path.join(w, 'data')
    os.makedirs(data, exist_ok=True)
    for name in ('Fallout4.esm', 'Test.esp', 'Disabled.esp'):
        with open(os.path.join(data, name), 'wb') as f:
            f.write(b'TES4')

    with open(os.path.join(w, 'plugins.txt'), 'w', newline='\r\n') as f:
        f.write('# This file is used by the game to keep track of your downloaded content.\n')
        f.write('*Fallout4.esm\n')
        f.write('Disabled.esp\n')
        f.write('*Test.esp\n')
    print('fixtures written under %s' % w)


if __name__ == '__main__':
    main()
