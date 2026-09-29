# MERGE1 copy of MAPS1 offline_water.py: input paths come from env MAPS_CW (and MAPS_V2); nothing else changed.
"""offline_water.py -- MAPS1 water: whole-Commonwealth top-down, one panel per water field, and the refuters.

Readers (neither shares code with the viewer):
  v2 cell table  -- tests/spells/lodl_open_authority.Lodt.cell()  (lo, hi, water height, water type, flags)
  v3 planes      -- scratchpad/water2_20260909/lodl_v3_authority.LodlV3 (body table, body-ID / flow / shore)
Files: this bake's Commonwealth.lodl (v2) and the water5 lane's regenerated.lodl (v3, --water-bodies).
Panels are north up; the Boston render region (cells -5..2, -10..-3) is outlined in white."""
import sys, os, json, struct, math, zlib
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, 'E:/Projects/NifskopeWWE-bake2/tests/spells')
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/water2_20260909')
import lodl_open_authority as LA
import lodl_v3_authority as W3

HERE = os.path.dirname(os.path.abspath(__file__))
SP = 'C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad'
V2 = os.environ['MAPS_V2']   # MERGE1: the merged exe's --no-water-bodies .lodl
V3 = os.environ['MAPS_CW'] + '/Commonwealth.lodl'   # MERGE1: the installed bake (v3 by default)
REG = (-5, -10, 2, -3)
out = {}


def ramp(t):
    """t in 0..1 -> a blue-to-yellow ramp (dark blue, cyan, green, yellow)."""
    stops = np.array([[20, 30, 110], [30, 150, 220], [60, 200, 120], [250, 230, 60]], dtype=np.float64)
    t = np.clip(t, 0, 1) * 3
    i = np.minimum(t.astype(int), 2)
    f = (t - i)[..., None]
    return (stops[i] * (1 - f) + stops[i + 1] * f).astype(np.uint8)


def hashcol(ids):
    ids = ids.astype(np.int64)
    out = np.zeros(ids.shape + (3,), np.uint8)
    for k, s in enumerate((12.9898, 78.233, 45.164)):
        x = np.sin((ids + 1) * s) * 43758.5453
        out[..., k] = ((x - np.floor(x)) * 0.8 + 0.2) * 255
    return out


def save(name, rgb, cells_per_px, minx, maxy, note):
    im = Image.fromarray(rgb)
    d = ImageDraw.Draw(im)
    x0, y0, x1, y1 = REG
    d.rectangle([(x0 - minx) / cells_per_px, (maxy - y1) / cells_per_px,
                 (x1 + 1 - minx) / cells_per_px - 1, (maxy + 1 - y0) / cells_per_px - 1], outline=(255, 255, 255), width=2)
    im.save(os.path.join(HERE, 'pics', name + '.png'))
    out[name] = note


# ---------------------------------------------------------------- v2: the per-cell table
d2 = LA.Lodt(V2)
cx, cy = d2.cellsX, d2.cellsY
raw = open(V2, 'rb').read()
cells = np.frombuffer(raw, dtype=np.dtype([('lo', '<f4'), ('hi', '<f4'), ('wh', '<f4'), ('wt', '<u2'), ('fl', '<u2')]),
                      count=cx * cy, offset=d2.oCell).reshape(cy, cx)[::-1]   # row 0 = SOUTH in the file -> flip north up
# spot-check the vectorised read against the reader's own cell() on three cells
for (ax, ay) in ((0, 0), (-3, -4), (20, 20)):
    a = d2.cell(ax, ay)
    r = cells[d2.maxY - ay, ax - d2.minX]
    assert abs(a[2] - r['wh']) < 1e-3 and a[3] == r['wt'] and a[4] == r['fl'], (ax, ay, a, r)
wet = (cells['fl'] & 1) > 0
wh = cells['wh']
out['v2_cells'] = {'cells': int(cx * cy), 'wet': int(wet.sum()), 'land': int(((cells['fl'] & 2) > 0).sum()),
                   'wh_min': float(wh[wet].min()), 'wh_max': float(wh[wet].max()),
                   'types': sorted(int(t) for t in np.unique(cells['wt'][wet]))}
S = 8   # 8 px a cell -> 1536 px
def up(a):
    return np.repeat(np.repeat(a, S, 0), S, 1)
lo_, hi_ = -200.0, 1200.0
rgb = ramp((wh - lo_) / (hi_ - lo_)); rgb[~wet] = (40, 40, 44)
save('WC_v2_waterheight', up(rgb), 1.0 / S, d2.minX, d2.maxY,
     'per-cell water height, ramp %g..%g units (dark blue low, yellow high); grey = no water' % (lo_, hi_))
wt = cells['wt'].astype(np.int64)
rgb = hashcol(wt); rgb[wt == 0xFFFF] = (0, 230, 230); rgb[~wet] = (40, 40, 44)
save('WC_v2_watertype', up(rgb), 1.0 / S, d2.minX, d2.maxY,
     'per-cell WATR index hashed to colour; cyan = the worldspace default 0xFFFF; grey = no water')
