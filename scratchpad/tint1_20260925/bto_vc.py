"""TINT1 side question, part 3: does the SHIPPED stock object LOD (vanilla .bto, the CK's bake) carry per-vertex
hue on buildings? Reads Boston dim-4 .bto chunks from his MO2 stack (same resolver as census.py). Read only."""
exec(open('census.py').read().split('# ---- per mesh')[0])
import numpy as np, collections
tot = collections.Counter()
for x in range(-8, 4, 4):
    for y in range(-12, 0, 4):
        nm = 'terrain' + BS + 'commonwealth' + BS + 'objects' + BS + 'commonwealth.4.%d.%d.bto' % (x, y)
        data, src = getnif(nm)
        if data is None: tot['missing'] += 1; continue
        N = nifwind.Nif(data)
        for k, (t, o, sz) in enumerate(N.blocks):
            if t not in nifwind.SHAPES: continue
            sh = N.shape(k); tot['shapes'] += 1
            f1, f2, mat = N.shader_flags(sh['shader']) if 0 <= sh['shader'] < len(N.blocks) else (0, 0, '')
            if sh['cols'] is None: continue
            C = np.asarray(sh['cols'])[:, :3].astype(int); tot['vc'] += 1
            tot['vcflag'] += bool((f2 or 0) & 0x20)
            if np.any((C[:, 0] != C[:, 1]) | (C[:, 1] != C[:, 2])):
                tot['hue'] += 1
                if tot['hue'] <= 6: print('  hue', src, nm[-30:], sh['name'], (mat or '')[-45:], 'mean rgb', C.mean(0).round(1))
            elif np.any(C != 255): tot['shade'] += 1
print(dict(tot))
