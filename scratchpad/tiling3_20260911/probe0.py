import os, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
sys.path.insert(0, SP); sys.path.insert(0, T2)
import splatlib as S
import t1_lib as T
for cx, cy in ((-20,24), (-20,20)):
    for suf in ('', '_msn', '_data'):
        p = S.van_sheet(cx, cy, suf)
        if not os.path.exists(p):
            print('MISSING', p); continue
        d = S.Dds(p)
        l0 = d.level(0)
        print('(%d,%d)%-6s shape=%s dtype=%s maxmip=%s' % (cx, cy, suf or '(col)', l0.shape, l0.dtype, d.mipCount if hasattr(d,'mipCount') else '?'))