rgb = np.zeros((cy, cx, 3), np.uint8); rgb[...] = (40, 40, 44)
land = (cells['fl'] & 2) > 0
rgb[land & ~wet] = (60, 170, 60); rgb[wet & ~land] = (210, 50, 50); rgb[wet & land] = (230, 220, 50)
save('WC_v2_cellflags', up(rgb), 1.0 / S, d2.minX, d2.maxY, 'cell flags: green land only, red water only, yellow both')
# terrain height at cell centre (lo/hi mean) vs water height -> where the water plane is ABOVE the ground
gap = wh - (cells['lo'] + cells['hi']) / 2.0

# ---------------------------------------------------------------- v3: body table + planes
w = W3.LodlV3(V3)
out['v3'] = {'version': w.version, 'bodies': w.nBody, 'bodySamplesPerCell': w.bodyS, 'flowS': w.flowS,
             'shoreS': w.shoreS, 'shoreQuantum': w.shoreQ, 'strokes': w.strokes, 'sect': w.sect,
             'dye': bool(w.sect & (1 << 8))}
names = {}
if w.nameLen:
    w.f.seek(w.oName); blob = w.f.read(w.nameLen)
    for b in w.bodies:
        if b['nameOffset']:
            names[b['id']] = blob[b['nameOffset']:].split(b'\0')[0].decode('latin-1')


def plane(s):
    tx, ty, e = s['tilesX'], s['tilesY'], s['tileEdge']
    dt = '<u2' if s['bps'] == 2 else 'u1'
    a = np.zeros((ty * e, tx * e), dtype=np.uint16 if s['bps'] == 2 else np.uint8)
    for j in range(ty):
        for i in range(tx):
            kind, v = w.tile(s, i, j)
            if kind == 'uniform':
                a[j * e:(j + 1) * e, i * e:(i + 1) * e] = v
            else:
                a[j * e:(j + 1) * e, i * e:(i + 1) * e] = np.frombuffer(v, dtype=dt).reshape(e, e)
    return a   # row 0 = SOUTH


ids = plane(w.idStore)
flow = plane(w.flowStore)
shore = plane(w.shoreStore)
print('planes', ids.shape, flow.shape, shore.shape)
# the reader's own slow path must agree with the vectorised one on random texels
rng = np.random.default_rng(1)
for _ in range(200):
    x, y = int(rng.integers(0, ids.shape[1])), int(rng.integers(0, ids.shape[0]))
    assert w.sample(w.idStore, x, y) == ids[y, x]
# area per body from the plane == the table's area field (two measurements of one thing)
cnt = np.bincount(ids.ravel(), minlength=w.nBody + 1)
bad = sum(1 for b in w.bodies if cnt[b['id']] != b['area'])
out['v3']['area_mismatch_bodies'] = bad
spc = w.bodyS
ext_cells = ids.shape[1] // spc


def north_up(a):
    return a[::-1]


K = 4   # keep every 4th sample -> 8 px a cell (nearest: an id is a name)
idn = north_up(ids)[::K, ::K]
rgb = hashcol(idn); rgb[idn == 0] = (40, 40, 44)
save('WC_v3_bodyid', rgb, K / spc, w.minX, w.maxY, 'body-ID plane, id hashed to colour (nearest); grey = dry')
fl = north_up(flow)[::K, ::K].astype(np.int64)
ang = (fl & 0xFF) / 256.0 * 2 * math.pi
spd = (fl >> 8) & 0xF
conf = (fl >> 12) & 0xF
# direction as hue, speed as brightness; still water (0) dark blue, dry grey
hue = ang / (2 * math.pi)
import colorsys
hsv = np.stack([hue, np.ones_like(hue), 0.35 + 0.65 * spd / 15.0], -1)
flat = hsv.reshape(-1, 3)
rgbf = np.array([colorsys.hsv_to_rgb(*t) for t in flat[:0]])   # placeholder so the loop below is vectorised
h6 = (hue * 6.0); i6 = np.floor(h6).astype(int) % 6; f6 = h6 - np.floor(h6); vv = hsv[..., 2]
p_ = np.zeros_like(vv); q_ = vv * (1 - f6); t_ = vv * f6
r = np.choose(i6, [vv, q_, p_, p_, t_, vv]); g = np.choose(i6, [t_, vv, vv, q_, p_, p_]); bch = np.choose(i6, [p_, p_, t_, vv, vv, q_])
rgb = (np.stack([r, g, bch], -1) * 255).astype(np.uint8)
wetm = idn > 0
rgb[wetm & (fl == 0)] = (20, 30, 90); rgb[~wetm] = (40, 40, 44)
save('WC_v3_flow', rgb, K / spc, w.minX, w.maxY,
     'flow plane: hue = direction (red east, yellow-green north, cyan west, blue-violet south), brightness = speed; '
     'dark blue = still water; grey = dry')
sh = north_up(shore)[::K, ::K].astype(np.float64)
rgb = ramp(sh / 64.0); rgb[~wetm] = (40, 40, 44)
save('WC_v3_shore', rgb, K / spc, w.minX, w.maxY,
     'shore distance, ramp 0..64 steps x %d units = 0..%d units (dark blue at the bank, yellow far out); grey = dry'
     % (w.shoreQ, 64 * w.shoreQ))

