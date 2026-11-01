"""lane PERFFIX3 (PERFRES1101 B water OPT 2): the GPU cost of the High/Ultra water's tessellated displacement
(fo4_waterlookd.prog = fo4_waterdisp.tesc / .tese + every water look), OFFLINE through the real driver, in the frame the
cell view draws: 1920 x 1026, 4x MSAA RGBA32F + depth (the live linear frame), the cell view's water geometry (one water
quad a cell pre-split into 4 x 4 quads of 1024 u, 5 x 5 cells round the camera), depth test LEQUAL. tests/watercost
times one full-screen quad single-sampled and never the tessellated program; this one does both (the flat twin
fo4_waterlook.prog over the SAME triangles is the "displacement off" arm = WW_CELL_WATER_OCEAN_RED=flat).
Inputs as tests/watercost/watercost.py (its setup is exec'd: the real baked sea texel, the default rows, seeded noise and
ocean arrays; the arrays' values do not change the work done).
  arms : flat | disp at waterOceanDisp (full, none, edge px, max level) = the shipped (4096, 8192, 8, 64) and candidates
  cams : down (450 u over the water, 35 deg down: the frame all water) and level (200 u over it, 12 deg down: horizon in
         view, the near water split hardest)
usage: python -I watertess.py <tree> <water info dump dir> [msaa=4] [N=10] [arms=...]
Prints `TESS <cam> <arm> <ms> tris=<primitives after the split>` per arm (min over 5 interleaved rounds of the median)."""
import ctypes
import math
import os
import sys

import numpy as np

T, CPP = sys.argv[1], sys.argv[2]
MS = int(sys.argv[3]) if len(sys.argv) > 3 else 4
NREP = int(sys.argv[4]) if len(sys.argv) > 4 else 10
ARMS_SEL = sys.argv[5].split(',') if len(sys.argv) > 5 else None
if ARMS_SEL == ['gate']:	# the gate's three arms (tests/watertess/run.sh)
	ARMS_SEL = ['flat', 'disp default', 'densetess']
FW, FH = 1920, 1026
src = open(os.path.join(T, 'tests', 'watercost', 'watercost.py'), encoding='utf-8').read()
src = src[:src.index("SD = os.path.join(T, 'res', 'shaders')")]
sys.argv = [sys.argv[0], T, CPP, 'sea', str(FW), str(FH), '4']
g = {'__name__': 'wc', '__file__': os.path.join(T, 'tests', 'watercost', 'watercost.py')}
exec(compile(src, 'watercost.py', 'exec'), g)
f, gen, U, I, E, P = g['f'], g['gen'], g['U'], g['I'], g['E'], g['P']
slc = g['slc']
for name, sig in {'glPatchParameteri': (None, E, I), 'glEnable': (None, E), 'glDepthFunc': (None, E),
				  'glGenRenderbuffers': (None, I, P), 'glBindRenderbuffer': (None, E, U),
				  'glRenderbufferStorageMultisample': (None, E, I, E, I, I), 'glRenderbufferStorage': (None, E, E, I, I),
				  'glFramebufferRenderbuffer': (None, E, E, E, U), 'glClearDepth': (None, ctypes.c_double),
				  'glDepthMask': (None, ctypes.c_ubyte), 'glUniform4f': (None, I, ctypes.c_float, ctypes.c_float,
				  ctypes.c_float, ctypes.c_float)}.items():
	g[name] = f(name, *sig)
G = type('G', (), {})()
for k, v in g.items():
	if k.startswith('gl'):
		setattr(G, k, v)

SD = os.path.join(T, 'res', 'shaders')
VS_SRC = r'''#version 410 core
layout( location = 0 ) in vec3 wpos;
uniform mat4 hView;
uniform mat4 hProj;
uniform vec3 hLightView;
out vec3 LightDir;
out vec3 ViewDir;
out vec2 texCoord;
out mat3 btnMatrix;
flat out vec4 A;
out vec4 C;
flat out vec4 D;
out float rawVertexAlpha;
flat out mat3 reflMatrix;
void main()
{
	vec4 v = hView * vec4( wpos, 1.0 );
	ViewDir = -v.xyz;
	LightDir = hLightView;
	texCoord = wpos.xy / 4096.0;
	btnMatrix = mat3( 1.0 );
	A = vec4( 0.2, 0.2, 0.2, 1.0 );
	C = vec4( 1.0 );
	D = vec4( 1.0, 0.95, 0.85, 1.0 );
	rawVertexAlpha = 1.0;
	reflMatrix = mat3( 1.0 );
	gl_Position = hProj * v;
}
'''
VS = g['compile_one'](0x8B31, VS_SRC)


