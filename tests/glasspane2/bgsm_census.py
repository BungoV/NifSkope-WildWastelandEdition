"""Lane GLASSPANE2 (FIX1030B round 2, 2026-10-30): is NifSkope's BGSM reader aligned, and what do receiveShadows and
bBackLighting really say in the game's own files? Offline: the game's BA2s by bytes, no NifSkope, no window.

READER UNDER TEST. `read_bgsm()` is a line-for-line transcription of src/io/materialfile.cpp (Material::readFile +
ShaderMaterial::readFile, every version branch), so a misread there is a misread here. Two reds show the gates can
fail: `RED=short` (one byte short before the textures: R1) and `RED=swap` (cast/receive/hide read in another order,
same byte count: R1 passes, R2 must fail).

REFERENCES (independent of that order):
  R1  every BGSM must be consumed to its LAST byte (a misaligned field order leaves bytes or runs past the end);
  R2  Bethesda's exporter wrote the NIF's BSLightingShaderProperty (Fallout4 - Meshes*.ba2) from the same material:
      its flag bits (release/nif.xml Fallout4ShaderPropertyFlags1/2) and floats (Grayscale to Palette Scale, Fresnel
      Power, Smoothness, Backlight/Rimlight Power, ...) must agree with what the reader got for that material. Fields
      AFTER receiveShadows (castShadows, hair, skin tint, tessellate, grayscale scale) prove the receive byte's place.
  R3  path semantics: hair only on hair materials, skin tint on skin, tree on trees, terrain on landscape.

usage: python -I tests/glasspane2/bgsm_census.py <out.txt> [<Data dir> [<resource root> ...]]
       env MAXNIF=<n> caps the NIF walk (0 = all, the default); RED=short|swap = a misread reader (must FAIL).
Prints `BGSMRECV PASS <n>` or `BGSMRECV FAIL <what>` lines; rc 0 only when every gate passes.
"""
import os
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, 'tests', 'spells'))
from near_texture_stack import TexStack  # noqa: E402

DATA = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data'
BS = chr(92)
RED = os.environ.get('RED', '')	# '' = the reader under test; 'short' / 'swap' = the two reds
FLT_MAX = 3.4028234663852886e38


class Rd:
    def __init__(self, b, o=4):
        self.b, self.o = b, o

    def t(self, fmt):
        v = struct.unpack_from('<' + fmt, self.b, self.o)
        self.o += struct.calcsize('<' + fmt)
        return v[0] if len(v) == 1 else v

    def s(self):	# QDataStream >> char*: quint32 length, then that many bytes (the NUL included)
        n = self.t('I')
        v = self.b[self.o:self.o + n].rstrip(b'\0').decode('latin1')
        if self.o + n > len(self.b):
            raise struct.error('string past the end')
        self.o += n
        return v


