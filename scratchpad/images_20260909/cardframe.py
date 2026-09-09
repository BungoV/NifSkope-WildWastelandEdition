"""Cut ONE frame out of an octahedral card sheet and put it beside the renders
of the source model taken from the SAME directions.

The frame-to-direction law is the bake's own (src/nifskope_ui.cpp ~21762):

    u = i/(N-1)*2 - 1 ,  v = j/(N-1)*2 - 1
    dx = (u+v)/2 , dy = (u-v)/2 , dz = 1 - |dx| - |dy| , normalised
    camera = setRotation( -90 + elevation, 0, 90 - azimuth )

and the viewer's own axis rotations (src/glview.cpp:186) are Top (0,0,0),
Left (-90,0,-90), Right (-90,0,90), Front (-90,0,180), Back (-90,0,0). Solving
the two against each other gives, for any N, three EXACT horizon pairings:

    ViewLeft  (WW_RENDER_VIEW=3)  <->  frame (0,   0)
    ViewFront (WW_RENDER_VIEW=5)  <->  frame (0,   N-1)
    ViewRight (WW_RENDER_VIEW=4)  <->  frame (N-1, N-1)

  python cardframe.py <cards dir> <formid> <i> <j> <out.png> [zoom]
"""
import os, sys
from PIL import Image

BG = (18, 18, 20)


def read_oct(path):
    for line in open(path, encoding='utf-8-sig'):
        p = line.split()
        if p and p[0] == 'oct':
            return int(p[1]), int(p[2]), int(p[3])
    raise SystemExit('no oct line in %s' % path)


def main():
    cards, fid, i, j, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
    zoom = int(sys.argv[6]) if len(sys.argv) > 6 else 4
    n, fw, fh = read_oct(os.path.join(cards, fid + '.txt'))
    sheet = Image.open(os.path.join(cards, '%s_oct_albedo.png' % fid)).convert('RGBA')
    # the sheet may be written at half of each side; scale the frame with it
    sw, sh = sheet.width // n, sheet.height // n
    box = (i * sw, j * sh, (i + 1) * sw, (j + 1) * sh)
    fr = sheet.crop(box)
    flat = Image.new('RGB', fr.size, BG)
    flat.paste(fr, (0, 0), fr)
    flat = flat.resize((fr.width * zoom, fr.height * zoom), Image.NEAREST)
    flat.save(out, optimize=True)
    print('%s  frame (%d,%d) of %dx%d  %dx%d texels -> %dx%d' %
          (out, i, j, n, n, sw, sh, flat.width, flat.height))


if __name__ == '__main__':
    main()
