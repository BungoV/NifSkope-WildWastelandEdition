"""Lane NEAR1 (2026-09-26): gates G1, G2 and G3 of the NEAR library bake, from an INDEPENDENT reading.

Nothing here links NifSkope or reads a NifSkope intermediate: the plugins are read in Python off the
MO2 profile's load order (lodgen_loadorder_check.expected), the NIFs and BGSMs out of the same stack
(loose files and GNRL BA2s, last entry wins, loose beats archive), the block types' inheritance out
of build/nif.xml. The bake's own files are only the thing being checked.

  G1  every REFR the bake read is a REFR this reader finds in the region (and the other way round);
      the eligible count equals this reader's; every eligible placement (REFR form id, SCOL part
      ordinal) is in the .lodi EXACTLY ONCE and nothing else is; the Initially-Disabled bit of each
      instance equals the REFR's record flag 0x800. Reasons are compared per REFR.
  G2  per drawn shape of every plain model the library holds: triangles, vertices and the model-space
      box equal this reader's decode of the source NIF (BSTriShape vertex data, the shape's own
      transform and its NiNode chain; a BSMeshLODTriShape keeps its first LOD0 Size triangles).
  G3  the census line: eligible + excluded = read, the reasons sum to excluded, and each count equals
      the refs.txt rows.

`--sabotage <kind>` breaks ONE input in memory and must turn its gate red (the refuter):
  drop-instance  forget one eligible .lodi instance          -> G1 red
  dup-instance   list one instance twice                     -> G1 red
  no-marker-rule this reader forgets the STAT marker flag    -> G1 red (count and reasons)
  no-dest-rule   this reader forgets the destructible rule   -> G1 red only where a STAT/SCOL
                 carries DEST (none in the Boston or Sanctuary test regions: it stays green there)
  no-lod0-trim   this reader draws a BSMeshLODTriShape whole -> G2 red
  tris-off       one shapes.txt row's triangle count + 1     -> G2 red
  census-off     the census line's eligible count + 1        -> G3 red

usage: near_library_check.py --lib <dir> --region x0 y0 x1 y1 [--profile P] [--mods M] [--data D]
       [--ws 3C] [--sabotage kind] [--shapes N]   (N = cap on G2 models, default all)
"""
import argparse
import os
import re
import struct
import sys
import zlib
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import lodgen_loadorder_check as lo   # noqa: E402
import lodgen_native_decode as dec     # noqa: E402

# the .lodi wide scale's refusal line (contract: base 8.0 + 65535/8192 = 15.99988)
SCALE_MAX_WIDE = 8.0 + 65535.0 / 8192.0


# ------------------------------------------------------------------------------------------ stack

class Stack:
    """The resource stack: loose folders and GNRL archives; LAST entry wins; loose beats archive."""

    def __init__(self, entries):
        self.loose = list(reversed(entries))       # first hit wins in this order
        self.arch = {}                              # lowercase backslash name -> (archive, rec)
        for e in entries:                           # later entries overwrite earlier
            if not os.path.isdir(e):
                continue
            for n in sorted(os.listdir(e)):
                if n.lower().endswith('.ba2'):
                    self._index(os.path.join(e, n))

    def _index(self, path):
        with open(path, 'rb') as f:
            head = f.read(24)
            magic, _ver, kind, n, nto = struct.unpack('<4sII I Q', head)
            if magic != b'BTDX' or kind.to_bytes(4, 'little') != b'GNRL':
                return
            recs = [struct.unpack('<IIIIQIII', f.read(36)) for _ in range(n)]
            f.seek(nto)
            for r in recs:
                ln = struct.unpack('<H', f.read(2))[0]
                name = f.read(ln).decode('latin-1').lower().replace('/', '\\')
                self.arch[name] = (path, r[4], r[5], r[6])

    def read(self, rel):
        rel = rel.replace('/', '\\').lower()
        for e in self.loose:
            p = os.path.join(e, rel)
            if os.path.isfile(p):
                with open(p, 'rb') as f:
                    b = f.read()
                if b:
                    return b
        hit = self.arch.get(rel)
        if not hit:
            return None
        path, off, packed, unpacked = hit
        with open(path, 'rb') as f:
            f.seek(off)
            b = f.read(packed if packed else unpacked)
        return zlib.decompress(b) if packed else b


# ------------------------------------------------------------------------------------------ ESM