def read_bgsm(b):
    """src/io/materialfile.cpp's order, every branch. Returns (fields dict, bytes left after the last field)."""
    r, m = Rd(b), {}
    v = m['version'] = r.t('I')
    m['tileFlags'] = r.t('I')
    m['uv'] = r.t('4f')
    m['alpha'] = r.t('f')
    m['alphaBlend'], m['alphaSrc'], m['alphaDst'] = r.t('B'), r.t('I'), r.t('I')
    m['alphaTestRef'] = r.t('B')
    m['alphaTest'], m['zWrite'], m['zTest'] = r.t('B'), r.t('B'), r.t('B')
    m['ssr'], m['wetSsr'] = r.t('B'), r.t('B')
    m['decal'], m['twoSided'], m['decalNoFade'], m['nonOccluder'] = r.t('B'), r.t('B'), r.t('B'), r.t('B')
    m['refraction'], m['refractionFalloff'], m['refractionPower'] = r.t('B'), r.t('B'), r.t('f')
    m['envMapping'] = r.t('B')
    if v < 10:
        m['envMaskScale'] = r.t('f')
    m['grayscaleToPaletteColor'] = r.t('B')
    if v >= 6:
        m['maskWrites'] = r.t('B')
    if RED == 'short':
        r.o -= 1	# red 1: a reader one byte short here (a missed version branch): R1 must fail
    tex = [r.s() for _ in range(10 if v >= 17 else 9)]
    m['tex'] = tex
    m['editorAlphaRef'] = r.t('B')
    if v >= 8:
        m['translucency'] = r.t('3B')
        m['translucencyColor'] = r.t('3f')
        m['translucencyScale'], m['translucencyTurbulence'] = r.t('f'), r.t('f')
    else:
        m['rimLighting'], m['rimPower'], m['backLightPower'] = r.t('B'), r.t('f'), r.t('f')
        m['subsurface'], m['subsurfaceRolloff'] = r.t('B'), r.t('f')
    m['specularEnabled'] = r.t('B')
    m['specularColor'] = r.t('3f')
    m['specularMult'], m['smoothness'], m['fresnelPower'] = r.t('f'), r.t('f'), r.t('f')
    m['wetSpecScale'], m['wetSpecPowerScale'], m['wetSpecMinvar'] = r.t('f'), r.t('f'), r.t('f')
    if v < 10:
        m['wetEnvMapScale'] = r.t('f')
    m['wetFresnelPower'], m['wetMetalness'] = r.t('f'), r.t('f')
    if v > 2:
        m['pbr'] = r.t('B')
    if v >= 9:
        m['customPorosity'], m['porosity'] = r.t('B'), r.t('f')
    m['rootMaterial'] = r.s()
    m['anisoLighting'], m['emitEnabled'] = r.t('B'), r.t('B')
    if m['emitEnabled']:
        m['emitColor'] = r.t('3f')
    m['emitMult'], m['modelSpaceNormals'] = r.t('f'), r.t('B')
    m['externalEmittance'] = r.t('B')
    if v >= 12:
        m['lumEmittance'] = r.t('f')
    if v >= 13:
        m['adaptiveEmissive'] = (r.t('B'), r.t('f'), r.t('f'), r.t('f'))
    if v < 8:
        m['backLighting'] = r.t('B')
    if RED == 'swap':	# red 2: the same byte count, the cast byte read first (R1 cannot see it; R2 must fail)
        m['castShadows'], m['receiveShadows'], m['hideSecret'] = r.t('B'), r.t('B'), r.t('B')
    else:
        m['receiveShadows'], m['hideSecret'], m['castShadows'] = r.t('B'), r.t('B'), r.t('B')
    m['dissolveFade'], m['assumeShadowmask'], m['glowmap'] = r.t('B'), r.t('B'), r.t('B')
    if v < 7:
        m['envWindow'], m['envEye'] = r.t('B'), r.t('B')
    m['hair'] = r.t('B')
    m['hairTint'] = r.t('3f')
    m['tree'], m['facegen'], m['skinTint'], m['tessellate'] = r.t('B'), r.t('B'), r.t('B'), r.t('B')
    if v < 3:
        m['displacement'] = r.t('5f')
    m['grayscaleToPaletteScale'], m['skewSpecularAlpha'] = r.t('f'), r.t('B')
    if v >= 3:
        m['terrain'] = r.t('B')
        if m['terrain']:
            if v == 3:
                r.o += 4
            m['terrainParams'] = r.t('3f')
    return m, len(b) - r.o


