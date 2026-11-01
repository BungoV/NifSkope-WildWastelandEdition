"""lane PERFFIX3 (PERFRES1101 B water OPT 3): ONE refraction copy a second pass, not one per water draw
(src/gl/glscene.cpp Scene::grabRefractionSource; red WW_CELL_WATER_REFR_RED=perdraw). OFFLINE through the real driver:
the tree's fo4_waterlook.prog (the default rows, as tests/watercost/watercost.py sets them, a real baked sea texel) draws
two water bodies into an RGBA32F frame, once with a copy of the frame before EACH draw (the red, as before) and once with
one copy before the first (the default), and the two frames are compared bit for bit.
  disjoint : two bodies far apart on screen: the frames must be IDENTICAL (0 px): with nothing drawn between the water
             draws (drawDeferredShapes draws every water back to back), one copy holds what every copy would.
  adjacent : two bodies sharing an edge: any differing pixel must lie within REACH px of the first body's footprint
             (a refraction read across the edge). MEASURED 11-01: 0 px (the reads across the edge land on the bed).
  overlap  : the second body covers half the first (two water shapes over one place): there it now reads the frame
             behind both, not the first body's colour; some must differ, none farther than REACH px from the overlap.
  between  : the red that shows the condition matters: a card drawn BETWEEN the two water draws (never done by the app)
             makes the frames differ away from the first body (> REACH px): a copy may only be reused while nothing but
             water drew since it.
  source   : glscene.cpp keys the reuse on the second pass (fxDepthPassNow), the framebuffer and viewport, behind the red;
             drawDeferredShapes' water loop draws water nodes only, back to back.
usage: python -I refr_gate.py <tree> <water info dump dir> [W H]
Prints one line per gate and `REFR RESULT PASS|FAIL`."""
import ctypes
import os
import re
import sys

import numpy as np

T, CPP = sys.argv[1], sys.argv[2]
W_, H_ = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (960, 540)
REACH = 48
src = open(os.path.join(T, 'tests', 'watercost', 'watercost.py'), encoding='utf-8').read()
src = src[:src.index("SD = os.path.join(T, 'res', 'shaders')")]
sys.argv = [sys.argv[0], T, CPP, 'sea', str(W_), str(H_), '4']
ns = {'__name__': 'wc', '__file__': os.path.join(T, 'tests', 'watercost', 'watercost.py')}
exec(compile(src, 'watercost.py', 'exec'), ns)
f, gen, U, I, E = ns['f'], ns['gen'], ns['U'], ns['I'], ns['E']
glBlitFramebuffer = f('glBlitFramebuffer', None, I, I, I, I, I, I, I, I, U, E)
glEnable = f('glEnable', None, E)
glBlendFunc = f('glBlendFunc', None, E, E)
g = ns
FW, FH = g['FW'], g['FH']
COL = g['COL']


def rgba32(unit, data):
	t = gen(g['glGenTextures'])
	g['glActiveTexture'](0x84C0 + unit)
	g['glBindTexture'](0x0DE1, t)
	g['glTexImage2D'](0x0DE1, 0, 0x8814, FW, FH, 0, 0x1908, 0x1406, data.ctypes.data if data is not None else None)
	for p, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F)):
		g['glTexParameteri'](0x0DE1, p, v)
	g['glTexParameteri'](0x0DE1, 0x813D, 0)
	return t


def fbo(t):
	fb = gen(g['glGenFramebuffers'])
	g['glBindFramebuffer'](0x8D40, fb)
	g['glFramebufferTexture2D'](0x8D40, 0x8CE0, 0x0DE1, t, 0)
	assert g['glCheckFramebufferStatus'](0x8D40) == 0x8CD5
	return fb


FRAME = rgba32(20, None)
SCENE = rgba32(3, COL)   # unit 3 = waterScene (watercost's FIXED map): the copy the water reads
FB_FRAME, FB_SCENE = fbo(FRAME), fbo(SCENE)
SRC = rgba32(21, COL)
FB_SRC = fbo(SRC)
pr, binds = g['program'](os.path.join(T, 'res', 'shaders'), 'fo4_waterlook.prog')
X0, Y0, H0 = g['X0'], g['Y0'], g['H0']
DEF = {'clear', 'refl', 'glint', 'ocean', 'edge', 'foam', 'contact', 'riverfoam'}

