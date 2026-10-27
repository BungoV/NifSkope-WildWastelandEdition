#!/usr/bin/env python3
"""Rename and rewrite an FO4CS LOD bake's far texture arrays to the lodm 3 names, in place, without a rebake.

Lane LODMNAME1 (2026-10-27, docs/LODGEN_LODM_FORMAT.md 4.1). Before that day a bake wrote one set per size
PER FAMILY:
    <ws>.LodgenCards.legacy.<WxH>(.ring)   <ws>.LodgenCards.pbr.<WxH>(.ring)      (card arrays)
    <ws>.LodgenArrays.<WxH>                <ws>.LodgenArraysPBR.<WxH>              (mesh texture arrays)
Since then it writes ONE set per size, `<ws>.LodgenCards.<WxH>(.ring)` / `<ws>.LodgenArrays.<WxH>`, lodm 3:
one `_n` sheet holding every layer of both families (legacy first), the colour and mask sheets per family.

This tool takes a pair folder (`<mod>/FO4CSLOD/<ws>`) from the old names to the new ones:
  * Objects/: the .lodm files rewritten to lodm 3, the sheets renamed, a legacy + pbr pair of one size merged
    (the two `_n` arrays concatenated, slice for slice; their layout must agree or the tool refuses);
  * Objects/<ws>.LodgenArrays.txt: version 5 -> 6 (unified layer, the lodm 3 name, familyLayer appended);
  * every chunk manifest: the C and A lines' array name and layer;
  * <ws>.lodo: cardCorpusHash, the base rows' cardLayer (a merged set moves the pbr layers), indexCrc32 and
    headerCrc32; <ws>.lodi: lodoIdentity and headerCrc32;
  * <ws>.lodb: the product rows (names, sha1, order), the out rows of rewritten manifests, `hash cardCorpusHash`
    and the `end` line.

A DRY RUN BY DEFAULT: it prints what it would do. `--apply --backup <dir>` does it, after copying every file it
rewrites, renames or deletes into <dir> (same relative paths). It refuses a folder it does not fully understand:
an envelope other than 1, a payload other than the old one for its name, a .lodo cardCorpusHash that the folder's
files do not reproduce, a reference to an old set it cannot rewrite.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import sys
import zlib

OLD_CARD = re.compile(r'^(?P<ws>.+)\.LodgenCards\.(?P<fam>legacy|pbr)\.(?P<size>\d+x\d+(?:\.ring)?)\.lodm$')
NEW_CARD = re.compile(r'^(?P<ws>.+)\.LodgenCards\.(?P<size>\d+x\d+(?:\.ring)?)\.lodm$')
OLD_PBR_MESH = re.compile(r'^(?P<ws>.+)\.LodgenArraysPBR\.(?P<size>\d+x\d+)\.lodm$')
MESH = re.compile(r'^(?P<ws>.+)\.LodgenArrays\.(?P<size>\d+x\d+)\.lodm$')
PER_LAYER = ('layers', 'emissiveScale', 'sway', 'leafAmplitude', 'leafFrequency')
SIDECAR_V5 = b'# lodgen texture arrays 5:'
SIDECAR_V6 = (b'# lodgen texture arrays 6: family class layer lodm color normal mask emissive source '
              b'emissiveScale familyLayer (docs/LODGEN_IMPOSTOR_SPEC.md)')
NO_CARD = 0xFFFF


class Refusal(Exception):
    pass


def crc32(b, seed=0):
    return zlib.crc32(b, seed) & 0xFFFFFFFF


def fnv1a64(b, h=0xCBF29CE484222325):
    for c in b:
        h = ((h ^ c) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def last(p):
    return re.split(r'[\\/]', p)[-1]


def swap_last(p, name):
    return p[:len(p) - len(last(p))] + name


# ------------------------------------------------------------------ .lodm envelope (LODGEN_LODM_FORMAT.md 1)
def lodm_read(b, what):
    if len(b) < 12 or b[:4] != b'LODM':
        raise Refusal('%s: not a LODM envelope' % what)
    ver, size = struct.unpack_from('<II', b, 4)
    if ver != 1:
        raise Refusal('%s: envelope version %d; this tool knows 1' % (what, ver))
    if 12 + size != len(b):
        raise Refusal('%s: payload size %d but the file holds %d' % (what, size, len(b) - 12))
    return json.loads(b[12:].decode('utf-8'))


def intify(v):
    # QJsonDocument writes an integral double as an integer ("1", never "1.0")
    if isinstance(v, float) and v.is_integer() and abs(v) < 2 ** 53:
        return int(v)
    if isinstance(v, dict):
        return {k: intify(x) for k, x in v.items()}
    if isinstance(v, list):
        return [intify(x) for x in v]
    return v


def lodm_bytes(root):
    p = json.dumps(intify(root), sort_keys=True, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
    return b'LODM' + struct.pack('<II', 1, len(p)) + p


# ------------------------------------------------------------------ DX10 arrays
def dds_split(b, what):
    if len(b) < 148 or b[:4] != b'DDS ' or b[84:88] != b'DX10':
        raise Refusal('%s: not a DX10 DDS' % what)
    n = struct.unpack_from('<I', b, 140)[0]
    if n < 1 or (len(b) - 148) % n:
        raise Refusal('%s: %d layers do not divide the %d payload bytes' % (what, n, len(b) - 148))
    return b[:148], n, (len(b) - 148) // n


def dds_concat(parts, what):
    heads = [dds_split(b, w) for b, w in parts]
    h0, _, per0 = heads[0]
    for (h, _, per), (_, w) in zip(heads[1:], parts[1:]):
        if h[:140] != h0[:140] or h[144:] != h0[144:] or per != per0:
            raise Refusal('%s: %s does not share the layout (format, size, mips) of %s' % (what, w, parts[0][1]))
    total = sum(n for _, n, _ in heads)
    return h0[:140] + struct.pack('<I', total) + h0[144:] + b''.join(b[148:] for b, _ in parts)


# ------------------------------------------------------------------ the folder
class Folder:
    def __init__(self, root):
        self.root = os.path.abspath(root)
        self.objects = os.path.join(self.root, 'Objects')
        if not os.path.isdir(self.objects):
            raise Refusal('%s has no Objects folder: give the pair folder <mod>/FO4CSLOD/<ws>' % self.root)
        self.files = {}      # rel path (forward slashes) -> bytes, loaded on demand
        self.out = {}        # rel path -> new bytes (None = delete)

    def rel(self, p):
        return os.path.relpath(p, self.root).replace(os.sep, '/')

    def read(self, rel):
        if rel in self.out:
            if self.out[rel] is None:
                raise Refusal('%s is read after the tool deleted it' % rel)
            return self.out[rel]
        if rel not in self.files:
            with open(os.path.join(self.root, rel), 'rb') as f:
                self.files[rel] = f.read()
        return self.files[rel]

    def exists(self, rel):
        if rel in self.out:
            return self.out[rel] is not None
        return os.path.isfile(os.path.join(self.root, rel))

    def write(self, rel, b):
        self.out[rel] = b

    def delete(self, rel):
        if rel not in self.out or self.out[rel] is not None:
            self.out[rel] = None

    def listing(self):
        """Every file as it will be after the migration, rel -> size."""
        have = {}
        for dp, dn, fn in os.walk(self.root):
            for f in fn:
                r = self.rel(os.path.join(dp, f))
                have[r] = os.path.getsize(os.path.join(dp, f))
        for r, b in self.out.items():
            if b is None:
                have.pop(r, None)
            else:
                have[r] = len(b)
        return have


def collect_sets(F):
    """The array sets of Objects/, grouped by (kind, size): {'legacy': set, 'pbr': set} of OLD sets, or a lodm 3."""
    groups = {}
    ws_seen = set()
    for f in sorted(os.listdir(F.objects)):
        if not f.endswith('.lodm'):
            continue
        m = OLD_CARD.match(f)
        if m:
            kind, fam, new = 'cardArray', m['fam'], '%s.LodgenCards.%s' % (m['ws'], m['size'])
        elif OLD_PBR_MESH.match(f):
            m = OLD_PBR_MESH.match(f)
            kind, fam, new = 'array', 'pbr', '%s.LodgenArrays.%s' % (m['ws'], m['size'])
        elif NEW_CARD.match(f) or MESH.match(f):
            m = NEW_CARD.match(f) or MESH.match(f)
            kind = 'cardArray' if NEW_CARD.match(f) else 'array'
            fam, new = None, f[:-len('.lodm')]
        else:
            continue
        ws_seen.add(m['ws'])
        rel = 'Objects/' + f
        root = lodm_read(F.read(rel), rel)
        pv, fam_in = root.get('lodm'), root.get('family')
        if root.get('kind') != kind:
            raise Refusal('%s: kind %r, but the name says %s' % (rel, root.get('kind'), kind))
        if pv == 3:
            if fam is not None:
                raise Refusal('%s: a lodm 3 payload under a pre-lodm-3 name' % rel)
            groups.setdefault((kind, new), {})['done'] = rel
            continue
        if fam is None:
            fam = 'legacy'          # <ws>.LodgenArrays.<WxH> before lodm 3 is the legacy mesh set
        allowed = (1, 2) if kind == 'cardArray' else (1,)
        if pv not in allowed:
            raise Refusal('%s: payload lodm %r; this tool migrates %s' % (rel, pv, ' or '.join(map(str, allowed))))
        if fam_in != fam:
            raise Refusal('%s: family %r, but the name says %s' % (rel, fam_in, fam))
        g = groups.setdefault((kind, new), {})
        if fam in g:
            raise Refusal('two %s sets for %s' % (fam, new))
        g[fam] = (rel, root)
    if len(ws_seen) > 1:
        raise Refusal('Objects/ holds the arrays of more than one worldspace: %s' % sorted(ws_seen))
    for key, g in groups.items():
        if 'done' in g and len(g) > 1:
            raise Refusal('%s is already lodm 3 and an old set of the same size is still there' % key[1])
    return groups, (ws_seen.pop() if ws_seen else None)


COLOR = {False: 'diffuse', True: 'baseColor'}
MASK = {False: 'gsaos', True: 'rmaos'}


def sheets_of(root, pbr, rel):
    t = root.get('textures', {})
    out = {'normal': t.get('normal'), 'color': t.get(COLOR[pbr]), 'mask': t.get(MASK[pbr]), 'emissive': t.get('emissive')}
    for k in ('normal', 'color', 'mask'):
        if not out[k]:
            raise Refusal('%s names no %s sheet' % (rel, k))
    return out


def plan_group(F, kind, new, g, log):
    """Build the lodm 3 set `new` from the old legacy/pbr sets of g. Returns {old lodm file: (new lodm file, offset)}."""
    fams = [(f, g[f]) for f in ('legacy', 'pbr') if f in g]
    newbase = 'Objects/' + new
    renames = {}
    sh = {}
    for f, (rel, root) in fams:
        sh[f] = sheets_of(root, f == 'pbr', rel)
        for k, p in sh[f].items():
            if p and not F.exists('Objects/' + last(p)):
                raise Refusal('%s names %s, which is not in Objects/' % (rel, last(p)))
    first_rel, first = fams[0][1]
    oldbase0 = first_rel[:-len('.lodm')]
    sep_dir = sh[fams[0][0]]['normal'][:len(sh[fams[0][0]]['normal']) - len(last(sh[fams[0][0]]['normal']))]
    game = lambda suffix: sep_dir + new + suffix

    def suffix_of(rel, path):
        base = last(rel)[:-len('.lodm')]
        n = last(path)
        if not n.startswith(base):
            raise Refusal('%s names sheet %s, which does not carry its own name' % (rel, n))
        return n[len(base):]

    # the array block: one family's keys, both families' layers
    arr0 = {k: v for k, v in first['array'].items() if k not in PER_LAYER}
    for f, (rel, root) in fams[1:]:
        arr1 = {k: v for k, v in root['array'].items() if k not in PER_LAYER}
        if arr1 != arr0:
            raise Refusal('%s and %s disagree on the array layout and cannot share one `_n`' % (first_rel, rel))
    any_sway = any('sway' in root['array'] for _, (_, root) in fams)
    layers, scales, sway, amp, frq = [], [], [], [], []
    tex = {}
    n_parts = []
    unified = 0
    for f, (rel, root) in fams:
        a = root['array']
        ls = a.get('layers', [])
        if not ls:
            raise Refusal('%s has no layers' % rel)
        es = a.get('emissiveScale', [])
        if len(es) != len(ls):
            raise Refusal('%s: %d emissiveScale values for %d layers' % (rel, len(es), len(ls)))
        renames[last(rel)] = (new + '.lodm', unified)
        for i, L in enumerate(ls):
            o = {'color': L} if isinstance(L, str) else dict(L)
            o['family'] = f
            o['normalLayer'] = unified + i
            o['familyLayer'] = i
            layers.append(o)
        scales += es
        if any_sway:
            if 'sway' in a:
                for k, dst in (('sway', sway), ('leafAmplitude', amp), ('leafFrequency', frq)):
                    if len(a.get(k, [])) != len(ls):
                        raise Refusal('%s: %s does not run parallel to the layers' % (rel, k))
                    dst += a[k]
            else:
                # a family with no model sway: the writer's default for each layer
                sway += ['synthetic'] * len(ls)
                amp += [1] * len(ls)
                frq += [1] * len(ls)
        unified += len(ls)
        s = sh[f]
        pbr = f == 'pbr'
        ft = {}
        for k, key in (('color', COLOR[pbr]), ('mask', MASK[pbr]), ('emissive', 'emissive')):
            if not s[k]:
                continue
            sfx = suffix_of(rel, s[k])
            ft[key] = game(sfx)
            src, dst = 'Objects/' + last(s[k]), newbase + sfx
            F.write(dst, F.read(src))
            if src != dst:
                F.delete(src)
        tex[f] = ft
        n_parts.append(('Objects/' + last(s['normal']), suffix_of(rel, s['normal'])))
    nsfx = n_parts[0][1]
    if any(sfx != nsfx for _, sfx in n_parts):
        raise Refusal('%s: the families name their normal sheets differently' % new)
    tex['normal'] = game(nsfx)
    if len(n_parts) == 1:
        F.write(newbase + nsfx, F.read(n_parts[0][0]))
    else:
        F.write(newbase + nsfx, dds_concat([(F.read(p), p) for p, _ in n_parts], new + nsfx))
    for p, _ in n_parts:
        if p != newbase + nsfx:
            F.delete(p)
    arr = dict(arr0)
    arr['layers'] = layers
    arr['emissiveScale'] = scales
    if any_sway:
        arr['sway'], arr['leafAmplitude'], arr['leafFrequency'] = sway, amp, frq
    root = {'lodm': 3, 'family': 'mixed' if len(fams) == 2 else fams[0][0], 'kind': kind,
            'textures': tex, 'array': arr}
    # keys the old root carried that this tool does not know would be lost: refuse
    for f, (rel, r) in fams:
        extra = set(r) - {'lodm', 'family', 'kind', 'textures', 'array'}
        if extra:
            raise Refusal('%s carries root keys %s this tool does not know' % (rel, sorted(extra)))
    for f, (rel, _) in fams:
        if rel != newbase + '.lodm':
            F.delete(rel)
    F.write(newbase + '.lodm', lodm_bytes(root))
    log.append('  %s <- %s (%d layers%s)' % (new, ' + '.join(last(r) for _, (r, _) in fams), unified,
                                              ', merged' if len(fams) == 2 else ''))
    return renames


# ------------------------------------------------------------------ text: manifests and the sidecar
def rewrite_manifest(rel, b, ren):
    keys = {k.lower(): v for k, v in ren.items()}
    out, n = [], 0
    for raw in b.split(b'\n'):
        cr = raw.endswith(b'\r')
        line = raw[:-1] if cr else raw
        low = line.lower()
        if not any(k.encode() in low for k in keys):
            out.append(raw)
            continue
        s = line.decode('utf-8')
        t = s.split(' ')
        if t[0] == 'C' and len(t) == 12 and last(t[10]).lower() in keys:
            nn, off = keys[last(t[10]).lower()]
            t[10], t[11] = swap_last(t[10], nn), str(int(t[11]) + off)
        elif t[0] == 'A' and len(t) == 4 and last(t[3]).lower() in keys:
            nn, off = keys[last(t[3]).lower()]
            t[3], t[2] = swap_last(t[3], nn), str(int(t[2]) + off)
        else:
            raise Refusal('%s: a line names an old array in a shape this tool does not know: %s' % (rel, s[:160]))
        n += 1
        out.append(' '.join(t).encode('utf-8') + (b'\r' if cr else b''))
    return b'\n'.join(out), n


def rewrite_sidecar(rel, b, ren):
    keys = {k.lower(): v for k, v in ren.items()}
    lines = b.split(b'\n')
    if not lines or not lines[0].startswith(SIDECAR_V5):
        if lines and lines[0].startswith(b'# lodgen texture arrays 6:'):
            return None, 0
        raise Refusal('%s: header %r; this tool migrates version 5' % (rel, lines[0][:40] if lines else b''))
    body = []
    for raw in lines[1:]:
        if not raw.strip():
            continue
        cr = raw.endswith(b'\r')
        t = raw.decode('utf-8').rstrip('\r').split(' ')
        if len(t) < 10:
            raise Refusal('%s: a line with %d columns: %s' % (rel, len(t), raw[:120]))
        k = last(t[3]).lower()
        if k not in keys:
            raise Refusal('%s: a line names %s, which this tool did not migrate' % (rel, last(t[3])))
        nn, off = keys[k]
        fam_rank = 0 if t[0] == 'legacy' else 1
        size = t[1]
        nl = [t[0], t[1], str(int(t[2]) + off), swap_last(t[3], nn)] + t[4:] + [t[2]]
        body.append(((size, fam_rank), ' '.join(nl).encode('utf-8') + (b'\r' if cr else b'')))
    # the writer's order: by size (QMap of the size string), legacy then pbr, by layer
    order = [l for _, l in sorted(body, key=lambda e: e[0])]
    moved = order != [l for _, l in body]
    tail = b'\n' if b.endswith(b'\n') else b''
    head = SIDECAR_V6 + (b'\r' if lines[0].endswith(b'\r') else b'')
    return head + b'\n' + b'\n'.join(order) + tail, (len(order), moved)


# ------------------------------------------------------------------ the card link (nativeemit.cpp 3.)
def card_link(F, manifests):
    """The arrays the C lines name, in set order, and cardCorpusHash over them (lodgenNativeLinkCards)."""
    names = {}
    for rel in manifests:
        for raw in F.read(rel).split(b'\n'):
            if raw.startswith(b'C '):
                t = raw.decode('utf-8').strip().split()
                if len(t) == 12:
                    n = last(t[10])
                    names.setdefault(n.lower().encode('utf-8'), n)
    order = [names[k] for k in sorted(names)]
    h = 0xCBF29CE484222325

    def hash_file(name):
        nonlocal h
        b = F.read('Objects/' + name)
        h = fnv1a64(name.lower().encode('utf-8'), h)
        h = fnv1a64(struct.pack('<Q', len(b)), h)
        h = fnv1a64(b, h)

    layers = []
    for name in order:
        root = lodm_read(F.read('Objects/' + name), name)
        hash_file(name)
        t = root.get('textures', {})
        if root.get('lodm') == 3:
            fam = [k for k in ('legacy', 'pbr') if k in t]
            first = t[fam[0]]
            pbr0 = fam[0] == 'pbr'
            seq = [first.get(COLOR[pbr0]), t.get('normal'), first.get(MASK[pbr0]), first.get('emissive')]
            if root.get('family') == 'mixed':
                p = t['pbr']
                seq += [p.get('baseColor'), p.get('rmaos'), p.get('emissive')]
        else:
            pbr = root.get('family') == 'pbr'
            seq = [t.get(COLOR[pbr]), t.get('normal'), t.get(MASK[pbr]), t.get('emissive')]
        for k, s in enumerate(seq):
            if k in (3, 6) and not s:
                continue
            if not s:
                raise Refusal('%s names no sheet %d' % (name, k))
            hash_file(last(s))
        layers.append(len(root['array']['layers']))
    return order, layers, h


TABS = lambda h: [(h['offBases'], h['baseCount'] * 32), (h['offMeshes'], h['meshCount'] * 56),
                  (h['offClusters'], h['clusterCount'] * 16), (h['offClusterLods'], h['clusterCount'] * 48),
                  (h['offMaterials'], h['materialCount'] * 16), (h['offLocalIndices'], h['clusterCount'] * 48),
                  (h['offVertices'], h['vertexCount'] * 16), (h['offStrings'], h['stringBytes'])] + \
                 ([(h['offColours'], h['colourVertexCount'] * 4)] if h['colourVertexCount'] else []) + \
                 ([(h['offChartStream'], h['clusterCount'] * 16)] if h['offChartStream'] else [])


def lodo_head(b, rel):
    if b[:4] != b'LODO':
        raise Refusal('%s: not a .lodo' % rel)
    v = struct.unpack_from('<I', b, 4)[0]
    if v not in (4, 5, 6, 7, 8):
        raise Refusal('%s: version %d; this tool knows 4 to 8' % (rel, v))
    h = {'version': v}
    h['headerCrc32'] = struct.unpack_from('<I', b, 0x0C)[0]
    (h['objectCorpusHash'], h['modelCorpusHash'], h['cardCorpusHash']) = struct.unpack_from('<QQQ', b, 0x18)
    (h['baseCount'], h['meshCount'], h['clusterCount'], h['materialCount'], h['vertexCount'], _, _, _,
     h['stringBytes']) = struct.unpack_from('<IIIIIIHHI', b, 0x50)
    (h['offBases'], h['offMeshes'], h['offClusters'], h['offMaterials'], h['offLocalIndices'], h['offVertices'],
     h['offStrings']) = struct.unpack_from('<QQQQQQQ', b, 0x70)
    h['indexCrc32'] = struct.unpack_from('<I', b, 0xA8)[0]
    h['offClusterLods'] = struct.unpack_from('<Q', b, 0xC0)[0]
    h['colourVertexCount'], h['offColours'] = struct.unpack_from('<IQ', b, 0xD4) if v >= 5 else (0, 0)
    h['offChartStream'] = struct.unpack_from('<Q', b, 0xE0)[0] if v == 8 else 0
    if crc32(b[0x10:0x100]) != h['headerCrc32']:
        raise Refusal('%s: headerCrc32 does not match its header' % rel)
    crc = 0
    for off, size in TABS(h):
        crc = crc32(b[off:off + size], crc)
    if crc != h['indexCrc32']:
        raise Refusal('%s: indexCrc32 does not match its tables' % rel)
    s = b[h['offStrings']:h['offStrings'] + h['stringBytes']]
    if b'Lodgen' in s:
        raise Refusal('%s: its strings name an array (a material row); this tool does not rewrite those' % rel)
    return h


def lodo_rewrite(b, h, remap, new_hash):
    b = bytearray(b)
    moved = 0
    for i in range(h['baseCount']):
        at = h['offBases'] + i * 32 + 16
        cl = struct.unpack_from('<H', b, at)[0]
        if cl == NO_CARD:
            continue
        if cl not in remap:
            raise Refusal('.lodo base row %d: cardLayer 0x%04x names no layer of the old arrays' % (i, cl))
        if remap[cl] != cl:
            struct.pack_into('<H', b, at, remap[cl])
            moved += 1
    struct.pack_into('<Q', b, 0x28, new_hash)
    crc = 0
    for off, size in TABS(h):
        crc = crc32(bytes(b[off:off + size]), crc)
    struct.pack_into('<I', b, 0xA8, crc)
    struct.pack_into('<I', b, 0x0C, crc32(bytes(b[0x10:0x100])))
    return bytes(b), moved


def lodi_rewrite(b, rel, old_lodo, new_lodo):
    if b[:4] != b'LODI':
        raise Refusal('%s: not a .lodi' % rel)
    v, flags, hc = struct.unpack_from('<III', b, 4)
    hdr = 512 if v >= 7 else 256
    if v < 3 or v > 13:
        raise Refusal('%s: version %d; this tool knows 3 to 13' % (rel, v))
    if crc32(b[0x10:hdr]) != hc:
        raise Refusal('%s: headerCrc32 does not match its header' % rel)
    if flags & 4:
        return b, False         # NOLIB: paired to no .lodo

    def ident(lb):
        return fnv1a64(struct.pack('<IQQ', struct.unpack_from('<I', lb, 0x0C)[0],
                                   struct.unpack_from('<Q', lb, 0x20)[0], struct.unpack_from('<Q', lb, 0x18)[0]))
    if struct.unpack_from('<Q', b, 0x20)[0] != ident(old_lodo):
        raise Refusal('%s: its lodoIdentity does not name the folder\'s .lodo' % rel)
    b = bytearray(b)
    struct.pack_into('<Q', b, 0x20, ident(new_lodo))
    struct.pack_into('<I', b, 0x0C, crc32(bytes(b[0x10:hdr])))
    return bytes(b), True


# ------------------------------------------------------------------ the bake record (.lodb)
def lodb_rewrite(F, rel, changed, new_hash, old_hash):
    lines = F.read(rel).decode('utf-8').split('\n')
    out, products = [], []
    pos = None
    for ln in lines:
        f = ln.split('\t')
        if f[0] == 'product':
            if pos is None:
                pos = len(out)
            continue
        if f[0] == 'out' and len(f) == 5:
            p = os.path.normpath(os.path.join(os.path.dirname(rel), f[3])).replace(os.sep, '/')
            if p in changed:
                f[4] = hashlib.sha1(F.read(p)).hexdigest()
                ln = '\t'.join(f)
        if f[0] == 'hash' and len(f) == 3 and f[1] == 'cardCorpusHash':
            if old_hash is not None and int(f[2], 16) != old_hash:
                raise Refusal('%s: hash cardCorpusHash %s is not the .lodo\'s %016x' % (rel, f[2], old_hash))
            if new_hash is not None:
                f[2] = '%016x' % new_hash
                ln = '\t'.join(f)
        if f[0] == 'end':
            continue
        out.append(ln)
    while out and out[-1] == '':
        out.pop()
    # the products: every file under the record's folder no chunk row claims (lodgenchunkpass.cpp 6b)
    claimed = set()
    for ln in out:
        f = ln.split('\t')
        if f[0] == 'out' and len(f) == 5:
            claimed.add(os.path.normpath(os.path.join(os.path.dirname(rel), f[3])).replace(os.sep, '/').lower())
    claimed.add(rel.lower())
    have = F.listing()
    base = os.path.dirname(rel)
    for r in sorted(have):
        if r.lower() in claimed or (base and not r.startswith(base + '/')):
            continue
        rr = r[len(base) + 1:] if base else r
        products.append('product\t%s\t%s' % (rr, hashlib.sha1(F.read(r)).hexdigest()))
    products.sort(key=lambda s: s.split('\t')[1])
    if pos is None:
        raise Refusal('%s lists no product rows (a record before lane INCR2); this tool does not add them' % rel)
    out[pos:pos] = products
    nf = sum(1 for r in have if r != rel and (not base or r.startswith(base + '/')))
    nb = sum(s for r, s in have.items() if r != rel and (not base or r.startswith(base + '/')))
    out.append('end\t%d\t%d' % (nf, nb))
    return ('\n'.join(out) + '\n').encode('utf-8')


# ------------------------------------------------------------------ main
def migrate(root):
    F = Folder(root)
    log = []
    groups, ws = collect_sets(F)
    todo = {k: g for k, g in groups.items() if 'done' not in g}
    if not todo:
        log.append('nothing to migrate: every array set is lodm 3 already (or there are none)')
        return F, log, False
    manifests = sorted(F.rel(os.path.join(F.root, f)) for f in os.listdir(F.root) if f.endswith('.manifest.txt'))
    lodo = [f for f in os.listdir(F.root) if f.endswith('.lodo')]
    lodi = [f for f in os.listdir(F.root) if f.endswith('.lodi')]
    lodb = [f for f in os.listdir(F.root) if f.endswith('.lodb')]
    if len(lodo) > 1 or len(lodi) > 1 or len(lodb) > 1:
        raise Refusal('more than one .lodo/.lodi/.lodb in %s' % F.root)
    # before anything moves: the old link, recomputed, must be the .lodo's
    old_order, old_layers, old_hash = card_link(F, manifests) if lodo else ([], [], None)
    old_lodo = F.read(lodo[0]) if lodo else None
    h = lodo_head(old_lodo, lodo[0]) if lodo else None
    if lodo and old_order and h['cardCorpusHash'] != old_hash:
        raise Refusal('%s: cardCorpusHash %016x, but the folder\'s card arrays hash to %016x: the files are not '
                      'the ones this .lodo was linked against' % (lodo[0], h['cardCorpusHash'], old_hash))
    if lodo and not old_order and h['cardCorpusHash']:
        raise Refusal('%s carries a cardCorpusHash but no C line names an array' % lodo[0])
    log.append('sets: %d group(s) to migrate in %s' % (len(todo), ws))
    ren = {}
    for (kind, new), g in sorted(todo.items()):
        ren.update(plan_group(F, kind, new, g, log))
    # text
    nlines = 0
    for rel in manifests:
        nb, n = rewrite_manifest(rel, F.read(rel), ren)
        if n:
            F.write(rel, nb)
            nlines += n
    log.append('manifests: %d line(s) rewritten in %d file(s)' % (nlines, sum(1 for r in manifests if r in F.out)))
    side = 'Objects/%s.LodgenArrays.txt' % ws
    if F.exists(side):
        nb, info = rewrite_sidecar(side, F.read(side), ren)
        if nb is not None:
            F.write(side, nb)
            log.append('sidecar: %s version 5 -> 6, %d line(s)%s' % (side, info[0], ', reordered' if info[1] else ''))
    # the pair
    new_hash = None
    if lodo:
        new_order, new_layers, new_hash = card_link(F, manifests)
        if not old_order:
            new_hash = 0
        remap = {}
        newidx = {n.lower(): i for i, n in enumerate(new_order)}
        for s, (name, n) in enumerate(zip(old_order, old_layers)):
            nn, off = ren.get(name, (name, 0))
            if nn.lower() not in newidx:
                k = [v for k2, v in ren.items() if k2.lower() == name.lower()]
                nn, off = k[0] if k else (name, 0)
            ns = newidx[nn.lower()]
            for li in range(n):
                remap[(s << 11) | li] = (ns << 11) | (li + off)
        nlo, moved = lodo_rewrite(old_lodo, h, remap, new_hash)
        F.write(lodo[0], nlo)
        log.append('%s: cardCorpusHash %016x -> %016x; %d set(s) -> %d; %d base row(s) moved layer'
                   % (lodo[0], h['cardCorpusHash'], new_hash, len(old_order), len(new_order), moved))
        lodo_head(nlo, lodo[0] + ' (rewritten)')
        if lodi:
            nli, paired = lodi_rewrite(F.read(lodi[0]), lodi[0], old_lodo, nlo)
            if paired:
                F.write(lodi[0], nli)
            log.append('%s: %s' % (lodi[0], 'lodoIdentity re-derived' if paired else 'NOLIB, left as it is'))
        # a file this tool does not rewrite that still names the old link would be orphaned: refuse instead
        ident = fnv1a64(struct.pack('<IQQ', h['headerCrc32'], h['modelCorpusHash'], h['objectCorpusHash']))
        marks = [('lodoIdentity', ident)] + ([('cardCorpusHash', h['cardCorpusHash'])] if h['cardCorpusHash'] != new_hash else [])
        scanned = 0
        for r in sorted(F.listing()):
            if r in F.out or r.lower().endswith(('.dds', '.lodb')):
                continue
            b = F.read(r)
            scanned += 1
            for what, v in marks:
                if struct.pack('<Q', v) in b or ('%016x' % v).encode() in b.lower():
                    raise Refusal('%s names the old %s %016x and this tool does not rewrite it' % (r, what, v))
            F.files.pop(r, None)
        log.append('scan: %d other file(s) name neither the old lodoIdentity nor the old cardCorpusHash' % scanned)
    if lodb:
        changed = {r for r, b in F.out.items() if b is not None}
        F.write(lodb[0], lodb_rewrite(F, lodb[0], changed, new_hash, h['cardCorpusHash'] if h else None))
        log.append('%s: products, out rows, cardCorpusHash and end rewritten' % lodb[0])
    return F, log, True


def apply(F, backup):
    touched = sorted(F.out)
    os.makedirs(backup, exist_ok=True)
    for r in touched:
        src = os.path.join(F.root, r)
        if os.path.isfile(src):
            dst = os.path.join(backup, r)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
            if os.path.getsize(dst) != os.path.getsize(src):
                raise Refusal('backup of %s is short' % r)
    for r in touched:
        if F.out[r] is not None:
            p = os.path.join(F.root, r)
            tmp = p + '.lodmname1.tmp'
            with open(tmp, 'wb') as f:
                f.write(F.out[r])
            os.replace(tmp, p)
    for r in touched:
        if F.out[r] is None:
            os.remove(os.path.join(F.root, r))
    return touched


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('folder', help='the pair folder, <mod>/FO4CSLOD/<ws>')
    ap.add_argument('--apply', action='store_true', help='write the changes (default: a dry run)')
    ap.add_argument('--backup', help='with --apply: copy every file the tool touches here first (required)')
    a = ap.parse_args()
    try:
        F, log, work = migrate(a.folder)
        print('\n'.join(log))
        w = sorted(r for r, b in F.out.items() if b is not None)
        d = sorted(r for r, b in F.out.items() if b is None)
        print('files: %d written, %d deleted' % (len(w), len(d)))
        if not work:
            return 0
        if not a.apply:
            print('DRY RUN: nothing written (--apply --backup <dir> writes it)')
            return 0
        if not a.backup:
            raise Refusal('--apply needs --backup <dir>')
        if os.path.abspath(a.backup).startswith(F.root + os.sep):
            raise Refusal('the backup folder may not be inside the folder it backs up')
        t = apply(F, a.backup)
        print('APPLIED: %d file(s) touched, backups in %s' % (len(t), a.backup))
        return 0
    except Refusal as e:
        print('REFUSED: %s' % e)
        return 2


if __name__ == '__main__':
    sys.exit(main())