# ---------------------------------------------------------------- the river in the Boston region: refuters
x0, y0, x1, y1 = REG
sx0, sy0 = (x0 - w.minX) * spc, (y0 - w.minY) * spc
sx1, sy1 = (x1 + 1 - w.minX) * spc, (y1 + 1 - w.minY) * spc
rid = ids[sy0:sy1, sx0:sx1]
present = np.unique(rid[rid > 0])
reg_bodies = []
for bid in present:
    b = w.bodies[bid - 1]
    reg_bodies.append({'id': int(bid), 'class': ['sea', 'river', 'lake'][b['class']] if b['class'] < 3 else b['class'],
                       'height': round(b['height'], 1), 'form': '%08X' % b['form'], 'areaTexels': b['area'],
                       'texelsInRegion': int((rid == bid).sum()), 'flowSource': b['flowSource'],
                       'meanFlow': [round(b['flowX'], 2), round(b['flowY'], 2)], 'name': names.get(int(bid), '')})
reg_bodies.sort(key=lambda b: -b['texelsInRegion'])
out['region_bodies'] = reg_bodies
big = reg_bodies[0]['id'] if reg_bodies else 0
wetR = rid == big
dryR = rid == 0
rf = flow[sy0:sy1, sx0:sx1]
rs = shore[sy0:sy1, sx0:sx1]
out['refute_bodyid'] = {'body': big, 'wetTexels': int(wetR.sum()), 'dryTexels': int(dryR.sum()),
                        'regionTexels': int(rid.size)}
out['refute_flow'] = {'wet_nonzero_share': float((rf[wetR] != 0).mean()) if wetR.any() else None,
                      'dry_nonzero': int((rf[dryR] != 0).sum()),
                      'wet_dir_deg_median': float(np.median((rf[wetR] & 0xFF) / 256.0 * 360)) if wetR.any() else None,
                      'wet_speed_median': float(np.median((rf[wetR] >> 8) & 0xF)) if wetR.any() else None}
out['refute_flow_per_body'] = {str(b['id']): {'class': b['class'], 'flowSource': b['flowSource'],
                                              'nonzero_share': float((rf[rid == b['id']] != 0).mean()),
                                              'dir_deg_median': float(np.median((rf[rid == b['id']] & 0xFF) / 256.0 * 360)),
                                              'mean_flow_deg': round(math.degrees(math.atan2(b['meanFlow'][1], b['meanFlow'][0])) % 360, 1)}
                               for b in reg_bodies}
vals, cn = np.unique(rs[dryR], return_counts=True)
out['refute_shore_dry_values'] = {int(a): int(c) for a, c in zip(vals, cn)}
# the shore distance must grow away from the bank: wet texels next to a dry texel vs the rest
wet_all = rid > 0
edge = np.zeros_like(wet_all)
edge[1:-1, 1:-1] = wet_all[1:-1, 1:-1] & ~(wet_all[:-2, 1:-1] & wet_all[2:, 1:-1] & wet_all[1:-1, :-2] & wet_all[1:-1, 2:])
out['refute_shore_bank_vs_inner'] = {'bankTexels': int(edge.sum()), 'bank_mean_steps': float(rs[edge].mean()),
                                     'inner_mean_steps': float(rs[wet_all & ~edge].mean())}
out['refute_shore'] = {'wet_mean_steps': float(rs[wetR].mean()) if wetR.any() else None,
                       'wet_max_steps': int(rs[wetR].max()) if wetR.any() else None,
                       'dry_max_steps': int(rs[dryR].max()) if dryR.any() else None}
# v2 per-cell water over the same river: the cells the river body covers vs the region's cells it does not
bx = np.zeros((y1 - y0 + 1, x1 - x0 + 1), bool)
for j in range(y1 - y0 + 1):
    for i in range(x1 - x0 + 1):
        blk = rid[j * spc:(j + 1) * spc, i * spc:(i + 1) * spc]
        bx[j, i] = (blk == big).mean() > 0.25
c2 = cells[d2.maxY - np.arange(y0, y1 + 1)][:, x0 - d2.minX:x1 + 1 - d2.minX]   # rows south->north like bx
g2 = gap[d2.maxY - np.arange(y0, y1 + 1)][:, x0 - d2.minX:x1 + 1 - d2.minX]
out['refute_v2_waterheight'] = {
    'riverCells': int(bx.sum()), 'otherCells': int((~bx).sum()),
    'river_wh_mean': float(c2['wh'][bx].mean()) if bx.any() else None,
    'river_body_height': reg_bodies[0]['height'] if reg_bodies else None,
    'river_water_minus_ground_mean': float(g2[bx].mean()) if bx.any() else None,
    'other_water_minus_ground_mean': float(g2[~bx].mean()),
    'other_wh_mean': float(c2['wh'][~bx].mean())}
# Commonwealth-wide: a known lake/sea vs dry land in the v3 id plane -- water plane above ground where wet
json.dump(out, open(os.path.join(HERE, 'offline_water.json'), 'w'), indent=1)
print(json.dumps(out, indent=1))