def frag_of(prog):
	for line in open(os.path.join(SD, prog)):
		if line.startswith('shaders'):
			return line.split()[-1]


def link(prog, tess, frag=None):
	fs = frag or slc.expand(os.path.join(SD, frag_of(prog))).replace('NUM_TEXTURE_UNITS', '30')
	pr = G.glCreateProgram()
	G.glAttachShader(pr, VS)
	if tess:
		G.glAttachShader(pr, g['compile_one'](0x8E88, slc.expand(os.path.join(SD, 'fo4_waterdisp.tesc'))))
		G.glAttachShader(pr, g['compile_one'](0x8E87, slc.expand(os.path.join(SD, 'fo4_waterdisp.tese'))))
	G.glAttachShader(pr, g['compile_one'](0x8B30, fs))
	G.glLinkProgram(pr)
	ok = I(0)
	G.glGetProgramiv(pr, 0x8B82, ctypes.byref(ok))
	if not ok.value:
		log = ctypes.create_string_buffer(1 << 15)
		G.glGetProgramInfoLog(pr, 1 << 15, None, log)
		sys.exit('LINK FAIL %s %s' % (prog, log.value.decode(errors='replace')[:800]))
	# the samplers on their own units, as watercost.program() does
	G.glUseProgram(pr)
	n = I(0)
	G.glGetProgramiv(pr, 0x8B86, ctypes.byref(n))
	binds, spare = [], 40
	for i in range(n.value):
		nm = ctypes.create_string_buffer(256)
		sz, ty = I(0), U(0)
		G.glGetActiveUniform(pr, i, 256, None, ctypes.byref(sz), ctypes.byref(ty), nm)
		name = nm.value.decode().split('[')[0]
		tv = ty.value
		if tv in g['SAMPLER_2D'] or tv in g['SAMPLER_2DARRAY'] or tv in (0x8B5F, 0x8B60, 0x8DC4, 0x8B62, 0x8DD3, 0x8DD4,
				0x8DCF, 0x9108, 0x8DD7, 0x8DD8, 0x8DC5, 0x8B5D, 0x900C, 0x900D, 0x900F, 0x8DCC, 0x8DCB, 0x8DCE):
			u = g['FIXED'].get(name)
			if u is None:
				u = spare
				spare += 1
				if tv == 0x8B5E:
					binds.append((u, 0x0DE1, g['GENERIC']))
				elif tv == 0x8DD2:
					binds.append((u, 0x0DE1, g['UGENERIC']))
				elif tv in g['SAMPLER_2DARRAY']:
					binds.append((u, 0x8C1A, g['ARR']))
			G.glUniform1i(G.glGetUniformLocation(pr, name.encode()), u)
	bi = G.glGetUniformBlockIndex(pr, b'globalUniforms')
	if bi != 0xFFFFFFFF:
		G.glUniformBlockBinding(pr, bi, 0)
	return pr, binds


P_FLAT = link('fo4_waterlook.prog', False)
P_DISP = link('fo4_waterlookd.prog', True)
# exploration: the split + move alone (a one-colour fragment): the geometry's share of the cost
NULLFS = '#version 410 core\nin vec2 wwOcBase;\nout vec4 o;\nvoid main() { o = vec4( wwOcBase * 1e-6, 0.5, 1.0 ); }\n'
P_GEO = link('fo4_waterlookd.prog', True, NULLFS)
print('programs: fo4_waterlook (flat) and fo4_waterlookd (vert + fo4_waterdisp.tesc/.tese + frag) linked')

# the cell view's water: 5 x 5 cells round the spot, each 4 x 4 quads of 1024 u (src/cellview.cpp pre-split), ccw
X0, Y0, H0 = g['X0'], g['Y0'], g['H0']
cx0, cy0 = math.floor(X0 / 4096.0) - 2, math.floor(Y0 / 4096.0) - 2
tri = []
for j in range(20):
	for i in range(20):
		x0, y0 = cx0 * 4096.0 + i * 1024.0, cy0 * 4096.0 + j * 1024.0
		a, b, c, d = (x0, y0), (x0 + 1024.0, y0), (x0 + 1024.0, y0 + 1024.0), (x0, y0 + 1024.0)
		tri += [a, b, c, a, c, d]
