#!/usr/bin/env python
"""Does anything in Fallout 4's LOD tree emit?  (EMIS1, 2026-09-06)

Offline and self-contained: it reads Fallout4.esm, the unpacked meshes and the
BGSMs directly, so it can answer without the game, the Creation Kit or a built
NifSkope.  It exists because the LOD material's `emissiveScale`
(docs/LODGEN_IMPOSTOR_SPEC.md) is 0 for every vanilla surface, and a number that
is always zero has to be shown to be a MEASUREMENT rather than a stuck field.

Three questions:

  A. every LOD model an MNAM slot names, over the whole plugin: its
     BSLightingShaderProperty fields - Own-Emit (Shader Flags 1 bit 22), the
     Emissive Color and the Emissive Multiple - and whether any of them carries
     a BSEffectShaderProperty or a filled glow slot
  B. every MATERIAL those models name, read field for field against
     src/io/materialfile.cpp: Own-Emit, the emittance colour and multiple
  C. the bases whose editor ID matches a pattern, with their LOD slot count and
     their near model's emitters - the "what lights the stadium" question

USAGE
  python tools/lod_emission_probe.py [--esm PATH] [--data PATH] [--cache FILE]
                                     [--window X0 X1 Y0 Y1] [--match REGEX]

  Diamond City: --window -6 2 -10 2 --match "^DiamondStadiumLight0[123]$"
  (--window restricts A and B to bases placed in those Commonwealth cells plus
  the DiamondCity and DiamondCityFX worldspaces entire; without it, A and B ask
  the whole plugin.)

Two independent readings are compared wherever both exist: a NIF shader
property's own emissive fields and the BGSM it names.  A hand-written reader of
someone else's binary format is not a measurement until a second reading agrees
(docs/MISTAKES.md, 2026-09-06).
"""
import argparse
import math
import os
import pickle
import re
import struct
import sys
import zlib

BS = chr(92)
DEFAULT_ESM = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'
DEFAULT_DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'

COMMONWEALTH, DIAMONDCITY, DIAMONDCITYFX = 0x3C, 0xF94, 0xF93
DC_WINDOW = (-6, 2, -10, 2)          # the Commonwealth cells the stadium occupies, widened
OWN_EMIT = 1 << 22
REC_HDR = 24
COMPRESSED = 0x00040000
LOD_BEARING = (b'STAT', b'SCOL', b'MSTT', b'FURN', b'DOOR', b'ACTI', b'LIGH',
               b'TREE', b'FLOR', b'CONT', b'ALCH', b'MISC')


# ====================================================================== the plugin
def subrecords(data):
    off, big = 0, 0
    n = len(data)
    while off + 6 <= n:
        typ = data[off:off + 4]
        size = struct.unpack_from('<H', data, off + 4)[0]
        off += 6
        if typ == b'XXXX':
            big = struct.unpack_from('<I', data, off)[0]
            off += size
            continue
        if big:
            size, big = big, 0
        yield typ, data[off:off + size]
        off += size


def recordData(buf, off, size, flags):
    raw = buf[off:off + size]
    if flags & COMPRESSED:
        if len(raw) < 4:
            return b''
        try:
            return zlib.decompress(raw[4:])
        except zlib.error:
            return b''
    return raw


def zstr(b):
    return b.split(b'\0', 1)[0].decode('latin1')


def walk(buf):
    stack = []

    def rec(off, limit):
        while off + REC_HDR <= limit:
            typ = buf[off:off + 4]
            size = struct.unpack_from('<I', buf, off + 4)[0]
            if typ == b'GRUP':
                label = buf[off + 8:off + 12]
                gtype = struct.unpack_from('<i', buf, off + 12)[0]
                stack.append((label, gtype))
                yield from rec(off + REC_HDR, off + size)
                stack.pop()
                off += size
                continue
            flags = struct.unpack_from('<I', buf, off + 8)[0]
            form = struct.unpack_from('<I', buf, off + 12)[0]
            yield typ, form, flags, off + REC_HDR, size, tuple(stack)
            off += REC_HDR + size

    yield from rec(0, len(buf))


