#!/usr/bin/env python
"""Build the PBRM overlay the far-terrain mask gates need, because the shipped
Fallout 4 corpus contains NONE.

Measured 2026-09-11: 14 of 14 landscape textures on the Sanctuary region resolve
`legacy-inverted`; not one Fallout 4 landscape TXST names a `.pbrm`. So the
PBRM arm of bungo's ruling -- *"if PBRM is used to bake it, it gets roughness"*,
*"then also add metallic map, but that should only get derived from PBRM"* --
and the EMISSIVE sheet's present-side have nothing in the game to fire on, and a
gate that only ever sees the absent side is not a gate.

This writes a LOOSE resource folder that layers over the unpacked Data through
`--resource`, touching nothing of the user's own files:

  materials/Landscape/Ground/<stem>.pbrm     a real PBRM v5 envelope
  textures/wwtest/terrain_rmaos.dds          BC1, R/G/B constant per quadrant
  textures/wwtest/terrain_emissive.dds       BC1, one flat colour

The `.pbrm` is found by the resolver's diffuse-stem rule -- `textures\\X_d.dds`
-> `materials\\X_d.pbrm` through `lodmSourceCandidate()`'s own convention -- so
the fixture needs no plugin edit and no TXST that already names a material.

USAGE
  python lodgen_terrain_pbrm_fixture.py <outDir> <diffuse game path> [<diffuse2> ...]

Prints the `.pbrm` paths it wrote and the constants a gate should expect.
"""

import json
import os
import struct
import sys

# The four constants the gate reads back out of the mask sheet. Chosen away from
# 0, 1 and from each other so a channel swap cannot pass: R and G would have to
# land on the same value to alias.
ROUGH = 0.25
METAL = 0.75
AO = 0.60
EMIS = (0.90, 0.20, 0.40)


def dds_bc1(path, w, h, rgb):
    """A flat BC1 texture, one mip, every texel the same colour."""
    r, g, b = [int(round(c * 255)) for c in rgb]
    c565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    # c0 == c1 makes every index decode to the same endpoint, whichever index
    # the block carries -- so the value is exact and does not depend on the
    # interpolation branch
    block = struct.pack('<HHI', c565, c565, 0)
    hdr = bytearray(128)
    hdr[0:4] = b'DDS '
    struct.pack_into('<I', hdr, 4, 124)                 # dwSize
    struct.pack_into('<I', hdr, 8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000)
    struct.pack_into('<I', hdr, 12, h)
    struct.pack_into('<I', hdr, 16, w)
    struct.pack_into('<I', hdr, 20, max(1, w // 4) * max(1, h // 4) * 8)
    struct.pack_into('<I', hdr, 28, 1)                  # mipCount
    struct.pack_into('<I', hdr, 76, 32)                 # pf size
    struct.pack_into('<I', hdr, 80, 0x4)                # DDPF_FOURCC
    hdr[84:88] = b'DXT1'
    struct.pack_into('<I', hdr, 108, 0x1000)            # DDSCAPS_TEXTURE
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(bytes(hdr))
        f.write(block * (max(1, w // 4) * max(1, h // 4)))


def pbrm(path, rmaosTex, emissiveTex):
    doc = {
        'schema': 'FO4.PBRM.Material',
        'schemaVersion': 5,
        'shader': 'Standard',
        'primaryUv': {
            'primaryRmaos': {
                'enabled': True,
                'path': rmaosTex,
                'values': {
                    # overrides OFF means "sample the texture", which is what
                    # puts RmaosRoughness and RmaosMetallic in the feature mask
                    'overrideRoughness': False,
                    'overrideMetallic': False,
                    'overrideAo': False,
                    'roughness': ROUGH,
                    'metallic': METAL,
                    'ao': AO,
                },
            },
            'primaryEmissive': {
                'enabled': True,
                'path': emissiveTex,
                'values': {'color': list(EMIS), 'intensity': 1.0},
            },
        },
    }
    payload = json.dumps(doc, separators=(',', ':')).encode('utf-8')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(b'PBRM')
        f.write(struct.pack('<II', 5, len(payload)))
        f.write(payload)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    out = argv[1]
    rm = 'wwtest/terrain_rmaos.dds'
    em = 'wwtest/terrain_emissive.dds'
    dds_bc1(os.path.join(out, 'textures', 'wwtest', 'terrain_rmaos.dds'),
            64, 64, (ROUGH, METAL, AO))
    dds_bc1(os.path.join(out, 'textures', 'wwtest', 'terrain_emissive.dds'),
            64, 64, EMIS)
    wrote = []
    for d in argv[2:]:
        p = d.replace('\\', '/')
        if p.lower().startswith('data/'):
            p = p[5:]
        if p.lower().startswith('textures/'):
            p = p[9:]
        stem = os.path.splitext(p)[0]
        mp = os.path.join(out, 'materials', stem.replace('/', os.sep) + '.pbrm')
        pbrm(mp, rm, em)
        wrote.append(mp)
    # BC1 quantises 8-bit to 5:6:5, so the gate's expected bytes are the
    # ROUND TRIP of the constants, not the constants
    def q(v, bits):
        n = (1 << bits) - 1
        return round(round(v * 255) >> (8 - bits)) * 255 // n
    print('pbrm files: %d' % len(wrote))
    for w in wrote:
        print('  %s' % w)
    print('expect roughness %d metallic %d ao %d (8-bit, after BC1 5:6:5)'
          % (q(ROUGH, 5), q(METAL, 6), q(AO, 5)))
    print('expect emissive %d %d %d'
          % (q(EMIS[0], 5), q(EMIS[1], 6), q(EMIS[2], 5)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
