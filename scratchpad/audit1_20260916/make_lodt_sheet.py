"""AUDIT1 step 7: render one .lodt tile pyramid to a PNG contact sheet.

The picture is not decoration: it is the colour ROLE of a real VT container
decoded by the tree's own independent reader (tests/spells/lodgen_vt_check.py,
Lodv + decode_bc1), tile by tile, laid out at the tile grid the header
declares. Nothing in the exe draws it, so a container that decodes to garbage
shows as garbage rather than as a renderer's idea of the file.

usage: python make_lodt_sheet.py <file.lodt> <out.png> [mip]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SPELLS = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'tests', 'spells')
sys.path.insert(0, SPELLS)
import lodgen_vt_check as V                                  # noqa: E402
from PIL import Image                                        # noqa: E402


def main():
    path, out = sys.argv[1], sys.argv[2]
    mip = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    v = V.Lodv(path)
    side = v.stored >> mip
    img = Image.new('RGB', (v.tilesX * side, v.tilesY * side), (24, 24, 24))
    drawn = 0
    for i, e in enumerate(v.table):
        rows = v.colour(i, mip)
        if rows is None:
            continue
        t = Image.new('RGB', (side, side))
        t.putdata([px for r in rows for px in r])
        img.paste(t, ((i % v.tilesX) * side, (i // v.tilesX) * side))
        drawn += 1
    img.save(out)
    print('%s: level %d, %dx%d tiles, %d of %d decoded, mip %d (%d px a tile) -> %s %d B'
          % (os.path.basename(path), v.levelDim, v.tilesX, v.tilesY, drawn,
             v.tileCount, mip, side, os.path.basename(out), os.path.getsize(out)))


main()
