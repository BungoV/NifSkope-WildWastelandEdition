"""The far card pick unit, CPU vs GPU (lane FARCARDPICK1, 2026-11-02).

    python -I tests/features/far-lod/farcardpick/farcardpick.py <tree> <oracle exe> <work dir> [--rows <file>] [--red shuffle|convention]

The GPU column runs res/shaders/features/far-lod/impostor_pick.glsl (the vertex stage's wwCardPick, the shipped include) through the
real driver in a hidden 4.1 core context, one transform-feedback vertex a row. The CPU column is pick_oracle (the
shipped impostorpick.h + impostoroct.cpp at the product's flags). Rows: --rows <file> (a WW_FARCARD_PICKDUMP from a
live run: the c3 card set as the cell view drew it; the dump's own CPU answer is checked against the oracle too), else
a synthetic sweep over every grid side 2..16, rings of 8..32 views, both conventions, shuffle on/off, three sharpen
values, the snap and the blend, and the special directions (pole, level, below, the eye on the card).

Verdict (bars written before the first run):
  * every frame index that differs (drawn slots only) sits within 1e-4 of a boundary of the CPU's pick (a floor or
    half step, the diagonal, a snap tie), and fewer than 0.1 % of rows differ;
  * weights within 2e-4 and billboard axes within 2e-5 on the rows whose indices agree;
  * a dump's CPU answer equals the oracle's on every row.
--red shuffle / --red convention flips that input on the GPU only: the verdict must FAIL.
"""
import ctypes, os, random, re, struct, subprocess, sys, math

TREE, ORACLE, WORK = sys.argv[1], sys.argv[2], sys.argv[3]
a = sys.argv[4:]
ROWS = a[a.index('--rows') + 1] if '--rows' in a else None
RED = a[a.index('--red') + 1] if '--red' in a else ''
os.makedirs(WORK, exist_ok=True)

sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as slc   # the hidden window, the loader's #include expansion
gl = slc.gl

def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]

# ---------------------------------------------------------------- rows
def synth():
    rnd = random.Random(1102)
    rows = []
    cfgs = []
    for N in range(2, 17):
        for asb in (0, 1):
            for sh in (0, 1):
                for sharpen, fc in ((1.0, 1), (1.0, 3), (2.0, 3), (1.0 / 0.3, 3)):
                    cfgs.append((N, 0, 0, asb, sh, sharpen, fc))
    for V in (8, 12, 16, 24, 32):
        for sh in (0, 1):
            for sharpen, fc in ((1.0, 1), (1.0, 2), (2.0, 2), (1.0 / 0.3, 2)):
                cfgs.append((2, V, 1, 0, sh, sharpen, fc))
    for cfg in cfgs:
        for k in range(160):
            cx, cy, cz = rnd.uniform(-90000, 90000), rnd.uniform(-90000, 90000), rnd.uniform(-2000, 6000)
            dist = rnd.uniform(1500, 120000)
            az = rnd.uniform(-math.pi, math.pi)
            el = rnd.choice((rnd.uniform(-0.3, 1.5707), rnd.uniform(0.0, 0.25), rnd.uniform(-0.02, 0.02)))
            ex = cx + dist * math.cos(el) * math.cos(az)
            ey = cy + dist * math.cos(el) * math.sin(az)
            ez = cz + dist * math.sin(el)
            rows.append((ex, ey, ez, cx, cy, cz) + cfg)
        # the special directions: straight above, straight below, level, the eye on the card
        cx, cy, cz = 1234.5, -6789.25, 512.0
        for e in ((cx, cy, cz + 9000.0), (cx, cy, cz - 9000.0), (cx + 9000.0, cy, cz), (cx, cy - 9000.0, cz),
                  (cx, cy, cz), (cx + 1e-4, cy, cz)):
            rows.append(e + (cx, cy, cz) + cfg)
    return [tuple(f32(v) for v in r[:6]) + r[6:11] + (f32(r[11]), r[12]) for r in rows]

def load(path):
    rows, cpu = [], []
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        left, _, right = line.partition('|')
        v = left.split()
        rows.append(tuple(float(x) for x in v[:6]) + tuple(int(x) for x in v[6:11]) + (float(v[11]), int(v[12])))
        cpu.append(right.split())
    return rows, cpu

if ROWS:
    rows, dumped = load(ROWS)
else:
    rows, dumped = synth(), None
rpath = os.path.join(WORK, 'rows.txt')
with open(rpath, 'w') as f:
    for r in rows:
        f.write('%.9g %.9g %.9g %.9g %.9g %.9g %d %d %d %d %d %.9g %d\n' % r)

