"""Extract the tower LOD diffuse textures from every DX10 BA2 in his MO2 stack that carries them (READ ONLY; into
this lane's tex/ only) and print size + mean colour per copy, winner last (the stack order)."""
import sys, os, numpy as np
sys.path.insert(0, r'E:/Projects/Claude/.claude/skills/fo4-terrain-lod-input-cache')
import ba2_terrain as B
from PIL import Image
RES = [l.strip() for l in open(r'E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/resources.txt') if l.strip()]
want = [w.lower() for w in sys.argv[1:]]
for r in RES:
    try: fs = sorted(x for x in os.listdir(r) if x.lower().endswith('.ba2'))
    except OSError: continue
    for x in fs:
        try: f, ver, recs = B.records(r + '/' + x)
        except AssertionError: continue
        for rc in recs:
            n = rc['name'].lower().replace('/', chr(92))
            if n in want:
                out = 'tex/%s__%s' % (x.replace(' ', '_'), os.path.basename(n))
                open(out, 'wb').write(B.dds(f, rc))
                try:
                    a = np.asarray(Image.open(out).convert('RGBA'), np.float32) / 255
                    mx = a[..., :3].max(-1); mn = a[..., :3].min(-1); S = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0)
                    print('%-45s %-40s %dx%d fmt %d mean %s meanS %.3f' % (x, os.path.basename(n), rc['w'], rc['h'], rc['fmt'], a[..., :3].reshape(-1, 3).mean(0).round(3), S.mean()))
                except Exception as e:
                    print(x, n, rc['w'], rc['h'], rc['fmt'], 'decode fail', e)