VTX = np.array([(x, y, H0) for x, y in tri], np.float32)
NV = len(VTX)
VAO = gen(G.glGenVertexArrays)
G.glBindVertexArray(VAO)
VBO = gen(G.glGenBuffers)
G.glBindBuffer(0x8892, VBO)
G.glBufferData(0x8892, VTX.nbytes, VTX.ctypes.data, 0x88E4)
G.glEnableVertexAttribArray(0)
G.glVertexAttribPointer(0, 3, 0x1406, 0, 0, None)

# the live frame: MSAA RGBA32F colour + 32F depth
rb_c, rb_d = gen(G.glGenRenderbuffers), gen(G.glGenRenderbuffers)
G.glBindRenderbuffer(0x8D41, rb_c)
G.glRenderbufferStorageMultisample(0x8D41, MS, 0x8814, FW, FH)
G.glBindRenderbuffer(0x8D41, rb_d)
G.glRenderbufferStorageMultisample(0x8D41, MS, 0x8CAC, FW, FH)
FB = gen(G.glGenFramebuffers)
G.glBindFramebuffer(0x8D40, FB)
G.glFramebufferRenderbuffer(0x8D40, 0x8CE0, 0x8D41, rb_c)
G.glFramebufferRenderbuffer(0x8D40, 0x8D00, 0x8D41, rb_d)
assert G.glCheckFramebufferStatus(0x8D40) == 0x8CD5, 'msaa fbo'

PM = np.zeros((4, 4))
NEAR, FAR, FOVY = 5.0, 200000.0, math.radians(55.0)
fy = 1.0 / math.tan(FOVY / 2)
PM[0, 0], PM[1, 1] = fy * FH / FW, fy
PM[2, 2], PM[2, 3], PM[3, 2] = -(FAR + NEAR) / (FAR - NEAR), -2 * FAR * NEAR / (FAR - NEAR), -1.0
g['PM'] = PM


def camera(kind):
	if kind == 'down':
		eye, at = np.array([X0, Y0 - 640.0, H0 + 450.0]), np.array([X0, Y0, H0])
	else:
		pitch = math.radians(12.0)
		eye = np.array([X0, Y0 - 2000.0, H0 + 200.0])
		at = eye + np.array([0.0, math.cos(pitch), -math.sin(pitch)]) * 1000.0
	V, R = g['look'](eye, at)
	g['VIEW'], g['ROT'], g['EYE'] = V, R, eye


QT, QP = gen(G.glGenQueries), gen(G.glGenQueries)


def time_arm(pr, binds, tess, disp, n=NREP):
	G.glBindFramebuffer(0x8D40, FB)
	G.glViewport(0, 0, FW, FH)
	G.glDisable(0x0BE2)
	G.glEnable(0x0B71)
	G.glDepthFunc(0x0203)
	G.glDepthMask(1)
	for u, tgt, t in binds:
		G.glActiveTexture(0x84C0 + u)
		G.glBindTexture(tgt, t)
	g['setup'](pr, g['DEF'] if 'DEF' in g else {'clear', 'refl', 'glint', 'ocean', 'edge', 'foam', 'contact', 'riverfoam'})
	G.glUniform4f(G.glGetUniformLocation(pr, b'waterOceanDisp'), *[float(x) for x in disp[:4]])
	G.glUniform1i(G.glGetUniformLocation(pr, b'waterOceanRed'), int(disp[4]) if len(disp) > 4 else 0)
	G.glPatchParameteri(0x8E72, 3)
	times, prims = [], 0
	for k in range(n):
		G.glClearDepth(1.0)
		G.glClear(0x4100)
		G.glBeginQuery(0x88BF, QT)
		if k == 0:
			G.glBeginQuery(0x8C87, QP)
		G.glDrawArrays(0x000E if tess else 0x0004, 0, NV)
		if k == 0:
			G.glEndQuery(0x8C87)
		G.glEndQuery(0x88BF)
		v = ctypes.c_uint64(0)
		G.glGetQueryObjectui64v(QT, 0x8866, ctypes.byref(v))
		times.append(v.value / 1e6)
		if k == 0:
			G.glGetQueryObjectui64v(QP, 0x8866, ctypes.byref(v))
			prims = v.value
	e = G.glGetError()
	if e:
		sys.exit('GL error 0x%x' % e)
	return float(np.median(times)), prims