# ---------------------------------------------------------------- CPU (the oracle)
opath = os.path.join(WORK, 'cpu.txt')
res = subprocess.run([ORACLE, rpath, opath], capture_output=True, text=True)
print(res.stdout.strip())
if res.returncode != 0:
    print('ORACLE FAIL rc', res.returncode, res.stderr[:500]); sys.exit(2)
cpu = []
for line in open(opath):
    left, _, b = line.partition('|')
    v = left.split()
    cpu.append(([int(x) for x in v[:3]], [float(x) for x in v[3:15]], float(b)))

# ---------------------------------------------------------------- GPU (transform feedback)
VS = r'''#version 410 core
layout( location = 0 ) in vec3 aEye;
layout( location = 1 ) in vec3 aCentre;
layout( location = 2 ) in vec4 aP;    // N V ring asBaked
layout( location = 3 ) in vec4 aQ;    // shuffle sharpen frameCount 0
%s
flat out ivec3 oIdx;
out vec3 oW;
out vec3 oR;
out vec3 oU;
out vec3 oF;
void main()
{
	bool asBaked = aP.w > 0.5;
	bool shuffle = aQ.x > 0.5;
#ifdef RED_CONVENTION
	asBaked = !asBaked;
#endif
#ifdef RED_SHUFFLE
	shuffle = !shuffle;
#endif
	wwCardPick( aEye, aCentre, int( aP.x ), int( aP.y ), aP.z > 0.5, asBaked, shuffle, aQ.y, int( aQ.z ),
				oIdx, oW, oR, oU, oF );
	gl_Position = vec4( 0.0 );
}
'''
pick = slc.expand(os.path.join(TREE, 'res', 'shaders', 'impostor_pick.glsl'), 1)
src = VS % pick
if RED:
    src = src.replace('#version 410 core\n', '#version 410 core\n#define RED_%s 1\n' % RED.upper(), 1)