def readPlugin(esm):
    """(worldOf, cellWorld, cellXY, baseOf, refs) - baseOf[form] = (type, edid, MODL, [MNAM])"""
    buf = open(esm, 'rb').read()
    worldOf, cellWorld, cellXY, baseOf = {}, {}, {}, {}
    refs = []
    want = {COMMONWEALTH, DIAMONDCITY, DIAMONDCITYFX}
    for typ, form, flags, doff, dsize, stack in walk(buf):
        if typ == b'WRLD':
            for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                if st == b'EDID':
                    worldOf[form] = zstr(payload)
            continue
        if typ == b'CELL':
            w = None
            for label, gtype in reversed(stack):
                if gtype == 1:
                    w = struct.unpack_from('<I', label, 0)[0]
                    break
            cellWorld[form] = w
            for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                if st == b'XCLC' and len(payload) >= 8:
                    cellXY[form] = struct.unpack_from('<ii', payload, 0)
            continue
        if typ == b'REFR':
            c = None
            for label, gtype in reversed(stack):
                if gtype in (6, 8, 9, 10):
                    c = struct.unpack_from('<I', label, 0)[0]
                    if gtype == 6:
                        break
            if c is None or cellWorld.get(c) not in want:
                continue
            for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                if st == b'NAME' and len(payload) >= 4:
                    refs.append((struct.unpack_from('<I', payload, 0)[0], cellWorld[c], c))
                    break
            continue
        if typ in LOD_BEARING:
            edid, modl, mnam = '', '', []
            for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                if st == b'EDID':
                    edid = zstr(payload)
                elif st == b'MODL' and not modl and b'.' in payload:
                    modl = zstr(payload)
                elif st == b'MNAM':
                    # four fixed 260-byte LOD slots in Fallout 4
                    for k in range(0, len(payload), 260):
                        s = zstr(payload[k:k + 260])
                        if s:
                            mnam.append(s)
            baseOf[form] = (typ.decode(), edid, modl, mnam)
    return worldOf, cellWorld, cellXY, baseOf, refs


# ====================================================================== the NIF
def nifHeader(path):
    data = open(path, 'rb').read()
    off = data.index(b'\n') + 1

    def u32():
        nonlocal off
        v = struct.unpack_from('<I', data, off)[0]
        off += 4
        return v

    def u16():
        nonlocal off
        v = struct.unpack_from('<H', data, off)[0]
        off += 2
        return v

    def u8():
        nonlocal off
        v = data[off]
        off += 1
        return v

    def sstr():
        nonlocal off
        n = u32()
        s = data[off:off + n]
        off += n
        return s.decode('latin1')

    def shortstr():
        nonlocal off
        n = u8()
        s = data[off:off + n]
        off += n
        return s.decode('latin1')

    u32(); u8(); u32()
    numBlocks = u32()
    bs = u32()
    shortstr(); shortstr(); shortstr()
    if bs >= 130:
        shortstr()
    types = [sstr() for _ in range(u16())]
    idx = [u16() for _ in range(numBlocks)]
    sizes = [u32() for _ in range(numBlocks)]
    nstr = u32(); u32()
    strings = [sstr() for _ in range(nstr)]
    for _ in range(u32()):
        u32()
    blocks = []
    for i in range(numBlocks):
        blocks.append((types[idx[i]], off, sizes[i]))
        off += sizes[i]
    return data, strings, blocks, bs


