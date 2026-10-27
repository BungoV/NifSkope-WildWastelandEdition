"""lane CLOUDLOOK1: the sky's air must not veil the volume clouds.
The Volumetric Air's sky pass (cell_volsky.frag, wwVolAirSky) lays the air seen up to the medium's top over the
backdrop, where the volume clouds already sit: sky x T + L. Its top was the scene sphere's top (a union of every
shape's sphere, src/gl/gltools.cpp BoundSphere::operator|=), about 1 km over the ground on a cell set, so the
Overcast air (optical depth ~2.7 straight up) left T < 1 % at 30 degrees: a uniform veil, no cloud shapes.
The fix (src/gl/cellvolair.cpp vaShapesTop) takes the highest top of a visible shape's own sphere.

python -I cloudveil_check.py <tree> [--red spheretop] [--rev <git rev>]
  --red spheretop : the air's top at the union sphere's (the old law): must FAIL
  --rev <rev>     : read cellvolair.cpp of that commit for the wiring check (origin/main: must FAIL)
Gates:
  U  the union law reproduces the live echo's order (top ~ 70 km units on a 25 x 25 cell set) and lies far above
     every shape (the old top is not the scene's highest point)
  K  the shape law's top = the highest shape top, never above the union's
  T  sky transmittance up the air with the c3 Overcast medium (echo of 10-27: sigma0 3.72732e-05, zb -9936,
     H 789958.6, ground 1.623 thick 490.2 zg 64) at 20/30/45/60 degrees: T(30) >= 0.25 and T(20) >= 0.12;
     the cloud contrast left (T x the cloud layer's own contrast) above 2/255 at every angle
  W  wiring: topW reads vaShapesTop unless the red, the red word is parsed, the helper takes the max of shape tops
The medium frame is taken as world units (fogView offset 0) and the camera at 160 units over the fixture's ground:
an assumption, stated, not measured. The fixture's buildings are 2500 units tall (a Concord church spire is ~2000)."""
import math, os, re, subprocess, sys

tree = sys.argv[1]
red = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else ''
rev = sys.argv[sys.argv.index('--rev') + 1] if '--rev' in sys.argv else None


# ---- BoundSphere::operator|= (src/gl/gltools.cpp), in float64 (the float32 rounding does not matter at these sizes)
def union(a, b):
    c1, r1 = a
    c2, r2 = b
    if not (r1 >= r2):
        (c1, r1), (c2, r2) = (c2, r2), (c1, r1)
    if r2 >= 0.0:
        d = math.dist(c1, c2)
        if d > 0.0 and r1 < d + r2:
            nr = (r1 + r2 + d) * 0.5
            c1 = tuple(c1[i] + (c2[i] - c1[i]) * ((nr - r1) / d) for i in range(3))
            r1 = nr
    return c1, r1


# ---- the fixture: a 25 x 25 LAND grid (4096 units a cell, ground 0..600 units), a building per 3rd cell
CELL = 4096.0
shapes = []
for gx in range(-12, 13):
    for gy in range(-12, 13):
        x0, y0 = gx * CELL, gy * CELL
        h = 300.0 + 300.0 * math.sin(gx * 0.7) * math.cos(gy * 0.5)
        # a LAND cell's own sphere: its 4096 x 4096 x relief box's
        shapes.append(((x0 + CELL / 2, y0 + CELL / 2, h), math.sqrt(2 * (CELL / 2) ** 2 + 150.0 ** 2)))
        if (gx + gy) % 3 == 0:
            shapes.append(((x0 + 1000.0, y0 + 1500.0, h + 1250.0), math.hypot(900.0, 1250.0)))   # 2500 units tall
uni = ((0.0, 0.0, 0.0), -1.0)
for s in shapes:
    uni = union(uni, s)
union_top = uni[0][2] + uni[1]
shape_top = min(max(c[2] + r for c, r in shapes), union_top)
ground = 300.0
cam_z = ground + 160.0
top = union_top if red == 'spheretop' else shape_top

fails = []