def rec_fields(buf, off, size):
    end = off + size
    big = 0
    while off + 6 <= end:
        t = buf[off:off + 4]
        sz = struct.unpack_from('<H', buf, off + 4)[0]
        off += 6
        if t == b'XXXX':
            big = struct.unpack_from('<I', buf, off)[0]
            off += sz
            continue
        if big:
            sz, big = big, 0
        yield t, buf[off:off + sz]
        off += sz


def rec_payload(buf, off, size, flags):
    if flags & 0x00040000:
        try:
            d = zlib.decompress(buf[off + 4:off + size])
        except Exception:
            return None, 0, 0
        return d, 0, len(d)
    return buf, off, size


class World:
    """Plugins merged the game's way: a record's LAST version wins; a REFR stays in the cell of the
    FIRST file that placed it (an override carries no cell of its own)."""

    def __init__(self, plugins, ws_global):
        self.names = [os.path.basename(p).lower() for p in plugins]
        self.bases = {}          # global form -> (buf, off, size, flags, type, plugin)
        self.cells = {}          # global cell form -> dict(x, y, persistent)
        self.refs = {}           # global ref form -> dict(cell, flags, base, pos, rot, scale)
        self.ws = ws_global
        for pi, p in enumerate(plugins):
            with open(p, 'rb') as f:
                buf = f.read()
            self._plugin(pi, buf)

    def _gmap(self, pi, masters, form):
        fi = form >> 24
        if fi < len(masters):
            try:
                gi = self.names.index(masters[fi].lower())
            except ValueError:
                return 0
        else:
            gi = pi
        return (gi << 24) | (form & 0xFFFFFF)

    def _plugin(self, pi, buf):
        size = struct.unpack_from('<I', buf, 4)[0]
        masters = []
        for t, pl in rec_fields(buf, 24, size):
            if t == b'MAST':
                masters.append(pl.split(b'\0')[0].decode('cp1252'))
        off = 24 + size
        end = len(buf)
        while off + 24 <= end:
            gsize = struct.unpack_from('<I', buf, off + 4)[0]
            label = buf[off + 8:off + 12]
            if label == b'WRLD':
                self._wrld(pi, buf, masters, off + 24, off + gsize)
            elif label != b'CELL':
                self._top(pi, buf, masters, off + 24, off + gsize)
            off += gsize

    def _top(self, pi, buf, masters, off, end):
        while off + 24 <= end:
            t = buf[off:off + 4]
            size = struct.unpack_from('<I', buf, off + 4)[0]
            if t == b'GRUP':
                self._top(pi, buf, masters, off + 24, off + size)
                off += size
                continue
            flags, form = struct.unpack_from('<II', buf, off + 8)
            self.bases[self._gmap(pi, masters, form)] = (buf, off + 24, size, flags, t, pi, masters)
            off += 24 + size

    def _wrld(self, pi, buf, masters, off, end):
        """The WRLD top group: WRLD records, each followed by its world-children group (type 1)."""
        while off + 24 <= end:
            t = buf[off:off + 4]
            size = struct.unpack_from('<I', buf, off + 4)[0]
            if t == b'GRUP':
                gtype = struct.unpack_from('<i', buf, off + 12)[0]
                lab = struct.unpack_from('<I', buf, off + 8)[0]
                if gtype == 1 and self._gmap(pi, masters, lab) == self.ws:
                    self._children(pi, buf, masters, off + 24, off + size, None, True)
                off += size
                continue
            flags, form = struct.unpack_from('<II', buf, off + 8)
            self.bases[self._gmap(pi, masters, form)] = (buf, off + 24, size, flags, t, pi, masters)
            off += 24 + size

    def _children(self, pi, buf, masters, off, end, cell, top):
        while off + 24 <= end:
            t = buf[off:off + 4]
            size = struct.unpack_from('<I', buf, off + 4)[0]
            if t == b'GRUP':
                gtype = struct.unpack_from('<i', buf, off + 12)[0]
                lab = struct.unpack_from('<I', buf, off + 8)[0]
                if gtype == 6:
                    c = self._gmap(pi, masters, lab)
                    self._children(pi, buf, masters, off + 24, off + size, c, False)
                elif gtype in (4, 5):
                    self._children(pi, buf, masters, off + 24, off + size, None, False)
                else:
                    self._children(pi, buf, masters, off + 24, off + size, cell, False)
                off += size
                continue
            flags, form = struct.unpack_from('<II', buf, off + 8)
            g = self._gmap(pi, masters, form)
            if t == b'CELL':
                d, o, s = rec_payload(buf, off + 24, size, flags)
                ent = self.cells.setdefault(g, {'x': None, 'y': None, 'persistent': False})
                if top:
                    ent['persistent'] = True
                if d is not None:
                    for ft, pl in rec_fields(d, o, s):
                        if ft == b'XCLC' and len(pl) >= 8:
                            ent['x'], ent['y'] = struct.unpack_from('<ii', pl, 0)
            elif t == b'REFR':
                d, o, s = rec_payload(buf, off + 24, size, flags)
                r = self.refs.get(g)
                if r is None:
                    r = self.refs[g] = {'cell': cell}
                r.update({'flags': flags, 'base': 0, 'pos': (0.0, 0.0, 0.0), 'rot': (0.0, 0.0, 0.0),
                          'scale': 1.0})
                if d is not None:
                    for ft, pl in rec_fields(d, o, s):
                        if ft == b'NAME' and len(pl) >= 4:
                            r['base'] = self._gmap(pi, masters, struct.unpack_from('<I', pl, 0)[0])
                        elif ft == b'DATA' and len(pl) >= 24:
                            v = struct.unpack_from('<6f', pl, 0)
                            r['pos'], r['rot'] = v[:3], v[3:]
                        elif ft == b'XSCL' and len(pl) >= 4:
                            r['scale'] = struct.unpack_from('<f', pl, 0)[0]
            off += 24 + size                             # LAND, NAVM, ACHR, PGRE...: not bases

    def base(self, form):
        """(type, record flags, fields list) of the winning version, or None."""
        e = self.bases.get(form)
        if not e:
            return None
        buf, off, size, flags, t, pi, masters = e
        d, o, s = rec_payload(buf, off, size, flags)
        fl = list(rec_fields(d, o, s)) if d is not None else []
        return t.decode('latin-1'), flags, fl, pi, masters