# ---- NIF side (release/nif.xml, BS version 130), written here: a header walk + the shader property's fields ----
def nif_lsp(b):
    """[(material path, flags1, flags2, floats dict)] for every BSLightingShaderProperty of a FO4 NIF."""
    o = b.index(b'\n') + 1
    ver, = struct.unpack_from('<I', b, o); o += 4
    o += 1 + 4
    nblocks, bsver = struct.unpack_from('<II', b, o); o += 8
    if ver != 0x14020007 or bsver != 130:
        return []
    for _ in range(4):	# Author, Process Script, Export Script, Max Filepath (byte length + chars)
        o += 1 + b[o]
    ntypes, = struct.unpack_from('<H', b, o); o += 2
    types = []
    for _ in range(ntypes):
        n, = struct.unpack_from('<I', b, o)
        types.append(b[o + 4:o + 4 + n].decode('latin1')); o += 4 + n
    tidx = struct.unpack_from('<%dH' % nblocks, b, o); o += 2 * nblocks
    bsize = struct.unpack_from('<%dI' % nblocks, b, o); o += 4 * nblocks
    nstr, = struct.unpack_from('<I', b, o); o += 8
    strings = []
    for _ in range(nstr):
        n, = struct.unpack_from('<I', b, o)
        strings.append(b[o + 4:o + 4 + n].decode('latin1')); o += 4 + n
    ngroups, = struct.unpack_from('<I', b, o); o += 4 + 4 * ngroups
    out = []
    for i in range(nblocks):
        start = o
        o += bsize[i]
        if types[tidx[i]] != 'BSLightingShaderProperty':
            continue
        p = start
        stype, name = struct.unpack_from('<Ii', b, p); p += 8
        ne, = struct.unpack_from('<I', b, p); p += 4 + 4 * ne + 4
        f1, f2 = struct.unpack_from('<II', b, p); p += 8
        p += 16 + 4	# UV offset, UV scale, texture set
        f = {}
        f['emitColor'] = struct.unpack_from('<3f', b, p); p += 12
        f['emitMult'], = struct.unpack_from('<f', b, p); p += 4
        p += 4	# Root Material
        p += 4	# Texture Clamp Mode
        f['alpha'], f['refractionPower'], f['smoothness'] = struct.unpack_from('<3f', b, p); p += 12
        f['specularColor'] = struct.unpack_from('<3f', b, p); p += 12
        f['specularMult'], f['subsurfaceRolloff'], f['rimPower'] = struct.unpack_from('<3f', b, p); p += 12
        f['backLightPower'] = None
        if FLT_MAX * 0.999 <= f['rimPower'] < float('inf'):
            f['backLightPower'], = struct.unpack_from('<f', b, p); p += 4
        f['grayscaleToPaletteScale'], f['fresnelPower'] = struct.unpack_from('<2f', b, p); p += 8
        f['shaderType'] = stype
        mat = strings[name] if 0 <= name < len(strings) else ''
        out.append((mat, f1, f2, f))
    return out


def mat_key(p):
    low = p.lower().replace('/', BS)
    if ('materials' + BS) in low:
        low = low[low.index('materials' + BS):]
    elif not low.startswith('materials' + BS):
        low = 'materials' + BS + low.lstrip(BS)
    return low


# BGSM field -> (flags word, bit) by release/nif.xml's Fallout4ShaderPropertyFlags names
BITS = [
    ('castShadows', 1, 9, 'Cast_Shadows'),
    ('hair', 1, 18, 'Hair'),
    ('skinTint', 1, 21, 'Skin_Tint'),
    ('tessellate', 1, 25, 'Tessellate'),
    ('modelSpaceNormals', 1, 12, 'Model_Space_Normals'),
    ('externalEmittance', 1, 29, 'External_Emittance'),
    ('emitEnabled', 1, 22, 'Own_Emit'),
    ('envMapping', 1, 7, 'Environment_Mapping'),
    ('decal', 1, 26, 'Decal'),
    ('grayscaleToPaletteColor', 1, 4, 'GreyscaleToPalette_Color'),
    ('specularEnabled', 1, 0, 'Specular'),
    ('zTest', 1, 31, 'ZBuffer_Test'),
    ('zWrite', 2, 0, 'ZBuffer_Write'),
    ('twoSided', 2, 4, 'Double_Sided'),
    ('glowmap', 2, 6, 'Glow_Map'),
    ('anisoLighting', 2, 21, 'Anisotropic_Lighting'),
    ('skewSpecularAlpha', 2, 22, 'Skew_Specular_Alpha'),
    ('tree', 2, 29, 'Tree_Anim'),
    ('alphaTest', 2, 25, 'Alpha_Test'),
    ('hideSecret', 1, 20, 'Localmap_Hide_Secret'),
    ('hideSecret', 2, 18, 'Hide_On_Local_Map'),
]
FLOATS = ['grayscaleToPaletteScale', 'fresnelPower', 'smoothness', 'specularMult', 'emitMult', 'alpha',
          'subsurfaceRolloff', 'rimPower', 'backLightPower']
# the bar: a field the exporter copies must agree on at least this share of the NIF/BGSM pairs
AGREE_BAR = 0.95
AFTER_RECEIVE = ('castShadows', 'hair', 'skinTint', 'tessellate', 'grayscaleToPaletteScale')


