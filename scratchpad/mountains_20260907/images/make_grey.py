"""Write a flat mid-grey BC1 sheet with a full mip chain.

The render hook has no "normal map only" view (the LOD channels are 1..13 and
none of them lights the surface with the normal map; channel 8 is the
GEOMETRIC normal, not the sheet).  So the diffuse is removed by substituting a
sheet that carries no information: one constant colour, byte-identical on both
halves, so the only thing left that can shade the surface is the _msn.

BC1 block: colour0 (565 LE), colour1 (565 LE), 4 index bytes.  colour0 >
colour1 selects the 4-colour opaque mode and every index 0 selects colour0, so
every texel of every mip is exactly colour0.

  colour0 = 0x8410 -> r 16, g 32, b 16 -> 8-bit (132, 130, 132)

which is the nearest 565 value to 128 grey.

  python make_grey.py <headerDonor.DDS> <out.DDS>

The 128-byte header is copied from a shipped 512x512 DXT1 sheet with 8 mips
rather than hand-written, so the file cannot disagree with the ones the
renderer already accepts.  The payload is regenerated in full.
"""
import struct
import sys

BLOCK = struct.pack('<HHI', 0x8410, 0x0000, 0x00000000)


def main():
    donor, out = sys.argv[1], sys.argv[2]
    b = open(donor, 'rb').read()
    assert b[:4] == b'DDS ', donor
    assert b[84:88] == b'DXT1', b[84:88]
    h, w = struct.unpack_from('<II', b, 12)
    mips = struct.unpack_from('<I', b, 28)[0] or 1
    assert (w, h) == (512, 512), (w, h)

    payload = bytearray()
    for i in range(mips):
        mw, mh = max(1, w >> i), max(1, h >> i)
        nblocks = max(1, (mw + 3) // 4) * max(1, (mh + 3) // 4)
        payload += BLOCK * nblocks
    data = b[:128] + bytes(payload)
    assert len(data) == len(b), (len(data), len(b))
    open(out, 'wb').write(data)
    print('%s  %dx%d  %d mips  %d bytes' % (out, w, h, mips, len(data)))


if __name__ == '__main__':
    main()