def gate(name, ok, text):
    print('  %-3s %s %s' % (name, 'ok  ' if ok else 'FAIL', text))
    if not ok:
        fails.append(name)


print('fixture: %d shapes; union top %.1f, shape top %.1f, camera %.1f%s' % (
    len(shapes), union_top, shape_top, cam_z, ' (red: the union top)' if red else ''))
gate('U', 40000.0 < union_top < 120000.0 and union_top > 5.0 * shape_top,
     'union top %.0f (live echo 70935) vs the highest shape %.0f' % (union_top, shape_top))
gate('K', shape_top <= union_top and abs(shape_top - max(c[2] + r for c, r in shapes)) < 1e-3,
     'shape law top %.1f = max of shape tops' % shape_top)

# ---- the column law (res/shaders/lookdev_fog.glsl wwAirColOne / wwAirColLaw), per unit sigma0
SIG0, ZB, H = 3.72732e-05, -9936.0, 789958.6
GS, GTHICK, GZ = 1.623, 490.2, 64.0


def col_one(lo, hi, z0, invH):
    if hi <= lo:
        return 0.0
    if invH <= 0.0:
        return hi - lo
    below = max(min(hi, z0) - lo, 0.0)
    a = max(lo, z0)
    if hi <= a:
        return below
    w = hi - a
    u = w * invH
    e1 = 1.0 - u * (0.5 - u / 6.0) if u < 1e-3 else (1.0 - math.exp(-u)) / u
    return below + math.exp(-(a - z0) * invH) * w * e1


def col(lo, hi, mtop):
    hi = min(hi, mtop)
    return col_one(lo, hi, ZB, 1.0 / H) + GS * col_one(lo, hi, GZ, 1.0 / GTHICK)


# the clouds' own signal on the sky, measured live without the air's sky pass (10-27, Overcast noon: c3 with volume
# clouds minus c3 without, both WW_CELL_AIR_RED=nosky): p5..p95 spread 63/255 of the display. Kept = T x that.
CONTRAST = 0.25
ts = {}
for el in (20, 30, 45, 60):
    od = SIG0 * col(cam_z, 1.0e9, top) / math.sin(math.radians(el))
    ts[el] = math.exp(-od)
    print('      elevation %2d deg: optical depth %.3f, T %.4f, cloud contrast left %.1f/255' % (
        el, od, ts[el], 255.0 * CONTRAST * ts[el]))
gate('T', ts[30] >= 0.25 and ts[20] >= 0.12 and min(ts.values()) * CONTRAST * 255.0 > 2.0,
     'T(20) %.4f T(30) %.4f (bars 0.12, 0.25)' % (ts[20], ts[30]))

# ---- wiring in the source
if rev:
    src = subprocess.run(['git', '-C', tree, 'show', '%s:src/gl/cellvolair.cpp' % rev], capture_output=True).stdout
else:
    src = open(os.path.join(tree, 'src', 'gl', 'cellvolair.cpp'), 'rb').read()
src = src.decode('utf-8', 'replace')
w1 = re.search(r'topW\s*=\s*s\.sphereTop\s*\?\s*bs\.center\[2\]\s*\+\s*bs\.radius\s*:\s*vaShapesTop\(\s*scene,\s*bs\s*\)', src)
w2 = 'r == "spheretop"' in src
m = re.search(r'static float vaShapesTop\(.*?\n}\n', src, re.S)
w3 = bool(m) and 'std::max( t, b.center[2] + b.radius )' in m.group(0) and 'FarLOD' in m.group(0)
gate('W', bool(w1) and w2 and w3, 'topW via vaShapesTop %s, red parsed %s, helper max-of-tops %s%s' % (
    bool(w1), w2, w3, ' (rev %s)' % rev if rev else ''))

print('CLOUDVEIL-CHECK %s%s%s' % ('PASS' if not fails else 'FAIL (%s)' % ' '.join(fails),
                                 ' red=' + red if red else '', ' rev=' + rev if rev else ''))
sys.exit(1 if fails else 0)