def main(a):
    out_path = a[0]
    roots = [a[1] if len(a) > 1 else DATA] + a[2:]
    t0 = time.time()
    st = TexStack(roots, 'stack')
    lines = []

    def say(s):
        print(s)
        lines.append(s)
        sys.stdout.flush()

    say('bgsm_census%s: %d root(s), %d archive entries' % (' (RED reader %s)' % RED if RED else '', len(roots),
                                                             len(st.arch)))
    names = sorted(k for k in st.arch if k.startswith('materials' + BS) and k.endswith('.bgsm'))
    mats, bad, left, vers = {}, [], {}, {}
    for k in names:
        b = st.read(k)
        try:
            m, rest = read_bgsm(b)
        except (struct.error, UnicodeDecodeError, ValueError) as e:
            bad.append((k, 'ran past the end: %s' % e))
            continue
        vers[m['version']] = vers.get(m['version'], 0) + 1
        left[rest] = left.get(rest, 0) + 1
        if rest != 0:
            bad.append((k, '%d bytes left' % rest))
        mats[k] = m
    exact = left.get(0, 0)
    say('R1 BGSMs: %d listed, versions %s; read to the exact last byte: %d; misread: %d %s' % (
        len(names), dict(sorted(vers.items())), exact, len(bad), bad[:3]))
    fails = []
    if len(bad) != 0 or exact != len(names):
        fails.append('R1 %d of %d BGSMs not read to their last byte' % (len(bad), len(names)))

    # shares of the flags (the "known vanilla values")
    ok = list(mats.values())
    for fld in ('receiveShadows', 'castShadows', 'hideSecret', 'dissolveFade', 'assumeShadowmask', 'twoSided',
                'tessellate', 'hair', 'tree', 'facegen', 'skinTint', 'backLighting', 'rimLighting', 'subsurface',
                'envWindow', 'envEye'):
        n1 = sum(1 for m in ok if m.get(fld))
        say('flag %-18s 1 on %5d of %d (%.1f%%)' % (fld, n1, len(ok), 100.0 * n1 / max(1, len(ok))))
    gs = [m['grayscaleToPaletteScale'] for m in ok] or [float('nan')]
    say('grayscaleToPaletteScale (read after the receive byte): min %.4f max %.4f, in [0,1]: %d of %d' % (
        min(gs), max(gs), sum(1 for x in gs if 0.0 <= x <= 1.0), len(gs)))
    if sum(1 for x in gs if 0.0 <= x <= 1.0) < 0.99 * len(gs):
        fails.append('R1 grayscaleToPaletteScale outside [0,1] on > 1%')

    # R3 path semantics
    def share(fld, kw):
        hit = [k for k, m in mats.items() if m.get(fld)]
        good = [k for k in hit if any(w in k for w in kw)]
        return len(good), len(hit)
    for fld, kw in (('hair', ('hair', 'beard', 'brow', 'lash', 'mustache', 'fur', 'wig')),
                    ('skinTint', ('skin', 'body', 'head', 'hand', 'face', 'actors', 'character')),
                    ('tree', ('tree', 'plant', 'landscape', 'shrub', 'bush', 'veg', 'flora')),
                    ('terrain', ('landscape', 'terrain', 'ground', 'dirt'))):
        g, h = share(fld, kw)
        say('R3 %-9s set on %4d materials, %4d of them under a matching path word %s' % (fld, h, g, kw[:3]))

    # R2 NIF cross-check
    maxnif = int(os.environ.get('MAXNIF', '0') or 0)
    nifs = sorted(k for k in st.arch if k.startswith('meshes' + BS) and k.endswith('.nif'))
    if maxnif:
        nifs = nifs[::max(1, len(nifs) // maxnif)][:maxnif]
    agree = {}
    fagree = {}
    pairs = 0
    nif_bad = 0
    recv_bits = {}	# (word, bit) -> [n with bit among receive 1, among receive 0]
    nrecv = [0, 0]
    bl = {}	# (bgsm flag, bgsm power > 0, nif power > 0) -> count
    for k in nifs:
        b = st.read(k)
        try:
            props = nif_lsp(b)
        except (struct.error, ValueError, IndexError):
            nif_bad += 1
            continue
        for mat, f1, f2, f in props:
            if not mat.lower().endswith('.bgsm'):
                continue
            m = mats.get(mat_key(mat))
            if not m:
                continue
            pairs += 1
            for fld, w, bit, nm in BITS:
                key = (fld, nm)
                got = bool((f1 if w == 1 else f2) >> bit & 1)
                a_ = agree.setdefault(key, [0, 0])
                a_[1] += 1
                a_[0] += got == bool(m.get(fld, 0))
            for fld in FLOATS:
                nv, bv = f.get(fld), m.get(fld)
                if nv is None or bv is None:
                    continue
                a_ = fagree.setdefault(fld, [0, 0])
                a_[1] += 1
                a_[0] += abs(nv - bv) <= 1e-3 * max(1.0, abs(bv))
            r1 = 1 if m['receiveShadows'] else 0
            nrecv[r1] += 1
            for w, word in ((1, f1), (2, f2)):
                for bit in range(32):
                    c = recv_bits.setdefault((w, bit), [0, 0])
                    if word >> bit & 1:
                        c[1 - r1] += 1
            if 'backLighting' in m:
                kk = (m['backLighting'], m['backLightPower'] > 0, (f['backLightPower'] or 0) > 0)
                bl[kk] = bl.get(kk, 0) + 1
    say('R2 NIFs walked %d (unreadable %d), lighting shader properties naming a read BGSM: %d' % (
        len(nifs), nif_bad, pairs))
    for (fld, nm), (g, n) in agree.items():
        say('R2 bit  %-24s vs NIF %-22s agree %6d of %6d (%.2f%%)' % (fld, nm, g, n, 100.0 * g / max(1, n)))
    for fld, (g, n) in fagree.items():
        say('R2 float %-23s vs NIF               agree %6d of %6d (%.2f%%)' % (fld, g, n, 100.0 * g / max(1, n)))
    for fld in AFTER_RECEIVE:
        rec = fagree.get(fld) or next((v for (f_, _n), v in agree.items() if f_ == fld), None)
        if not rec or rec[1] < 100 or rec[0] < AGREE_BAR * rec[1]:
            fails.append('R2 %s (read after receiveShadows) agrees on %s' % (fld, rec))
    # the receive flag against every NIF bit: does any bit follow it?
    best = []
    for (w, bit), (c1, c0) in recv_bits.items():
        p1 = c1 / max(1, nrecv[1])
        p0 = c0 / max(1, nrecv[0])
        best.append((abs(p1 - p0), w, bit, p1, p0))
    best.sort(reverse=True)
    say('R2 receiveShadows over the pairs: 1 on %d, 0 on %d; the NIF bits that differ most between them:' % (
        nrecv[1], nrecv[0]))
    for d, w, bit, p1, p0 in best[:5]:
        say('R2   flags%d bit %2d: set on %.1f%% of receive-1 pairs, %.1f%% of receive-0 pairs' % (w, bit, 100 * p1, 100 * p0))
    say('BL BGSM back-lighting flag / BGSM backlight power > 0 / NIF backlight power > 0 -> pairs: %s' % (
        ', '.join('%d/%d/%d: %d' % (k[0], k[1], k[2], v) for k, v in sorted(bl.items()))))
    n01 = bl.get((0, True, True), 0)
    n0x = n01 + bl.get((0, True, False), 0)
    say('BL VERDICT back-lighting flag as the switch: %s (Bethesda\'s exporter kept the BGSM backlight power in the NIF '
        'on %d of %d pairs whose material says flag 0 with power > 0)' % (
            'REFUTED' if n0x and n01 >= 0.9 * n0x else 'not refuted', n01, n0x))
    blm = {}
    for m in ok:
        if 'backLighting' in m:
            kk = (m['backLighting'], m['backLightPower'] > 0)
            blm[kk] = blm.get(kk, 0) + 1
    say('BL BGSMs by (flag, power > 0): %s' % ', '.join('%d/%d: %d' % (k[0], k[1], v) for k, v in sorted(blm.items())))
    for fl in (0, 1):
        ex = sorted(k for k, m in mats.items() if m.get('backLighting') == fl and m.get('backLightPower', 0) > 0)[:6]
        say('BL   flag %d power > 0 e.g. %s' % (fl, [e[len('materials' + BS):] for e in ex]))

    for f_ in fails:
        say('BGSMRECV FAIL %s' % f_)
    if not fails:
        say('BGSMRECV PASS %d' % (2 + len(AFTER_RECEIVE)))
    say('time %.0f s' % (time.time() - t0))
    with open(out_path, 'w') as fo:
        fo.write('\n'.join(lines) + '\n')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
