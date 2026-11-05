"""lane PERFFIX3 (PERFRES1101 B water OPT 2): the displaced water keeps its waves after the split cut, on the cell
view's PRE-SPLIT grid (4 x 4 quads of 1024 u a cell, src/cellview.cpp), with WATERLOOK1's real spectrum (its offline
tools ocean_gl.py + tess_gl.py + presplit_gl.py, exec'd) and the tree's real fo4_default.vert + fo4_waterdisp.tesc/.tese.
  height : 4 std( z ) within 4096 u of 36 eyes 200 u over the sea / the sea set's Hs: 0.80-1.05 (WATERLOOK1's gate)
  cracks : 9 x 9 cells pre-split, no shared edge split or moved apart
  fewer  : triangles a patch near the eyes under the default < 0.5 x those under the red (WW_CELL_WATER_OCEAN_RED=densetess,
           waterOceanRed 8192: every edge split up to 64 times, before PERFFIX3); the red must keep its height too
usage: python -I tess_hs.py <tree> [waterlook1 tools dir]   (exit 0 = all gates as expected)"""
import os
import sys

import numpy as np

WL = sys.argv[2] if len(sys.argv) > 2 else 'E:/Projects/Claude/notes/waterlook1/tools'
sys.argv = sys.argv[:2]
sys.path.insert(0, WL)
_src = open(os.path.join(WL, 'tess_gl.py'), encoding='latin-1').read()
_src = _src[:_src.index('CAM0 = (')]
_src = _src.replace("HERE = os.path.dirname(os.path.abspath(__file__))", "HERE = %r" % WL)
exec(compile(_src, os.path.join(WL, 'tess_gl.py'), 'exec'), globals())
_p = open(os.path.join(WL, 'presplit_gl.py'), encoding='latin-1').read()
_p = _p[_p.index('def grid_split'):_p.index('def run(')]
exec(compile(_p, 'presplit_gl.py', 'exec'), globals())
DISP = (4096.0, 8192.0, 8.0, 64.0)	# src/features/water/cellocean.cpp waterOceanDisp


def near_split(red, places=6):
	zs, ntri_in, ntri_out = [], 0, 0
	for j in range(places):
		for i in range(places):
			cam = (-80000.0 + i * 23000.0 + 311.0, -80000.0 + j * 23000.0 + 977.0, 200.0)
			view, tris, wxy = grid_split(cam, 3, 4)
			upload_grid(view, tris)
			set_uniforms(P_TESS, Pm, U, cam, red=4 | red, disp=DISP)	# 4 nopatch: the set's own Hs (WATERLOOK1's gate)
			o = capture(0, len(tris)).reshape(-1, NV)
			ntri_in += len(tris)
			ntri_out += len(o) // 3
			w = world_of(o, cam)
			base = o[:, 7:9].astype(np.float64)
			bd = np.hypot(np.hypot(base[:, 0] - cam[0], base[:, 1] - cam[1]), cam[2])
			zs.append(w[bd < 4096.0, 2])
	z = np.concatenate(zs)
	return 4.0 * float(np.std(z)) * KM / Pm[0]['hs_set'], ntri_out / max(ntri_in, 1)


def cracks(red):
	cam = (1000.0, 1700.0, 200.0)
	view, tris9, wxy9 = grid_split(cam, 9, 4)
	upload_grid(view, tris9)
	set_uniforms(P_TESS, Pm, U, cam, red=red, disp=DISP)
	return crack_report(tris9, wxy9, [capture(k, 1) for k in range(len(tris9))], cam)


print('sea Hs %.4f m, U %.0f m/s; pre-split grid, 36 eyes 200 u up' % (Pm[0]['hs_set'], U))
hs, tpp = near_split(0)
hs_r, tpp_r = near_split(8192)
sh, bad, worst = cracks(0)
print('default : 4 std(z) / Hs %.3f, %.1f triangles a patch; cracks %d of %d (worst %.3g u)' % (hs, tpp, bad, sh, worst))
print('red     : 4 std(z) / Hs %.3f, %.1f triangles a patch (densetess, before)' % (hs_r, tpp_r))
verdict('height: 4 std(z) near the camera is the sea set\'s Hs (0.80-1.05)', 0.80 < hs < 1.05)
verdict('cracks: none across 9 x 9 pre-split cells', sh > 0 and bad == 0)
verdict('fewer: under half the red\'s triangles near the camera, the red keeps its height', tpp < 0.5 * tpp_r and 0.80 < hs_r < 1.05)
err = glGetError()
verdict('no GL error (0x%x)' % err, err == 0)
print('TESSHS RESULT', 'PASS' if ok_all else 'FAIL')
sys.exit(0 if ok_all else 1)