# ------------------------------------------------------------------------------------------ NIF

class NifTypes:
    def __init__(self, xml_path):
        self.parent = {}
        for e in ET.parse(xml_path).getroot().iter('niobject'):
            self.parent[e.get('name')] = e.get('inherit')

    def inherits(self, t, base):
        seen = 0
        while t and seen < 64:
            if t == base:
                return True
            t = self.parent.get(t)
            seen += 1
        return False


def nif_header(data):
    # the header walk of tools/rigging_prototype/nifparse.py, on bytes (nifparse reads a path)
    off = data.index(b'\n') + 1
    ver, = struct.unpack_from('<I', data, off); off += 4
    off += 1                                            # endian
    user, nb, bsv = struct.unpack_from('<III', data, off); off += 12
    for _ in range(3):
        off += 1 + data[off]
    if bsv >= 130:
        off += 1 + data[off]
    nt, = struct.unpack_from('<H', data, off); off += 2
    types = []
    for _ in range(nt):
        n, = struct.unpack_from('<I', data, off); off += 4
        types.append(data[off:off + n].decode('latin-1')); off += n
    idx = struct.unpack_from('<%dH' % nb, data, off); off += 2 * nb
    sizes = struct.unpack_from('<%dI' % nb, data, off); off += 4 * nb
    ns, _mx = struct.unpack_from('<II', data, off); off += 8
    strings = []
    for _ in range(ns):
        n, = struct.unpack_from('<I', data, off); off += 4
        strings.append(data[off:off + n].decode('latin-1')); off += n
    ng, = struct.unpack_from('<I', data, off); off += 4 + 4 * ng
    blocks = []
    for i in range(nb):
        blocks.append((types[idx[i] & 0x7FFF], off, sizes[i]))
        off += sizes[i]
    return bsv, strings, blocks


def half(h):
    return struct.unpack('<e', struct.pack('<H', h))[0]