CARD = ns['compile_one'](0x8B30, '#version 410 core\nout vec4 o;\nvoid main() { o = vec4( 3.0, 0.2, 0.1, 1.0 ); }\n')
PCARD = g['glCreateProgram']()
g['glAttachShader'](PCARD, ns['VS'])
g['glAttachShader'](PCARD, CARD)
g['glLinkProgram'](PCARD)


def blit(src_fb, dst_fb):
	g['glBindFramebuffer'](0x8CA8, src_fb)
	g['glBindFramebuffer'](0x8CA9, dst_fb)
	glBlitFramebuffer(0, 0, FW, FH, 0, 0, FW, FH, 0x4000, 0x2600)


def draw_quad(prog, c, half):
	g['glUseProgram'](prog)
	L = lambda n: g['glGetUniformLocation'](prog, n.encode())  # noqa: E731
	if prog == PCARD:
		g['glUniformMatrix4fv'](L('hView'), 1, 1, g['VIEW'].astype(np.float32).ctypes.data)
		g['glUniformMatrix4fv'](L('hProj'), 1, 1, g['PM'].astype(np.float32).ctypes.data)
	g['glUniform3f'](L('hCentre'), float(c[0]), float(c[1]), float(H0))
	g['glUniform1f'](L('hHalf'), float(half))
	g['glBindFramebuffer'](0x8D40, FB_FRAME)
	g['glViewport'](0, 0, FW, FH)
	g['glDrawArrays'](0x0005, 0, 4)


def read():
	g['glBindFramebuffer'](0x8D40, FB_FRAME)
	out = np.empty((FH, FW, 4), np.float32)
	g['glReadPixels'](0, 0, FW, FH, 0x1908, 0x1406, out.ctypes.data)
	return out


def run(per_draw, bodies, card=None):
	"""bodies: [(centre xy, half)]; card: (centre, half) drawn between body 0 and body 1 (the red)"""
	blit(FB_SRC, FB_FRAME)
	g['glDisable'](0x0B71)
	g['glDisable'](0x0BE2)
	for u, tgt, t in binds:
		g['glActiveTexture'](0x84C0 + u)
		g['glBindTexture'](tgt, t)
	g['glActiveTexture'](0x84C0 + 3)
	g['glBindTexture'](0x0DE1, SCENE)
	g['setup'](pr, DEF)
	for i, (c, half) in enumerate(bodies):
		if per_draw or i == 0:
			blit(FB_FRAME, FB_SCENE)
		draw_quad(pr, c, half)
		if card is not None and i == 0:
			draw_quad(PCARD, card[0], card[1])
			g['glUseProgram'](pr)
	e = g['glGetError']()
	if e:
		sys.exit('GL error 0x%x' % e)
	return read()


def footprint(c, half):
	"""the pixels body c covers: the frame with it drawn alone vs the frame before"""
	a = run(True, [(c, half)])
	return np.any(a != COL, axis=-1)


def dist_to(mask):
	"""per pixel: the distance (px, chessboard) to the nearest True of mask, capped"""
	d = np.where(mask, 0, 10 ** 6).astype(np.int64)
	for _ in range(REACH * 3):
		n = d.copy()
		n[1:] = np.minimum(n[1:], d[:-1] + 1)
		n[:-1] = np.minimum(n[:-1], d[1:] + 1)
		n[:, 1:] = np.minimum(n[:, 1:], d[:, :-1] + 1)
		n[:, :-1] = np.minimum(n[:, :-1], d[:, 1:] + 1)
		n[1:, 1:] = np.minimum(n[1:, 1:], d[:-1, :-1] + 1)
		n[:-1, :-1] = np.minimum(n[:-1, :-1], d[1:, 1:] + 1)
		n[1:, :-1] = np.minimum(n[1:, :-1], d[:-1, 1:] + 1)
		n[:-1, 1:] = np.minimum(n[:-1, 1:], d[1:, :-1] + 1)
		if np.array_equal(n, d):
			break
		d = n
	return d


ok = True