def lightingShaders(path):
    """[(name, flags1, (r,g,b), mult)] for every BSLightingShaderProperty that passes
    the layout check, plus the number that did not.

    Fallout 4 layout (nif.xml, BSVER 130):
        Shader Type u32 | Name u32 | Num Extra u32 | Extra i32[n] | Controller i32
        | Shader Flags 1 u32 | Shader Flags 2 u32 | UV Offset 2f | UV Scale 2f
        | Texture Set i32 | Emissive Color 3f | Emissive Multiple f
    The Texture Set reference must name a real BSShaderTextureSet, the name index
    must be inside the string table and the floats must be finite and sane, or the
    block is counted as unread rather than believed.
    """
    data, strings, blocks, bs = nifHeader(path)
    out, bad = [], 0
    for tname, start, size in blocks:
        if tname != 'BSLightingShaderProperty':
            continue
        try:
            nameIdx = struct.unpack_from('<I', data, start + 4)[0]
            nExtra = struct.unpack_from('<I', data, start + 8)[0]
            if nExtra > 64:
                bad += 1
                continue
            o = start + 12 + 4 * nExtra + 4
            f1 = struct.unpack_from('<I', data, o)[0]
            o += 8 + 16
            tset = struct.unpack_from('<i', data, o)[0]
            o += 4
            r, g, b, mult = struct.unpack_from('<ffff', data, o)
            okSet = tset == -1 or (0 <= tset < len(blocks) and blocks[tset][0] == 'BSShaderTextureSet')
            okName = 0 <= nameIdx < len(strings) or nameIdx == 0xFFFFFFFF
            okNums = all(math.isfinite(v) and -1.0 <= v <= 100.0 for v in (r, g, b, mult))
            if not (okSet and okName and okNums and o + 16 <= start + size):
                bad += 1
                continue
            out.append((strings[nameIdx] if 0 <= nameIdx < len(strings) else '', f1, (r, g, b), mult))
        except Exception:
            bad += 1
    return out, bad, blocks, data


def textureSets(data, blocks):
    out = []
    for tname, start, size in blocks:
        if tname != 'BSShaderTextureSet':
            continue
        off = start
        n = struct.unpack_from('<I', data, off)[0]
        off += 4
        slots = []
        for _ in range(n):
            ln = struct.unpack_from('<I', data, off)[0]
            off += 4
            slots.append(data[off:off + ln].split(b'\0', 1)[0].decode('latin1'))
            off += ln
        out.append(slots)
    return out


# ====================================================================== the BGSM
class _R:
    def __init__(self, b):
        self.b = b
        self.o = 0

    def u8(self):
        v = self.b[self.o]
        self.o += 1
        return v

    def u32(self):
        v = struct.unpack_from('<I', self.b, self.o)[0]
        self.o += 4
        return v

    def f32(self):
        v = struct.unpack_from('<f', self.b, self.o)[0]
        self.o += 4
        return v

    def s(self):
        n = self.u32()
        v = self.b[self.o:self.o + n].split(b'\0', 1)[0].decode('latin1')
        self.o += n
        return v


def readBgsm(path):
    """Field for field against Material::readFile and ShaderMaterial::readFile.

    Two fields were missed on the first attempt and both shifted everything after
    them: `iAlphaTestRef` (one byte, after the alpha blend modes) and
    `sRootMaterialPath` (a length-prefixed string, right before `bAnisoLighting`).
    `consumed`/`size` are reported so a shifted read shows itself.
    """
    b = open(path, 'rb').read()
    if b[:4] != b'BGSM':
        return None
    r = _R(b[4:])
    v = r.u32()
    r.u32()
    r.f32(); r.f32(); r.f32(); r.f32()
    alpha = r.f32()
    r.u8(); r.u32(); r.u32()
    r.u8()                                   # iAlphaTestRef
    r.u8(); r.u8(); r.u8()
    r.u8(); r.u8()
    r.u8(); r.u8(); r.u8(); r.u8()
    r.u8(); r.u8(); r.f32()
    r.u8()
    if v < 10:
        r.f32()
    r.u8()
    if v >= 6:
        r.u8()
    tex = [r.s() for _ in range(10 if v >= 17 else 9)]
    r.u8()
    if v >= 8:
        r.u8(); r.u8(); r.u8()
        r.f32(); r.f32(); r.f32()
        r.f32(); r.f32()
    else:
        r.u8(); r.f32(); r.f32(); r.u8(); r.f32()
    specEnabled = r.u8()
    r.f32(); r.f32(); r.f32()
    specMult = r.f32()
    smoothness = r.f32()
    r.f32()
    r.f32(); r.f32(); r.f32()
    if v < 10:
        r.f32()
    r.f32(); r.f32()
    if v > 2:
        r.u8()
    if v >= 9:
        r.u8(); r.f32()
    root = r.s()                             # sRootMaterialPath
    r.u8()
    emit = r.u8()
    colour = (0.0, 0.0, 0.0)
    if emit:
        colour = (r.f32(), r.f32(), r.f32())
    mult = r.f32()
    r.u8()
    external = r.u8()
    if r.o > len(r.b):
        return None
    return dict(version=v, textures=tex, emitEnabled=bool(emit), emittanceColor=colour,
                emittanceMult=mult, externalEmittance=bool(external), alpha=alpha,
                specEnabled=bool(specEnabled), specMult=specMult, smoothness=smoothness,
                rootMaterial=root, consumed=r.o, size=len(r.b))