def bgsm_facts(b):
    """(alphaBlend, decal, tree) of a BGSM, or None when it does not read to its tree byte."""
    try:
        if b[:4] != b'BGSM':
            return None
        o = 4
        ver, = struct.unpack_from('<I', b, o); o += 4
        o += 4 + 16 + 4                                   # tile flags, uv offset/scale, alpha
        blend = b[o] != 0; o += 1 + 8                     # bAlphaBlend, src, dst
        o += 1 + 1 + 1 + 1 + 1 + 1                        # test ref, test, zwrite, ztest, ssr, wetness ssr
        decal = b[o] != 0; o += 1
        o += 1 + 1 + 1                                    # two-sided, decal no fade, non-occluder
        o += 1 + 1 + 4                                    # refraction, falloff, power
        o += 1                                            # env mapping
        if ver < 10:
            o += 4
        o += 1                                            # greyscale to palette
        if ver >= 6:
            o += 1
        for _ in range(10 if ver >= 17 else 9):
            n, = struct.unpack_from('<I', b, o); o += 4 + n
        o += 1                                            # editor alpha ref
        if ver >= 8:
            o += 3 + 12 + 8
        else:
            o += 1 + 4 + 4 + 1 + 4
        o += 1 + 12 + 4 + 4 + 4 + 12                      # spec on, colour, mult, smoothness, fresnel, wet x3
        if ver < 10:
            o += 4
        o += 8                                            # wet fresnel, metalness
        if ver > 2:
            o += 1
        if ver >= 9:
            o += 1 + 4
        n, = struct.unpack_from('<I', b, o); o += 4 + n   # root material
        o += 1                                            # aniso
        emit = b[o] != 0; o += 1
        if emit:
            o += 12
        o += 4 + 1 + 1                                    # emit mult, MSN, external emittance
        if ver >= 12:
            o += 4
        if ver >= 13:
            o += 1 + 12
        if ver < 8:
            o += 1
        o += 6                                            # receive/hide/cast, dissolve, shadowmask, glowmap
        if ver < 7:
            o += 2
        o += 1 + 12                                       # hair, tint
        tree = b[o] != 0
        if o >= len(b):
            return None
        return blend, decal, tree
    except (struct.error, IndexError):
        return None


