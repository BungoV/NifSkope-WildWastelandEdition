"""Lane GLASSPANE1 (FIX1030B, 2026-10-30): the offline census behind tests/glasspane1 (no NifSkope, no window).

Reads the game's own files by bytes (Fallout4 - Materials.ba2, the texture BA2s, Fallout4 - Meshes.ba2; loose files
of the given resource roots win, last root first) and writes one TSV row per material for the C++ unit test
(glasspane_test.cpp), which judges the C++ rules (src/gl/glasspanerules.h) on them:

  A  the Concord hardware store's glass (Bld03CornerBrickACom01Open01.nif): every glass shape of the NIF, every
     GlassWindows*.bgsm of the archive: receive shadows, cast shadows, env map + mask + fresnel inputs, the textures'
     numbers (albedo over the alpha-test-kept texels, _s R / G, the own cube's mean), and the twin of the shade's terms
     per unit of light: albedo, the env cube by the game's deferred composite law (CUBE1: 3 x sat(spec x _s.R) x
     min(sqrt(sat(gloss x _s.G - 0.3)), 1) x min(env scale, 50) x the cube, sRGB decoded) and by our shader's
     (cell_lights.glsl cellCubeGame, transcribed separately), the backlight our shader adds (fo4_default.frag adds
     albedo x N.(-L) x backlightPower whenever the power is > 0, the material's back-lighting switch unread).
     Plus the count of every BGSM whose receive-shadows flag is 0, by folder (what the rule moves).
  B  every effect material (BGEM) with env mapping and an env mask: the mask's R and G means (the game scales the
     cube by R, NifSkope read G), the normal map's alpha mean (the game's other factor), the pane rule
     (skill fo4-glass-vs-haze-material-rule: blend 6/7, env, not decal, not soft, alpha > 0).

usage: python -I tests/glasspane1/glass_census.py <out.tsv> [<Data dir> [<resource root> ...]]
"""
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, 'tests', 'spells'))
sys.path.insert(0, os.path.join(REPO, 'tools', 'rigging_prototype'))
from ddsdec import decode, facts, srgb2lin  # noqa: E402
from near_texture_stack import TexStack  # noqa: E402

DATA = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data'
BS = chr(92)
STORE_NIF = 'meshes' + BS + 'architecture' + BS + 'buildings' + BS + 'smalltown' + BS + 'bld03cornerbrickacom01open01.nif'


def rel(prefix, p):
    p = p.replace('/', BS).lower().lstrip(BS)
    return p if p.startswith(prefix + BS) else prefix + BS + p


class Rd:
    def __init__(self, b, o=4):
        self.b, self.o = b, o

    def t(self, fmt):
        v = struct.unpack_from('<' + fmt, self.b, self.o)
        self.o += struct.calcsize('<' + fmt)
        return v[0] if len(v) == 1 else v

    def s(self):
        n = self.t('I')
        v = self.b[self.o:self.o + n].rstrip(b'\0').decode('latin1')
        self.o += n
        return v


def head(r, m):
    m['ver'] = ver = r.t('I')
    r.t('I'); r.t('4f')
    m['alpha'] = r.t('f')
    m['blend'], m['src'], m['dst'] = r.t('B'), r.t('I'), r.t('I')
    m['testref'], m['test'] = r.t('B'), r.t('B')
    r.t('B'); r.t('B'); r.t('B'); r.t('B')
    m['decal'] = r.t('B')
    m['twosided'] = r.t('B')
    r.t('B')
    m['nonocc'] = r.t('B')
    r.t('B'); r.t('B'); r.t('f')
    if ver < 10:
        m['env'] = r.t('B')
        m['envscale'] = r.t('f')
    else:
        r.t('B')
    r.t('B')
    if ver >= 6:
        r.t('B')
    return ver


def bgsm(b):
    """The BGSM fields this census reads (version < 10; src/io/materialfile.cpp's order). None if not readable."""
    if b[:4] != b'BGSM':
        return None
    r, m = Rd(b), {'kind': 'bgsm'}
    ver = head(r, m)
    if ver >= 10:
        return None
    tex = [r.s() for _ in range(10 if ver >= 17 else 9)]
    m['tex_d'], m['tex_n'], m['tex_s'], m['tex_env'] = tex[0], tex[1], tex[2], tex[4]
    r.t('B')
    r.t('B'); r.t('f')
    m['backlightpower'] = r.t('f')
    r.t('B'); r.t('f')
    m['specon'] = r.t('B')
    r.t('3f')
    m['specmult'] = r.t('f')
    m['smooth'] = r.t('f')
    m['fresnel'] = r.t('f')
    r.t('6f')
    if ver > 2:
        r.t('B')
    r.s()
    r.t('B')
    if r.t('B'):
        r.t('3f')
    r.t('f'); r.t('B'); r.t('B')
    m['backlighting'] = r.t('B')
    m['receive'] = r.t('B')
    r.t('B')
    m['cast'] = r.t('B')
    return m


