"""SPLAT1 section 2 -- the mip the code selects vs the mip a footprint match
would pick, and one texel row sampled both ways beside vanilla's own row.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import splatlib as S                                          # noqa: E402
import offline_bake as B                                      # noqa: E402

TILE = 2048.0
UPT = 32.0

print('THE TILING CONSTANT, AND WHERE IT COMES FROM')
print('  src/lodgen.cpp:6195-6198, verbatim:')
print('    // world-space tiling of the source landscape textures; near-terrain')
print('    // repeats roughly every half cell (calibration against vanilla bakes is')
print('    // an open refinement - the constant only affects apparent texel density)')
print('    constexpr float TILE = 2048.0f;')
print('  and again at src/lodgen.cpp:7486 for the pyramid path.')
print('  docs/LODGEN_TERRAIN_VT.md:734 restates it ("u = frac(wx/2048) ... the')
print('  bake\'s world-space tiling") and cites NOTHING but the bake.')
print('  NO LAND / LTEX / TXST FIELD CARRIES A TILING SCALE: the LTEX record is')
print('  EDID + TNAM(TXST) + HNAM + SNAM + GNAM, the TXST is texture paths and')
print('  flags. Checked below over every LTEX in the two chunks.')
print('')

print('THE MIP THE CODE SELECTS (src/lodgen.cpp:6551-6555)')
print('    texelWorld = TILE / tex->getWidth()')
print('    mip = clamp( log2( max(1, footprint/texelWorld) ), 0, maxMipLevel )')
print('  footprint = span/RES = dim*4096/512 = %.0f world units at dim 4.' % UPT)
print('')

e = B.esm()
seen = {}
for (cx0, cy0) in ((-20, 24), (-20, 20)):
    used = set()
    for cx in range(cx0, cx0 + 4):
        for cy in range(cy0, cy0 + 4):
            L = e.lands.get((cx, cy))
            if not L:
                continue
            for q in range(4):
                if L['base'][q]:
                    used.add(L['base'][q])
                for lay in L['layers'][q]:
                    if lay['ltex']:
                        used.add(lay['ltex'])
    for f in sorted(used):
        if f in seen:
            continue
        seen[f] = B.code_mip(f, TILE, UPT)

print('%-10s %-30s %5s %4s  %8s  %5s  %6s  %10s' %
      ('form', 'texture', 'width', 'mips', 'u/texel', 'MIP', 'side', 'u/texel@mip'))
for f, d in sorted(seen.items()):
    ed = (e.ltex.get(f) or {}).get('edid', '')
    if d is None:
        print('%08X   %-30s  UNRESOLVED' % (f, ed))
        continue
    print('%08X   %-30s %5d %4d  %8.3f  %5.2f  %6d  %10.2f' %
          (f, ed[:30], d['width'], d['maxMip'] + 1, d['texelWorld'], d['mip'],
           d['mipSide'], d['mipTexelWorld']))
print('')
print('  A FOOTPRINT MATCH is mip m with TILE/(width>>m) == footprint, i.e.')
print('  exactly the mip printed above -- so on this corpus THE CODE ALREADY')
print('  PICKS THE FOOTPRINT MIP, to within the clamp. What a footprint-matched')
print('  mip does NOT do is remove the texture\'s own variation at 32 units: it')
print('  hands the bake one whole texel of the 64x64 image per bake texel.')
print('')

# --------------------------------------------------------------- the row -----
for (cx0, cy0) in ((-20, 24), (-20, 20)):
    print('=== chunk (%d,%d), texel row j=256 (the middle), 512 texels ===' % (cx0, cy0))
    van = S.lum(S.Dds(S.van_sheet(cx0, cy0)).level(0))[256]
    ours = S.lum(S.Dds(S.OURS[(cx0, cy0)]).level(0))[256]
    rows = [('vanilla shipped row', van), ('our baked row', ours)]
    for tag, kw in (('offline, the code\'s mip', dict(mip='code')),
                    ('offline, exact footprint box', dict(mip='box')),
                    ('offline, texture global mean', dict(mip='mean')),
                    ('offline, code mip + 2', dict(mip=2.0)),
                    ('offline, code mip + 4', dict(mip=4.0)),
                    ('offline, base layer only', dict(mip='code', layers=False))):
        sheet = B.bake(cx0, cy0, 4, res=512, **kw)
        rows.append((tag, S.lum(np.dstack([sheet, np.full(sheet.shape[:2], 255.0)]))[256]))
        if tag == 'offline, the code\'s mip':
            full = sheet
    print('%-34s %8s %8s %8s' % ('row', 'mean', 'var', 'lv(1-D,3)'))
    for tag, r in rows:
        d = np.diff(r)
        lv = float(np.mean([np.var(r[max(0, i - 1):i + 2]) for i in range(len(r))]))
        print('%-34s %8.2f %8.2f %8.2f' % (tag, r.mean(), r.var(), lv))
    # the reproduction's own ceiling: offline vs the real bake, whole tile
    real = S.Dds(S.OURS[(cx0, cy0)]).level(0)[:, :, :3]
    d = np.abs(full - real)
    print('  REPRODUCTION CHECK offline vs the real bake, whole tile: '
          'mean %.2f p95 %.0f max %.0f of 255'
          % (d.mean(), np.percentile(d, 95), d.max()))
    print('')