class Models:
    def __init__(self, stack, types, lod0_trim=True):
        self.stack = stack
        self.T = types
        self.cache = {}
        self.bgsm = {}
        self.lod0_trim = lod0_trim

    def material(self, name):
        p = name.replace('\\', '/')
        i = p.lower().rfind('materials/')
        if i > 0:
            p = p[i:]
        elif not p.lower().startswith('materials/'):
            p = 'materials/' + p
        k = p.lower()
        if k not in self.bgsm:
            b = self.stack.read(p)
            self.bgsm[k] = bgsm_facts(b) if b else None
        return self.bgsm[k]

    def judge(self, model):
        """{'state': missing|no-geometry|animated|no-drawable-shape|ok, 'shapes': [...]}"""
        key = model.lower().replace('/', '\\')
        if key in self.cache:
            return self.cache[key]
        rel = key if key.startswith('meshes\\') else 'meshes\\' + key
        data = self.stack.read(rel)
        out = {'state': 'missing', 'shapes': []}
        if data is not None:
            try:
                out = self._parse(data)
            except Exception as e:                       # read but did not parse
                out = {'state': 'no-geometry', 'shapes': [], 'err': str(e)}
        self.cache[key] = out
        return out

    def _parse(self, data):
        T = self.T
        bsv, strings, blocks = nif_header(data)

        def name_of(i):
            if i < 0 or i >= len(blocks):
                return ''
            s = struct.unpack_from('<i', data, blocks[i][1])[0]
            return strings[s] if 0 <= s < len(strings) else ''

        def avobject(o):
            ne, = struct.unpack_from('<I', data, o + 4)
            o += 8 + 4 * ne + 4                          # name, extra list, controller
            o += 4                                       # flags (u32)
            t = struct.unpack_from('<3f', data, o)
            r = struct.unpack_from('<9f', data, o + 12)
            s, = struct.unpack_from('<f', data, o + 48)
            return o + 52 + 4, (t, r, s)                 # + collision ref

        parent, xform = {}, {}
        for i, (tn, st, sz) in enumerate(blocks):
            if T.inherits(tn, 'NiNode'):
                o, x = avobject(st)
                xform[i] = x
                nc, = struct.unpack_from('<I', data, o)
                for k in struct.unpack_from('<%di' % nc, data, o + 4):
                    if 0 <= k < len(blocks) and k not in parent:
                        parent[k] = i
        controllers = sum(1 for tn, _s, _z in blocks
                          if T.inherits(tn, 'NiTimeController') or T.inherits(tn, 'NiSequence'))
        shapes = []
        for i, (tn, st, sz) in enumerate(blocks):
            if not T.inherits(tn, 'BSTriShape'):
                continue
            o, own = avobject(st)
            o += 16 + 4                                  # bounding sphere, skin
            shader, alpha = struct.unpack_from('<ii', data, o); o += 8
            vdesc, = struct.unpack_from('<Q', data, o); o += 8
            ntri, = struct.unpack_from('<I', data, o); o += 4
            nv, = struct.unpack_from('<H', data, o); o += 2
            dsize, = struct.unpack_from('<I', data, o); o += 4
            if not nv:
                continue
            b, marker = i, False
            for _ in range(64):
                if name_of(b).lower().startswith('editormarker'):
                    marker = True
                    break
                if b not in parent:
                    break
                b = parent[b]
            if marker or not dsize:
                continue
            stride = (vdesc & 0xF) * 4
            full = bool((vdesc >> 44) & 0x400)
            vo = o
            to = vo + nv * stride
            tris = struct.unpack_from('<%dH' % (3 * ntri), data, to)
            if not ntri:
                continue
            lod0 = 0
            if T.inherits(tn, 'BSMeshLODTriShape'):
                lod0, = struct.unpack_from('<I', data, to + 6 * ntri)
            # facts
            f = {'block': i, 'name': name_of(i), 'srcTris': ntri, 'lod0': lod0, 'reason': ''}
            flags1 = flags2 = 0
            effect = False
            matname = ''
            if 0 <= shader < len(blocks):
                stn, sst, _ = blocks[shader]
                effect = T.inherits(stn, 'BSEffectShaderProperty')
                so = sst + (4 if stn == 'BSLightingShaderProperty' else 0)   # FO4: Shader Type first
                sidx, = struct.unpack_from('<i', data, so)
                matname = strings[sidx] if 0 <= sidx < len(strings) else ''
                ne, = struct.unpack_from('<I', data, so + 4)
                so += 8 + 4 * ne + 4
                flags1, flags2 = struct.unpack_from('<II', data, so)
            ablend = False
            if 0 <= alpha < len(blocks) and blocks[alpha][0] == 'NiAlphaProperty':
                ast = blocks[alpha][1]
                ne, = struct.unpack_from('<I', data, ast + 4)
                af, = struct.unpack_from('<H', data, ast + 8 + 4 * ne + 4)
                ablend = bool(af & 1)
            decal = bool(flags1 & ((1 << 26) | (1 << 27)))
            tree = bool(flags2 & (1 << 29))
            if matname.lower().endswith('.bgsm'):
                m = self.material(matname)
                if m:
                    ablend = ablend or m[0]
                    decal = decal or m[1]
                    tree = tree or m[2]
            f['reason'] = ('effect' if effect else 'alpha-blend' if ablend else 'decal' if decal
                           else 'tree-anim' if tree else '')
            # geometry: the drawn triangles, the vertices they use, the model-space box
            keep = tris
            if self.lod0_trim and lod0 and lod0 < ntri:
                keep = tris[:3 * lod0]
                used = sorted(set(keep))
            else:
                used = range(nv)
            chain = []
            b = i
            seen = set()
            while b in parent and b not in seen:
                seen.add(b)
                b = parent[b]
                if b in xform:
                    chain.append(xform[b])
            t0, r0, s0 = own
            lo_, hi_ = [3.4e38] * 3, [-3.4e38] * 3
            for v in used:
                if full:
                    p = struct.unpack_from('<3f', data, vo + v * stride)
                else:
                    p = struct.unpack_from('<3e', data, vo + v * stride)
                q = [t0[k] + s0 * (r0[k * 3] * p[0] + r0[k * 3 + 1] * p[1] + r0[k * 3 + 2] * p[2]) for k in range(3)]
                for t, r, s in chain:
                    q = [t[k] + s * (r[k * 3] * q[0] + r[k * 3 + 1] * q[1] + r[k * 3 + 2] * q[2]) for k in range(3)]
                for k in range(3):
                    lo_[k] = min(lo_[k], q[k])
                    hi_[k] = max(hi_[k], q[k])
            f['tris'] = len(keep) // 3
            f['verts'] = len(used)
            f['box'] = lo_ + hi_
            shapes.append(f)
        if not shapes:
            return {'state': 'no-geometry', 'shapes': []}
        if controllers:
            return {'state': 'animated', 'shapes': shapes}
        if not any(not s['reason'] for s in shapes):
            return {'state': 'no-drawable-shape', 'shapes': shapes}
        return {'state': 'ok', 'shapes': shapes}