slc.context()
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
fn = slc.fn
createShader = fn('glCreateShader', GLuint, GLenum)
shaderSource = fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)
compileShader = fn('glCompileShader', None, GLuint)
getShaderiv = fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
getShaderInfoLog = fn('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
createProgram = fn('glCreateProgram', GLuint)
attach = fn('glAttachShader', None, GLuint, GLuint)
link = fn('glLinkProgram', None, GLuint)
getProgramiv = fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
getProgramInfoLog = fn('glGetProgramInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
tfVaryings = fn('glTransformFeedbackVaryings', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), GLenum)
useProgram = fn('glUseProgram', None, GLuint)
genBuffers = fn('glGenBuffers', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindBuffer = fn('glBindBuffer', None, GLenum, GLuint)
bufferData = fn('glBufferData', None, GLenum, ctypes.c_ssize_t, ctypes.c_void_p, GLenum)
bindBufferBase = fn('glBindBufferBase', None, GLenum, GLuint, GLuint)
genVAO = fn('glGenVertexArrays', None, ctypes.c_int, ctypes.POINTER(GLuint))
bindVAO = fn('glBindVertexArray', None, GLuint)
enableAttr = fn('glEnableVertexAttribArray', None, GLuint)
attrPtr = fn('glVertexAttribPointer', None, GLuint, GLint, GLenum, ctypes.c_ubyte, ctypes.c_int, ctypes.c_void_p)
beginTF = fn('glBeginTransformFeedback', None, GLenum)
endTF = fn('glEndTransformFeedback', None)
getBufferSubData = fn('glGetBufferSubData', None, GLenum, ctypes.c_ssize_t, ctypes.c_ssize_t, ctypes.c_void_p)
gl.glEnable.argtypes = [ctypes.c_uint]; gl.glDisable.argtypes = [ctypes.c_uint]
gl.glDrawArrays.argtypes = [ctypes.c_uint, ctypes.c_int, ctypes.c_int]
gl.glGetError.restype = ctypes.c_uint
print('driver:', gl.glGetString(0x1F01).decode(), '|', gl.glGetString(0x1F02).decode())

s = createShader(0x8B31)
b = ctypes.c_char_p(src.encode('latin-1'))
shaderSource(s, 1, ctypes.byref(b), None)
compileShader(s)
ok = GLint(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
if not ok.value:
    log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
    print('COMPILE FAIL'); print(log.value.decode(errors='replace')[:2000]); sys.exit(2)
p = createProgram(); attach(p, s)
names = (ctypes.c_char_p * 5)(b'oIdx', b'oW', b'oR', b'oU', b'oF')
tfVaryings(p, 5, names, 0x8C8C)   # GL_INTERLEAVED_ATTRIBS
link(p)
getProgramiv(p, 0x8B82, ctypes.byref(ok))
if not ok.value:
    log = ctypes.create_string_buffer(1 << 16); getProgramInfoLog(p, 1 << 16, None, log)
    print('LINK FAIL'); print(log.value.decode(errors='replace')[:2000]); sys.exit(2)
useProgram(p)

n = len(rows)
STRIDE = 14   # floats a row in: eye 3, centre 3, P 4, Q 4
inp = (ctypes.c_float * (n * STRIDE))()
for i, r in enumerate(rows):
    inp[i * STRIDE:(i + 1) * STRIDE] = [r[0], r[1], r[2], r[3], r[4], r[5], float(r[6]), float(r[7]), float(r[8]),
                                        float(r[9]), float(r[10]), r[11], float(r[12]), 0.0]
vao = GLuint(); genVAO(1, ctypes.byref(vao)); bindVAO(vao)
vb = GLuint(); genBuffers(1, ctypes.byref(vb)); bindBuffer(0x8892, vb)
bufferData(0x8892, ctypes.sizeof(inp), inp, 0x88E4)
for loc, cnt, off in ((0, 3, 0), (1, 3, 3), (2, 4, 6), (3, 4, 10)):
    enableAttr(loc); attrPtr(loc, cnt, 0x1406, 0, STRIDE * 4, ctypes.c_void_p(off * 4))
OUTN = 15
tb = GLuint(); genBuffers(1, ctypes.byref(tb)); bindBuffer(0x8C8E, tb)
bufferData(0x8C8E, n * OUTN * 4, None, 0x88E5)
bindBufferBase(0x8C8E, 0, tb)
gl.glEnable(0x8C89)   # GL_RASTERIZER_DISCARD
beginTF(0x0000)       # GL_POINTS
gl.glDrawArrays(0x0000, 0, n)
endTF()
gl.glDisable(0x8C89)
outb = (ctypes.c_ubyte * (n * OUTN * 4))()
getBufferSubData(0x8C8E, 0, n * OUTN * 4, outb)
err = gl.glGetError()
if err:
    print('GL ERROR', hex(err)); sys.exit(2)
raw = bytes(outb)
gpu = []
for i in range(n):
    rec = raw[i * OUTN * 4:(i + 1) * OUTN * 4]
    gpu.append((list(struct.unpack('<3i', rec[:12])), list(struct.unpack('<12f', rec[12:]))))

# ---------------------------------------------------------------- compare
diff_rows, unexplained, worst_b = 0, [], 0.0
wmax = amax = 0.0
dump_bad = 0
by = {}
for i, r in enumerate(rows):
    ci, cf, bnd = cpu[i]
    gi, gf = gpu[i]
    fc = r[12]
    drawn = range(min(fc, 3) if not r[8] else min(fc, 2))
    differs = any(ci[k] != gi[k] for k in drawn)
    key = ('ring V%d' % r[7]) if r[8] else ('grid N%d' % r[6])
    t = by.setdefault(key, [0, 0])
    t[0] += 1
    if differs:
        diff_rows += 1
        t[1] += 1
        worst_b = max(worst_b, bnd)
        if bnd >= 1e-4:
            unexplained.append((i, r, ci, gi, bnd))
    else:
        for k in drawn:
            wmax = max(wmax, abs(cf[k] - gf[k]))
        for k in range(3, 12):
            amax = max(amax, abs(cf[k] - gf[k]))
    if dumped is not None:
        dv = dumped[i]
        di = [int(x) for x in dv[:3]]
        dfl = [float(x) for x in dv[3:15]]
        if di != ci or any(abs(x - y) > 0.0 for x, y in zip(dfl, cf)):
            dump_bad += 1

print('rows %d (%s)%s' % (n, ROWS if ROWS else 'synthetic sweep', ', RED ' + RED if RED else ''))
for key in sorted(by, key=lambda k: (k[:4], int(re.sub(r'\D', '', k)))):
    if by[key][1]:
        print('  %-10s %6d rows, %d differ' % (key, by[key][0], by[key][1]))
print('index differs on %d rows (%.4f %%), worst boundary distance among them %.3g (bar 1e-4); %d unexplained'
      % (diff_rows, 100.0 * diff_rows / max(1, n), worst_b, len(unexplained)))
for u in unexplained[:8]:
    print('  UNEXPLAINED row', u[0], 'in', u[1], 'cpu', u[2], 'gpu', u[3], 'boundary %.3g' % u[4])
print('weights max |cpu - gpu| %.3g (bar 2e-4), billboard axes max %.3g (bar 2e-5) on the agreeing rows' % (wmax, amax))
if dumped is not None:
    print('dump vs oracle: %d of %d rows differ (bar 0)' % (dump_bad, n))
ok = (not unexplained and diff_rows < 0.001 * n and wmax <= 2e-4 and amax <= 2e-5
      and (dumped is None or dump_bad == 0))
print('VERDICT', 'PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