def normalise(name):
    """LOD shaders often carry an absolute Bethesda build path; key on `materials\\...`."""
    n = name.lower().replace('/', BS)
    i = n.find('materials' + BS)
    return n[i:] if i >= 0 else n


# ====================================================================== the report
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--esm', default=DEFAULT_ESM)
    ap.add_argument('--data', default=DEFAULT_DATA)
    ap.add_argument('--cache', default=None)
    ap.add_argument('--match', default=r'^(DiamondStadiumLight|DiamondCityStadiumLights)')
    ap.add_argument('--window', nargs=4, type=int, metavar=('X0', 'X1', 'Y0', 'Y1'),
                    help='restrict A to bases placed in these Commonwealth cells,'
                         ' plus the DiamondCity and DiamondCityFX worldspaces entire'
                         ' (Diamond City: -6 2 -10 2)')
    args = ap.parse_args()
    if not os.path.exists(args.esm):
        print('no ESM at %s' % args.esm)
        return 2

    if args.cache and os.path.exists(args.cache):
        with open(args.cache, 'rb') as f:
            worldOf, cellWorld, cellXY, baseOf, refs = pickle.load(f)
        print('cache: %s' % args.cache)
    else:
        worldOf, cellWorld, cellXY, baseOf, refs = readPlugin(args.esm)
        if args.cache:
            with open(args.cache, 'wb') as f:
                pickle.dump((worldOf, cellWorld, cellXY, baseOf, refs), f, 2)
    print('%s: %d bases, %d placements in Commonwealth/DiamondCity/DiamondCityFX'
          % (os.path.basename(args.esm), len(baseOf), len(refs)))

    def mesh(m):
        return os.path.join(args.data, 'meshes', m.replace(BS, '/'))

    def mat(rel):
        p = os.path.join(args.data, rel.replace(BS, '/'))
        return p if os.path.exists(p) else None

    # ---- A
    inWindow = None
    if args.window:
        x0, x1, y0, y1 = args.window
        inWindow = set()
        for base, w, c in refs:
            if w in (DIAMONDCITY, DIAMONDCITYFX):
                inWindow.add(base)
                continue
            xy = cellXY.get(c)
            if w == COMMONWEALTH and xy and x0 <= xy[0] <= x1 and y0 <= xy[1] <= y1:
                inWindow.add(base)
    models = {}
    for b, v in baseOf.items():
        if inWindow is not None and b not in inWindow:
            continue
        for m in v[3]:
            models.setdefault(m.lower().replace('/', BS), set()).add(v[1])
    if inWindow is None:
        print('\nA. LOD models named by a base: %d' % len(models))
    else:
        print('\nA. bases placed in the window: %d; LOD models they name: %d'
              % (len(inWindow), len(models)))
    read = missing = shapes = ownEmit = litNif = effect = glow = 0
    mats = {}
    for m in sorted(models):
        p = mesh(m)
        if not os.path.exists(p):
            missing += 1
            continue
        try:
            props, bad, blocks, data = lightingShaders(p)
        except Exception:
            missing += 1
            continue
        read += 1
        if any(t == 'BSEffectShaderProperty' for t, _, _ in blocks):
            effect += 1
        if any(len(s) > 2 and s[2] for s in textureSets(data, blocks)):
            glow += 1
        for nm, f1, col, mult in props:
            shapes += 1
            if f1 & OWN_EMIT:
                ownEmit += 1
                if max(col) > 0.0:
                    litNif += 1
                    print('   NIF EMITS %s name=%r colour=%s mult=%.3f' % (m, nm, col, mult))
            low = nm.lower()
            if low.endswith('.bgsm') or low.endswith('.bgem'):
                mats.setdefault(normalise(nm), set()).add(m)
    print('   read %d, missing %d; shader blocks %d; Own-Emit %d (%.1f%%);'
          ' Own-Emit with a lit NIF colour %d; models with a BSEffectShaderProperty %d;'
          ' models with a filled glow slot %d'
          % (read, missing, shapes, ownEmit, 100.0 * ownEmit / max(1, shapes), litNif, effect, glow))

    # ---- B
    print('\nB. materials those LOD models name: %d' % len(mats))
    hit = miss = emitMat = 0
    for k in sorted(mats):
        p = mat(k)
        if p is None:
            miss += 1
            continue
        try:
            g = readBgsm(p)
        except Exception:
            g = None
        if g is None:
            miss += 1
            continue
        hit += 1
        if g['emitEnabled'] and max(g['emittanceColor']) > 0.0:
            emitMat += 1
            print('   MAT EMITS %-56s v%d colour %s mult %.3f glow %r'
                  % (k[-56:], g['version'], tuple(round(c, 3) for c in g['emittanceColor']),
                     g['emittanceMult'], g['textures'][2]))
    print('   read %d, unreadable/missing %d; OWN-EMIT with a colour that is not black: %d'
          % (hit, miss, emitMat))

    # ---- C
    print('\nC. bases matching %r' % args.match)
    pat = re.compile(args.match, re.I)
    count = {}
    seen = {}
    for base, w, c in refs:
        count[base] = count.get(base, 0) + 1
        seen.setdefault(base, set()).add((worldOf.get(w, hex(w)), cellXY.get(c)))
    for b, v in sorted(baseOf.items()):
        rt, edid, modl, mnam = v
        if not pat.search(edid):
            continue
        where = sorted({w for w, xy in seen.get(b, ())})
        cells = sorted({xy for w, xy in seen.get(b, ()) if xy})
        print('   %08X %-4s %-32s placed x%-3d LODslots=%d  %s' % (b, rt, edid[:32], count.get(b, 0), len(mnam), modl or '-'))
        print('        worldspaces=%s cells=%s' % (','.join(where) or '-', cells))
        if not modl or not os.path.exists(mesh(modl)):
            continue
        props, bad, blocks, data = lightingShaders(mesh(modl))
        for nm, f1, col, mult in props:
            if not (f1 & OWN_EMIT) or max(col) <= 0.0:
                continue
            line = '        EMITS name=%r NIF colour %s mult %.3f' % (nm, tuple(round(c, 3) for c in col), mult)
            q = mat(normalise(nm)) if nm.lower().endswith('.bgsm') else None
            if q:
                try:
                    g = readBgsm(q)
                except Exception:
                    g = None
                if g:
                    line += '  |  BGSM emit=%s colour %s mult %.3f glow %r (%d of %d bytes read)' % (
                        g['emitEnabled'], tuple(round(c, 3) for c in g['emittanceColor']),
                        g['emittanceMult'], g['textures'][2], g['consumed'], g['size'])
            print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main())