# ------------------------------------------------------------------------------------------ gates

def expected_refs(W, M, region, sab):
    """[(ref form, reason or '', [(part ordinal, disabled, reason or '')...])] for the region."""
    x0, y0, x1, y1 = region
    out = []
    for form, r in W.refs.items():
        c = W.cells.get(r['cell'])
        if c is None:
            continue
        if c['persistent']:
            px, py = r['pos'][0], r['pos'][1]
            if not (x0 * 4096.0 <= px < (x1 + 1) * 4096.0 and y0 * 4096.0 <= py < (y1 + 1) * 4096.0):
                continue
        elif c['x'] is None or not (x0 <= c['x'] <= x1 and y0 <= c['y'] <= y1):
            continue
        out.append((form, judge_ref(W, M, r, sab)))
    return out


def base_facts(W, form):
    b = W.base(form) if form else None
    if not b:
        return None
    t, flags, fl, pi, masters = b
    dest = any(ft in (b'DEST', b'DSTD') for ft, _ in fl)
    modl = ''
    parts = []
    for ft, pl in fl:
        if ft == b'MODL':
            modl = pl.split(b'\0')[0].decode('cp1252')
        elif ft == b'ONAM' and t == 'SCOL' and len(pl) >= 4:
            parts.append([W._gmap(pi, masters, struct.unpack_from('<I', pl, 0)[0]), []])
        elif ft == b'DATA' and t == 'SCOL' and parts:
            for k in range(len(pl) // 28):
                parts[-1][1].append(struct.unpack_from('<7f', pl, k * 28))
    return {'type': t, 'flags': flags, 'dest': dest, 'modl': modl, 'parts': parts}


def judge_ref(W, M, r, sab):
    dis = bool(r['flags'] & 0x800)
    if r['flags'] & 0x20:
        return ('deleted', [])
    bf = base_facts(W, r['base'])
    if not bf:
        return ('no-base', [])
    if bf['type'] not in ('STAT', 'SCOL'):
        return ('type:' + bf['type'], [])
    if bf['type'] == 'STAT' and bf['flags'] & 0x00800000 and sab != 'no-marker-rule':
        return ('marker', [])
    if bf['dest'] and sab != 'no-dest-rule':
        return ('destructible', [])

    def model_reason(modl, scale):
        st = M.judge(modl)['state']
        if st != 'ok':
            return {'missing': 'model-missing'}.get(st, st)
        if not (0.0 <= scale <= SCALE_MAX_WIDE):
            return 'scale-out-of-range'
        return ''

    if bf['type'] == 'STAT':
        if not bf['modl']:
            return ('no-model', [])
        why = model_reason(bf['modl'], r['scale'])
        return (why, [(-1, dis, why)])
    if not bf['parts']:
        return ('scol-no-parts', [])
    pls = []
    ordinal = 0
    for pbase, placements in bf['parts']:
        pb = base_facts(W, pbase)
        pr = ''
        if not pb:
            pr = 'part-no-base'
        elif pb['type'] != 'STAT':
            pr = 'part-type:' + pb['type']
        elif pb['flags'] & 0x00800000:
            pr = 'part-marker'
        elif pb['dest'] and sab != 'no-dest-rule':
            pr = 'part-destructible'
        elif not pb['modl']:
            pr = 'part-no-model'
        for pl in placements:
            why = pr or model_reason(pb['modl'], r['scale'] * pl[6])
            pls.append((ordinal, dis, why))
            ordinal += 1
    ok = any(not w for _, _, w in pls)
    return ('' if ok else 'scol-no-eligible-part', pls)


def read_refs_txt(path):
    R, P = {}, {}
    for ln in open(path, encoding='utf-8'):
        if ln.startswith('#'):
            continue
        c = ln.rstrip('\n').split('\t')
        if c[0] == 'R':
            R.setdefault(int(c[1], 16), []).append(c[3])
        elif c[0] == 'P':
            P[(int(c[1], 16), int(c[2]))] = c
    return R, P


def census_line(log):
    for ln in open(log, encoding='utf-8', errors='replace'):
        if ln.startswith('near census:'):
            return ln.strip()
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lib', required=True)
    ap.add_argument('--log', help='the bake log (for G3); default <lib>.log')
    ap.add_argument('--region', type=int, nargs=4, required=True)
    ap.add_argument('--profile', default='E:/Projects/Fallout 4 Mods/profiles/Default')
    ap.add_argument('--mods', default='E:/Projects/Fallout 4 Mods/mods')
    ap.add_argument('--data', default='X:/Programs/Steam/steamapps/common/Fallout 4/Data')
    ap.add_argument('--ws', default='3C')
    ap.add_argument('--world', default='Commonwealth')
    ap.add_argument('--sabotage', default='')
    ap.add_argument('--shapes', type=int, default=0)
    a = ap.parse_args()
    sab = a.sabotage
    fails = 0

    plugins, stack, _en, _dis, _stars, _m = lo.expected(a.profile, a.mods, a.data)
    missing = [p for p in plugins if p.startswith('(missing)')]
    if missing:
        print('ERROR plugins missing: %s' % missing)
        return 2
    S = Stack(stack)
    W = World(plugins, int(a.ws, 16))
    M = Models(S, NifTypes(os.path.join(ROOT, 'build', 'nif.xml')), lod0_trim=(sab != 'no-lod0-trim'))
    print('reader: %d plugins, %d stack entries, %d archived files, %d refs under the worldspace, %d cells'
          % (len(plugins), len(stack), len(S.arch), len(W.refs), len(W.cells)))

    stem = os.path.join(a.lib, a.world + '.near')
    R, P = read_refs_txt(stem + '.refs.txt')
    exp = expected_refs(W, M, a.region, sab)

    # ---- G1
    g1 = []
    mine = {f: j for f, j in exp}
    if set(mine) != set(R):
        g1.append('REFR sets differ: %d only here, %d only in the bake (e.g. %s / %s)'
                  % (len(set(mine) - set(R)), len(set(R) - set(mine)),
                     ['%08x' % x for x in sorted(set(mine) - set(R))[:3]],
                     ['%08x' % x for x in sorted(set(R) - set(mine))[:3]]))
    dups = [f for f, v in R.items() if len(v) > 1]
    if dups:
        g1.append('%d REFRs listed more than once in refs.txt' % len(dups))
    elig_here = sum(1 for f, (why, _) in exp if not why)
    elig_bake = sum(1 for f, v in R.items() if v[0] == 'eligible')
    if elig_here != elig_bake:
        g1.append('eligible: %d here, %d in the bake' % (elig_here, elig_bake))
    disagree = {}
    for f, (why, _) in exp:
        if f in R:
            theirs = '' if R[f][0] == 'eligible' else R[f][0]
            if theirs != why:
                k = '%s -> %s' % (why or 'eligible', theirs or 'eligible')
                disagree.setdefault(k, []).append(f)
    for k, v in sorted(disagree.items()):
        g1.append('reason %s: %d (e.g. %s)' % (k, len(v), ' '.join('%08x' % x for x in v[:3])))
    # the .lodi: every eligible placement exactly once, nothing else, the disabled bit
    L = dec.read_lodo(stem + '.lodo')
    T = dec.read_lodi(stem + '.lodi')
    got = {}
    order = list(zip(T['cold'], T['instances']))
    if sab == 'drop-instance' and order:
        order = order[1:]
    if sab == 'dup-instance' and order:
        order = order + order[:1]
    for c, inst in order:
        k = (c['refFormId'], c['scolPart'])
        got.setdefault(k, []).append(inst)
    want = {}
    for f, (why, pls) in exp:
        for part, dis, pw in pls:
            if not pw:
                want[(f, part)] = dis
    if set(got) != set(want):
        g1.append('.lodi placements: %d wanted and absent, %d present and not wanted'
                  % (len(set(want) - set(got)), len(set(got) - set(want))))
    twice = [k for k, v in got.items() if len(v) != 1]
    if twice:
        g1.append('.lodi placements listed more than once: %d' % len(twice))
    badDis = [k for k, v in got.items() if k in want and bool(v[0]['flags'] & 256) != want[k]]
    if badDis:
        g1.append('Initially-Disabled bit wrong on %d instances' % len(badDis))
    print('G1 %s: refs read %d (bake %d), eligible %d (bake %d), placements %d (.lodi %d)%s'
          % ('PASS' if not g1 else 'FAIL', len(mine), len(R), elig_here, elig_bake, len(want), len(order),
             '' if not g1 else '\n  ' + '\n  '.join(g1)))
    fails += bool(g1)

    # ---- G2
    g2 = []
    rows = {}
    for ln in open(stem + '.shapes.txt', encoding='utf-8'):
        if ln.startswith('#'):
            continue
        c = ln.rstrip('\n').split('\t')
        if c[2] != '0' or c[3] == '-':
            continue
        rows.setdefault(c[1], []).append(c)
    if sab == 'tris-off' and rows:
        k = sorted(rows)[0]
        rows[k][0][6] = str(int(rows[k][0][6]) + 1)
    n_models = n_shapes = 0
    worst = 0.0
    for path in sorted(rows):
        if a.shapes and n_models >= a.shapes:
            break
        jm = M.judge(path)
        mine_s = {s['block']: s for s in jm['shapes']}
        whole = 'model-animated' if jm['state'] == 'animated' else 'kept'
        n_models += 1
        for c in rows[path]:
            blk = int(c[3])
            s = mine_s.get(blk)
            n_shapes += 1
            if s is None:
                g2.append('%s block %d: in the bake, not drawn by this reader' % (path, blk))
                continue
            if int(c[6]) != s['tris'] or int(c[5]) != s['verts']:
                g2.append('%s block %d: tris %s verts %s, source %d / %d'
                          % (path, blk, c[6], c[5], s['tris'], s['verts']))
            if c[9] != (s['reason'] or whole):
                g2.append('%s block %d: bake says %s, source says %s' % (path, blk, c[9], s['reason'] or whole))
            box = [float(x) for x in c[8].split()]
            for k in range(6):
                d = abs(box[k] - s['box'][k])
                worst = max(worst, d)
                if d > 1e-3 * max(1.0, abs(s['box'][k])):
                    g2.append('%s block %d: box[%d] %.6g, source %.6g' % (path, blk, k, box[k], s['box'][k]))
                    break
        extra = set(mine_s) - {int(c[3]) for c in rows[path]}
        if extra:
            g2.append('%s: blocks %s drawn by this reader, absent from the bake' % (path, sorted(extra)))
    print('G2 %s: %d models, %d shapes; worst box difference %.3g units%s'
          % ('PASS' if not g2 else 'FAIL', n_models, n_shapes, worst,
             '' if not g2 else '\n  ' + '\n  '.join(g2[:12]) + ('\n  ... %d more' % (len(g2) - 12) if len(g2) > 12 else '')))
    fails += bool(g2)

    # ---- G3
    g3 = []
    line = census_line(a.log or (a.lib.rstrip('/\\') + '.log'))
    if not line:
        g3.append('no census line in the log')
    else:
        m = re.search(r'refs read (\d+), eligible (\d+), excluded (\d+) \((.*)\); sum', line)
        read, elig, excl = int(m.group(1)), int(m.group(2)), int(m.group(3))
        if sab == 'census-off':
            elig += 1
        reasons = {}
        for part in m.group(4).split(', '):
            k, v = part.rsplit(' ', 1)
            reasons[k] = int(v)
        if elig + excl != read:
            g3.append('eligible %d + excluded %d != read %d' % (elig, excl, read))
        if sum(reasons.values()) != excl:
            g3.append('reasons sum %d != excluded %d' % (sum(reasons.values()), excl))
        rows_r = {}
        for f, v in R.items():
            rows_r[v[0]] = rows_r.get(v[0], 0) + 1
        if rows_r.get('eligible', 0) != elig or len(R) != read:
            g3.append('refs.txt: %d rows, %d eligible; census %d / %d' % (len(R), rows_r.get('eligible', 0), read, elig))
        for k, v in reasons.items():
            if rows_r.get(k, 0) != v:
                g3.append('reason %s: census %d, refs.txt %d' % (k, v, rows_r.get(k, 0)))
    print('G3 %s: %s%s' % ('PASS' if not g3 else 'FAIL', line[:120] + '...' if line else '-',
                           '' if not g3 else '\n  ' + '\n  '.join(g3)))
    fails += bool(g3)
    print('RESULT %s (%d gate(s) red)%s' % ('PASS' if not fails else 'FAIL', fails,
                                            ' [sabotage %s]' % sab if sab else ''))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