def bgem(b):
    """BGEM fields (versions 1-2 and >= 10; notes fxcheck1 bgem.py's order). None if not readable."""
    if b[:4] != b'BGEM':
        return None
    r, m = Rd(b), {'kind': 'bgem'}
    ver = head(r, m)
    tex = [r.s() for _ in range(5)]
    m['tex_base'], m['tex_n'], m['tex_mask'] = tex[0], tex[3], tex[4]
    if ver >= 11:
        for _ in range(3):
            r.s()
    if ver >= 10:
        m['env'] = r.t('B')
        m['envscale'] = r.t('f')
    r.t('B')
    m['efflit'] = r.t('B')
    r.t('B'); r.t('B'); r.t('B')
    m['soft'] = r.t('B')
    return m


def is_pane(m):
    return (m['blend'] and m['src'] == 6 and m['dst'] == 7 and m.get('env') and not m['decal'] and not m.get('soft')
            and m['alpha'] > 0)


def mip_for(b, side=128):
    _dxgi, w, h, mips, _cube, _off = facts(b)
    m = 0
    while m + 1 < mips and max(w >> m, h >> m) > side:
        m += 1
    return m


def tex2d(st, path, side=128):
    """(rgba array, source) of a texture at the first mip <= side, or (None, why)."""
    if not path:
        return None, 'none named'
    k = rel('textures', path)
    b = st.read(k)
    if not b:
        return None, 'missing'
    try:
        return decode(b, mip_for(b, side)), st.where(k)
    except Exception as e:  # noqa: BLE001  (a format the decoder lacks: BC7, ...)
        return None, 'undecoded: %s' % e


def cube_lin(st, path):
    if not path:
        return None
    b = st.read(rel('textures', path))
    if not b:
        return None
    try:
        return float(np.mean([srgb2lin(decode(b, mip=0, face=k)[..., :3]).mean() for k in range(6)]))
    except Exception:  # noqa: BLE001
        return None


def cube_game(spec_mult, smooth, R, G, env_scale):
    """The game's deferred composite (lane CUBE1's read of it): per texel, x the cube and the diffuse light."""
    return 3.0 * np.clip(spec_mult * R, 0, 1) * np.minimum(np.sqrt(np.clip(smooth * G - 0.3, 0, 1)), 1.0) \
        * min(env_scale, 50.0)


def cube_ours(spec_mult, smooth, R, G, env_scale):
    """res/shaders/cell_lights.glsl cellCubeGame(gloss = cellCubeMat.y x _s.G, spec = cellCubeMat.x x _s.R): k =
    3 x clamp(spec) x min(sqrt(clamp(g - 0.3)), 1) x clamp(envScale, 0, 50), g = clamp(gloss)."""
    g = np.clip(smooth * G, 0.0, 1.0)
    return 3.0 * np.clip(spec_mult * R, 0.0, 1.0) * np.minimum(np.sqrt(np.clip(g - 0.3, 0.0, 1.0)), 1.0) \
        * np.clip(env_scale, 0.0, 50.0)


def nif_glass(st):
    """[(shape name, material)] of the store NIF's shapes whose material names glass."""
    import contextlib
    import io
    import tempfile
    import nifparse
    b = st.read(STORE_NIF)
    if not b:
        return None
    tmp = os.path.join(os.environ.get('TMPDIR', HERE), 'glasspane1_store.nif')
    try:
        open(tmp, 'wb').write(b)
    except OSError:
        tmp = os.path.join(tempfile.gettempdir(), 'glasspane1_store.nif')
        open(tmp, 'wb').write(b)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        data, _hdr, strings, blocks = nifparse.parse(tmp)
    os.remove(tmp)
    byid = {i: (t, s) for i, t, s, _z in blocks}
    out = []
    for i, tname, start, _size in blocks:
        if tname not in ('BSTriShape', 'BSMeshLODTriShape', 'BSSubIndexTriShape'):
            continue
        name_i = struct.unpack_from('<i', data, start)[0]
        o = start + 4
        ne = struct.unpack_from('<I', data, o)[0]
        o += 4 + 4 * ne + 8 + 12 + 36 + 4 + 4 + 16 + 4
        sh = struct.unpack_from('<i', data, o)[0]
        if sh not in byid:
            continue
        # FO4 (BS version 130): a BSLightingShaderProperty opens with its shader type, then the name (= material path)
        mi = struct.unpack_from('<i', data, byid[sh][1] + (4 if byid[sh][0] == 'BSLightingShaderProperty' else 0))[0]
        mat = strings[mi] if 0 <= mi < len(strings) else ''
        low = mat.lower().replace('/', BS)
        if ('materials' + BS) in low:
            mat = mat[low.index('materials' + BS):]	# the export's absolute path ("C:\Projects\...\Data\materials\...")
        if 'glass' in mat.lower():
            out.append((strings[name_i] if 0 <= name_i < len(strings) else '?', mat))
    return out