g['DEF'] = {'clear', 'refl', 'glint', 'ocean', 'edge', 'foam', 'contact', 'riverfoam'}
# PERFFIX3: 'disp default' = the tree's .tesc (no step finer than the heights' 64 u filter); the red densetess (8192) =
# every edge up to waterOceanDisp.w splits (before PERFFIX3); the other disp arms move the uniform (exploration)
ARMS = [('flat', P_FLAT, False, (4096, 8192, 8, 64)), ('disp default 4096/8192 8px', P_DISP, True, (4096, 8192, 8, 64)),
		('disp red densetess (before)', P_DISP, True, (4096, 8192, 8, 64, 8192)),
		('geo only default', P_GEO, True, (4096, 8192, 8, 64)), ('geo only densetess', P_GEO, True, (4096, 8192, 8, 64, 8192)),
		('geo only level 1', P_GEO, True, (4096, 8192, 1e9, 1)),
		('disp 12px', P_DISP, True, (4096, 8192, 12, 64)), ('disp 16px', P_DISP, True, (4096, 8192, 16, 64)),
		('disp 24px', P_DISP, True, (4096, 8192, 24, 64)), ('disp 32px', P_DISP, True, (4096, 8192, 32, 64)),
		('disp reach 2048/4096 8px', P_DISP, True, (2048, 4096, 8, 64)),
		('disp reach 2048/4096 16px', P_DISP, True, (2048, 4096, 16, 64)),
		('disp max 16 8px', P_DISP, True, (4096, 8192, 8, 16)),
		('disp max 32 8px', P_DISP, True, (4096, 8192, 8, 32)), ('disp max 8 8px', P_DISP, True, (4096, 8192, 8, 8)),
		('disp max 16 16px', P_DISP, True, (4096, 8192, 16, 16)),
		('disp max 16 reach 2048/4096', P_DISP, True, (2048, 4096, 8, 16)),
		('disp max 4 8px', P_DISP, True, (4096, 8192, 8, 4)),
		('disp level 1 (no split)', P_DISP, True, (4096, 8192, 1e9, 1))]
if ARMS_SEL:
	ARMS = [a for a in ARMS if any(s in a[0] for s in ARMS_SEL)]
print('frame %d x %d, %dx MSAA RGBA32F + depth, %d water triangles (400 quads of 1024 u), median of %d draws, min of 5 '
	  'interleaved rounds; GPU ms' % (FW, FH, MS, NV // 3, NREP))
VERD = []
for cam in ('down', 'level'):
	camera(cam)
	for a in ARMS:	# warm-up
		time_arm(a[1][0], a[1][1], a[2], a[3], 3)
	res = {a[0]: [] for a in ARMS}
	pr_n = {}
	for _ in range(5):
		for a in ARMS:
			ms, pn = time_arm(a[1][0], a[1][1], a[2], a[3])
			res[a[0]].append(ms)
			pr_n[a[0]] = pn
	for a in ARMS:
		print('TESS %-5s %-28s %.3f  tris=%-8d (rounds %s)' % (cam, a[0], min(res[a[0]]), pr_n[a[0]],
			  ' '.join('%.3f' % x for x in res[a[0]])))
	if 'flat' in res and 'disp default 4096/8192 8px' in res and 'disp red densetess (before)' in res:
		fl = min(res['flat'])
		dd, dr = min(res['disp default 4096/8192 8px']) - fl, min(res['disp red densetess (before)']) - fl
		VERD.append((cam, dd, dr))
# pre-registered bar (PERFFIX3, 11-01): the split's own cost (disp - flat) <= BAR ms at both cameras; the red (before) must
# be over it, else the bar measures nothing
BAR = 0.80
if VERD:
	good = all(dd <= BAR < dr for _, dd, dr in VERD)
	for cam, dd, dr in VERD:
		print('TESS SPLIT %-5s default +%.3f ms, red densetess +%.3f ms over flat (bar %.2f)' % (cam, dd, dr, BAR))
	print('TESS VERDICT', 'PASS' if good else 'FAIL')
	sys.exit(0 if good else 1)