def verdict(name, good, detail):
	global ok
	ok &= bool(good)
	print('%-10s %-4s %s' % (name, 'PASS' if good else 'FAIL', detail))


print('frame %d x %d RGBA32F, fo4_waterlook.prog default rows, sea texel at %.0f %.0f h %.0f' % (FW, FH, X0, Y0, H0))
# disjoint: two bodies 1400 u apart (each 250 u half), so no read of one lands on the other
B_DIS = [((X0 - 700.0, Y0), 250.0), ((X0 + 700.0, Y0), 250.0)]
a, b = run(True, B_DIS), run(False, B_DIS)
nd = int(np.count_nonzero(np.any(a != b, axis=-1)))
fa = footprint(*B_DIS[0])
fb_ = footprint(*B_DIS[1])
verdict('disjoint', nd == 0 and fa.sum() > 1000 and fb_.sum() > 1000,
	'%d px differ (bodies %d + %d px on screen)' % (nd, int(fa.sum()), int(fb_.sum())))

# adjacent: one edge shared at x = X0
B_ADJ = [((X0 - 300.0, Y0), 300.0), ((X0 + 300.0, Y0), 300.0)]
a, b = run(True, B_ADJ), run(False, B_ADJ)
diff = np.any(a != b, axis=-1)
fa = footprint(*B_ADJ[0])
d = dist_to(fa)
nd = int(diff.sum())
far = int(np.count_nonzero(diff & (d > REACH)))
dmax = int(d[diff].max()) if nd else 0
mag = float(np.abs(a - b)[diff].max()) if nd else 0.0
verdict('adjacent', far == 0, '%d px differ, all within %d px of the first body (max %d px, %d farther); '
	'largest channel change %.4f' % (nd, REACH, dmax, far, mag))

# overlap: the second body covers half the first (two water shapes over one place): there it now reads the frame behind
# both, not the first body's colour; nowhere else
B_OVL = [((X0 - 150.0, Y0), 300.0), ((X0 + 150.0, Y0), 300.0)]
a, b = run(True, B_OVL), run(False, B_OVL)
diff = np.any(a != b, axis=-1)
both = footprint(*B_OVL[0]) & footprint(*B_OVL[1])
d = dist_to(both)
nd = int(diff.sum())
far = int(np.count_nonzero(diff & (d > REACH)))
verdict('overlap', nd > 0 and far == 0, '%d px differ (overlap %d px), %d farther than %d px from the overlap'
	% (nd, int(both.sum()), far, REACH))

# between (red): a card drawn between the two water draws over the second body
a, b = run(True, B_DIS, card=((X0 + 700.0, Y0), 120.0)), run(False, B_DIS, card=((X0 + 700.0, Y0), 120.0))
diff = np.any(a != b, axis=-1)
fa = footprint(*B_DIS[0])
d = dist_to(fa)
far = int(np.count_nonzero(diff & (d > REACH)))
verdict('between', far > 0, 'red (a draw between the copies, never done by the app): %d px differ farther than %d px '
	'from the first body (must be > 0)' % (far, REACH))

# source: the reuse's key and the water loop
gs = open(os.path.join(T, 'src', 'gl', 'glscene.cpp'), encoding='latin-1').read()
key = re.search(r'if \( !refrPerDraw && refractionTexId && refrCopyPass == fxDepthPassNow && refrCopyFbo == prevDraw', gs)
red = 'wwEnv( "WW_CELL_WATER_REFR_RED" ).trimmed() == "perdraw"' in gs
loop = re.search(r'for \( Node \* node : secondPass\.list\(\) \) \{\s*if \( isWater\( node \) \) \{(.*?)\n\t\t\}\n\t\}', gs, re.S)
body = loop.group(1) if loop else ''
draws = re.findall(r'drawShapes\(\)', body)
bump = re.search(r'\+\+fxDepthPassNow;\s*// lane EFX2', gs)
verdict('source', bool(key) and red and loop is not None and len(draws) == 1 and bool(bump),
	'reuse keyed on the pass + fbo (%s), red perdraw (%s), the water loop draws water only (%d draw call), the pass '
	'counter bumped at each second pass (%s)' % (bool(key), red, len(draws), bool(bump)))
print('REFR RESULT', 'PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