def main(a):
    out = a[0]
    data = a[1] if len(a) > 1 else DATA
    roots = [data] + a[2:]
    st = TexStack(roots, 'stack')
    print('glasspane1 census: %d resource root(s), %d archive entries' % (len(roots), len(st.arch)))
    rows = []

    # ---- A: the store's glass
    shapes = nif_glass(st)
    print('A store NIF %s: %s' % (STORE_NIF, 'MISSING' if shapes is None else '%d glass shape(s)' % len(shapes)))
    for s, m in shapes or []:
        print('A   shape %r material %s' % (s, m))
    names = sorted(k for k, v in st.arch.items() if k.startswith('materials' + BS) and k.endswith('.bgsm'))
    glass = [k for k in names if os.path.basename(k).startswith('glasswindows')]
    for s, m in shapes or []:
        k = rel('materials', m)
        if k not in glass:
            glass.append(k)
    recv0 = {}
    unread = 0
    for k in names:
        b = st.read(k)
        m = bgsm(b) if b else None
        if not m:
            unread += 1
            continue
        if not m['receive']:
            top = k.split(BS)[1] if k.count(BS) > 1 else '-'
            recv0[top] = recv0.get(top, 0) + 1
    nread = len(names) - unread
    n0 = sum(recv0.values())
    print('A every BGSM: %d read, %d not (version >= 10 or not a BGSM); receive shadows 0: %d (%s)' % (
        nread, unread, n0, ', '.join('%s %d' % kv for kv in sorted(recv0.items(), key=lambda kv: -kv[1]))))
    # The brief's rule "a BGSM with receive shadows 0 takes no shadow" would unshadow this share of all materials
    # (walls, terrain, actors, which the game shadows): above half, the flag cannot mean that in the game's renderer.
    share = n0 / max(nread, 1)
    print('A VERDICT receive-shadows rule: %s (%.1f%% of the materials say 0; bar: a rule that unshadows more than '
          'half of them is refused)' % ('REFUSED' if share > 0.5 else 'possible', 100.0 * share))
    print('A %-52s %4s %4s %3s %5s %5s %5s %4s %3s %6s | %6s %6s %6s %6s | %8s %8s %6s %5s' % (
        'material', 'recv', 'cast', 'env', 'escl', 'spec', 'smth', 'frsn', 'bkl', 'bkpow', 'albLin', 'sR', 'sG',
        'cube', 'envGame', 'envOurs', 'env/al', 'keep'))
    ds = ss = '-'
    for k in glass:
        b = st.read(k)
        m = bgsm(b) if b else None
        if not m:
            print('A %s UNREADABLE' % k)
            rows.append(('bgsm', k, 'unreadable', '', '', ''))
            continue
        d, ds = tex2d(st, m['tex_d'], 256)
        s, ss = tex2d(st, m['tex_s'], 256)
        cl = cube_lin(st, m['tex_env']) if m['env'] else 0.0
        alb = sR = sG = eg = eo = keepf = float('nan')
        if d is not None:
            keep = d[..., 3] >= (m['testref'] / 255.0 if m['test'] else 0.0)
            keepf = float(keep.mean())
            lin = srgb2lin(d[..., :3]) @ np.array([0.2126, 0.7152, 0.0722])
            alb = float(lin[keep].mean())
            if s is not None:
                if s.shape[:2] != d.shape[:2]:
                    ky = np.linspace(0, s.shape[0] - 1, d.shape[0]).astype(int)
                    kx = np.linspace(0, s.shape[1] - 1, d.shape[1]).astype(int)
                    s = s[ky][:, kx]
                R, G = s[..., 0][keep], s[..., 1][keep]
                sR, sG = float(R.mean()), float(G.mean())
                spec = m['specmult'] if m['specon'] else 0.0
                if cl is not None and m['env']:
                    eg = float(cube_game(spec, m['smooth'], R, G, m['envscale']).mean()) * cl
                    eo = float(cube_ours(spec, m['smooth'], R, G, m['envscale']).mean()) * cl
        print('A %-52s %4d %4d %3d %5.2f %5.2f %5.2f %4.1f %3d %6.2f | %6.4f %6.3f %6.3f %6.4f | %8.5f %8.5f %6.3f %5.3f' % (
            k[len('materials' + BS):], m['receive'], m['cast'], m['env'], m['envscale'], m['specmult'], m['smooth'],
            m['fresnel'], m['backlighting'], m['backlightpower'], alb, sR, sG, cl if cl is not None else float('nan'),
            eg, eo, eg / alb if alb == alb and alb > 0 else float('nan'), keepf))
        rows.append(('bgsm', k, m['receive'], m['cast'], '%.6f' % eg, '%.6f' % eo))
    print('A   textures from (last row): d %s | s %s' % (ds, ss))
    print('A   (env/al = the env cube term over the albedo, both per unit of the diffuse light; envGame = envOurs is the '
          'cube law parity; our backlight adds albedo x N.(-L) x bkpow whatever bkl says, 0 on a face toward the sun)')

    # ---- B: effect materials with an env mask
    gems = sorted(k for k in st.arch if k.startswith('materials' + BS) and k.endswith('.bgem'))
    n_env = n_mask = n_dec = 0
    undec = {}
    Rs, Gs, NAs, panes_g1 = [], [], [], []
    for k in gems:
        b = st.read(k)
        try:
            m = bgem(b) if b else None
        except struct.error:
            m = None
        if not m or not m.get('env'):
            continue
        n_env += 1
        if not m['tex_mask']:
            continue
        n_mask += 1
        t, why = tex2d(st, m['tex_mask'])
        if t is None:
            undec[why.split(':')[0]] = undec.get(why.split(':')[0], 0) + 1
            rows.append(('bgem', k, int(bool(is_pane(m))), 'nan', 'nan', why))
            continue
        n_dec += 1
        R, G = float(t[..., 0].mean()), float(t[..., 1].mean())
        nt, _w = tex2d(st, m['tex_n'])
        na = float(nt[..., 3].mean()) if nt is not None else float('nan')
        Rs.append(R)
        Gs.append(G)
        NAs.append(na)
        pane = bool(is_pane(m))
        if pane:
            panes_g1.append((G >= 0.98, R < 0.98))
        rows.append(('bgem', k, int(pane), '%.6f' % R, '%.6f' % G, '%.4f' % na))
    Rs, Gs = np.array(Rs), np.array(Gs)
    print('B BGEMs %d, env mapped %d, with an env mask %d, decoded %d, not decoded %s' % (
        len(gems), n_env, n_mask, n_dec, undec or 0))
    if n_dec:
        ratio = Gs / np.maximum(Rs, 1e-3)
        print('B env mask means over decoded: R %.3f G %.3f; G >= 0.98 on %d of %d; G/R median %.2f (the cube the old '
              'G read adds per unit the game adds)' % (Rs.mean(), Gs.mean(), int((Gs >= 0.98).sum()), n_dec,
                                                     float(np.median(ratio))))
        npn = len(panes_g1)
        both = sum(1 for g1, r1 in panes_g1 if g1 and r1)
        print('B panes (the glass rule) with a decoded mask: %d; G == 1 but R < 1 on %d (%.0f%%)' % (
            npn, both, 100.0 * both / max(npn, 1)))
        nav = np.array([x for x in NAs if x == x])
        print('B normal map alpha (the game\'s other factor, not read by our effect shader): mean %.3f, < 0.98 on %d of %d'
              % (nav.mean() if len(nav) else float('nan'), int((nav < 0.98).sum()), len(nav)))
    with open(out, 'w', encoding='ascii', errors='replace', newline='\n') as f:
        f.write('# kind\tpath\tA: receive | B: pane\tA: cast | B: maskR\tA: envGame | B: maskG\tA: envOurs | B: normalA\n')
        for r in rows:
            f.write('\t'.join(str(x) for x in r) + '\n')
    print('census rows %d -> %s' % (len(rows), out))
    return 0 if rows else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
