"""Lane PLACED1 -- the cell view's PLACED ACTORS, checked against the plugin and the meshes themselves.

Nothing here reads NifSkope's code or its record reader. The plugin is walked with the standard library
(cell_lit_check's walk), the meshes are read with tests/spells/gltf_nifread.py, and every actor the cell
places is resolved and posed again from the published record layouts (the xEdit definitions):

  LOOKS    the actor record the Traits come from: follow the template (the per-category template, else the
           default one) while the record's template flags carry bit 0. A leveled list on the way is a dice
           roll -- refused -- unless it has exactly one entry, from level 1, with no chance of nothing.
  RACE     skeleton by sex (male when the female one does not exist), skin, height by sex, face-mesh flag.
  SKIN     the looks record's own, else the race's. Its addons made for the race (or its armor race) are
           drawn, each with the model for the sex; one whose body slots meet a slot the outfit wears is
           HIDDEN. No addon with a model = refused (a robot built from parts).
  OUTFIT   the default outfit of the record the Inventory comes from (template bit 8). A leveled item is
           taken when it is not a dice roll (use-all, or one entry), else left out.
  HEAD     meshes/actors/character/facegendata/facegeom/<plugin>/<looks form>.nif when the race has the flag
           and the file exists, unless slot 32 is worn. Shapes named after a meatcap head part are left out,
           hair when slot 30 or 31 is worn, facial hair when slot 48 is.
  POSE     every vertex = sum of weight * (bone's rest transform in the SKELETON, by name) * (the part's
           skin-to-bone transform) * vertex; a bone the skeleton does not name keeps the part's own node.
  PLACE    world = position + R * (vertex * scale), R = euler(-rx, -ry, -rz),
           scale = XSCL * race height for the sex * the middle of the actor's own height range.
  NOT SHOWN  deleted, no base, or initially disabled (an enable parent with the opposite flag inverts it).

Stages (the gate names the ones each camera carries):
  K  the census line's counts against the walk: read, drawn, not shown, refused by reason
  F  every placed actor: same fate; when drawn the same looks record, race, sex, skeleton, position,
     rotation and scale
  P  every drawn actor: the same models drawn, the same hidden, the same face mesh
  G  every drawn actor: the bounds of its posed vertices within 0.1 unit of the dump's
  N  nothing moves on screen outside the walk's posed triangles (against the actor-less shot)
  C  the actors show inside them

Usage:
  python cell_actor_check.py <Fallout4.esm> <data root> <cell EDID> --walk [out.txt]
  python cell_actor_check.py <Fallout4.esm> <data root> <cell EDID> <run dir> <look-at x,y,z> [stages]
The run dir holds off.png (WW_CELL_ACTOR_RED=none), on.png, on.notes, cam.txt and on.actors.txt.
"""

import math
import os
import re
import struct
import sys
import zlib

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_lit_check import euler, walk  # noqa: E402  (the plugin walk only)
from gltf_nifread import Nif  # noqa: E402

TYPES = (b'NPC_', b'LVLN', b'LVLI', b'RACE', b'ARMO', b'ARMA', b'OTFT', b'HDPT')
GTOL = 0.1
CSHARE = 0.25
REASONS = {'leveled': 'leveled list (a dice roll)', 'notactor': 'base is no actor record',
           'norace': 'no race record', 'noskeleton': 'skeleton file does not load',
           'nobody': 'no body model (built from parts)', 'nogeometry': 'no geometry in its models',
           'redoff': 'switched off by the red control'}


def z(b):
    return b.split(b'\0')[0].decode('cp1252', 'replace')


def u32(b, o=0):
    return struct.unpack_from('<I', b, o)[0]


class Plugin:
    def __init__(self, path, cell_edid):
        self.buf = buf = open(path, 'rb').read()
        self.name = os.path.basename(path)
        self.at, self.refs, cell = {}, [], None
        for t, form, off, stack in walk(buf):
            if t in TYPES:
                self.at[form] = (t, off)
            elif t == b'ACHR':
                if cell is not None and any(g[1] == cell and g[2] in (6, 8, 9) for g in stack):
                    self.refs.append((form, off))
            elif t == b'CELL' and cell is None and all(g[2] != 1 for g in stack):
                if z(dict(self.fields(off)).get(b'EDID', b'')) == cell_edid:
                    cell = form
        if cell is None:
            raise SystemExit('no interior named %s' % cell_edid)
        self.head_part = {}
        for form, (t, off) in self.at.items():
            if t == b'HDPT':
                f = dict(self.fields(off)[::-1])
                if b'EDID' in f and b'PNAM' in f:
                    self.head_part[z(f[b'EDID']).lower()] = u32(f[b'PNAM'])
        self._c = {}

    def fields(self, off):
        """One record's fields in file order, repeats kept."""
        size, flags = struct.unpack_from('<II', self.buf, off + 4)
        d = self.buf[off + 24:off + 24 + size]
        if flags & 0x00040000:
            d = zlib.decompress(d[4:])
        out, o, big = [], 0, 0
        while o + 6 <= len(d):
            name = d[o:o + 4]
            n, = struct.unpack_from('<H', d, o + 4)
            if name == b'XXXX':
                big, = struct.unpack_from('<I', d, o + 6)
                o += 6 + n
                continue
            if big:
                n, big = big, 0
            out.append((name, d[o + 6:o + 6 + n]))
            o += 6 + n
        return out

    def kind(self, form):
        return self.at[form][0] if form in self.at else None

    def rec(self, form, kind):
        """The fields of a record of that type, or None."""
        if self.kind(form) != kind:
            return None
        k = (kind, form)
        if k not in self._c:
            self._c[k] = self.fields(self.at[form][1])
        return self._c[k]

    def npc(self, form):
        fl = self.rec(form, b'NPC_')
        if fl is None:
            return None
        f = dict(fl[::-1])
        a = f.get(b'ACBS', b'\0' * 20)
        mn = struct.unpack('<f', f[b'NAM6'][:4])[0] if b'NAM6' in f else 1.0
        mx = struct.unpack('<f', f[b'NAM4'][:4])[0] if b'NAM4' in f else mn
        return dict(female=bool(u32(a) & 1), tflags=struct.unpack_from('<H', a, 14)[0],
                    tplt=u32(f[b'TPLT']) if b'TPLT' in f else 0,
                    tpta=struct.unpack_from('<13I', f[b'TPTA']) if len(f.get(b'TPTA', b'')) >= 52 else (0,) * 13,
                    race=u32(f[b'RNAM']) if b'RNAM' in f else 0, skin=u32(f[b'WNAM']) if b'WNAM' in f else 0,
                    outfit=u32(f[b'DOFT']) if b'DOFT' in f else 0, height=0.5 * (mn + mx))

    def leveled(self, form, kind, use_all_counts):
        """(entries, is it a dice roll)"""
        fl = self.rec(form, kind)
        chance, use_all, entries = False, False, []
        for n, p in fl:
            if n == b'LVLD' and p:
                chance |= p[0] != 0
            elif n == b'LVLG':
                chance = True
            elif n == b'LVLF' and p:
                use_all = bool(p[0] & 4)
            elif n == b'LVLO' and len(p) >= 12:
                chance |= struct.unpack_from('<H', p)[0] > 1 or p[10] != 0
                entries.append(u32(p, 4))
        dice = chance or not entries or not (len(entries) == 1 or (use_all_counts and use_all))
        return entries, dice

    def owner(self, form, bit):
        """The actor record owning that template category, or 'leveled' / 'notactor'."""
        for _ in range(12):
            k = self.kind(form)
            if k == b'LVLN':
                entries, dice = self.leveled(form, b'LVLN', False)
                if dice:
                    return 'leveled'
                form = entries[0]
                continue
            if k != b'NPC_':
                return 'notactor'
            n = self.npc(form)
            if not n['tflags'] & (1 << bit):
                return form
            nxt = n['tpta'][bit] or n['tplt']
            if not nxt:
                return form
            form = nxt
        return 'notactor'

    def race(self, form):
        fl = self.rec(form, b'RACE')
        if fl is None:
            return None
        version = struct.unpack_from('<H', self.buf, self.at[form][1] + 20)[0]
        out = dict(edid='', skel=[], skin=0, armor_race=0, height=(1.0, 1.0), face=False)
        for n, p in fl:
            if n == b'EDID':
                out['edid'] = z(p)
            elif n == b'WNAM' and len(p) >= 4 and not out['skin']:
                out['skin'] = u32(p)
            elif n == b'DATA' and len(p) >= 12:
                out['height'] = struct.unpack_from('<2f', p)
                at = 32 if version >= 109 else 8
                if len(p) >= at + 4:
                    out['face'] = bool(u32(p, at) & 2)
            elif n == b'ANAM' and len(out['skel']) < 2:
                out['skel'].append(z(p))
            elif n == b'RNAM' and len(p) == 4:
                out['armor_race'] = u32(p)
        out['skel'] += [''] * (2 - len(out['skel']))
        return out

    def armor(self, form, races, sex):
        """(slots, [(addon slots, model)]) for the addons made for the race."""
        fl = self.rec(form, b'ARMO')
        if fl is None:
            return 0, []
        slots, pieces = 0, []
        for n, p in fl:
            if n == b'BOD2' and len(p) >= 4:
                slots = u32(p)
            elif n == b'MODL' and len(p) == 4:
                al = self.rec(u32(p), b'ARMA')
                if al is None:
                    continue
                a = dict(slots=0, race=0, more=[], model=['', ''])
                for an, ap in al:
                    if an == b'BOD2' and len(ap) >= 4:
                        a['slots'] = u32(ap)
                    elif an == b'RNAM' and len(ap) >= 4:
                        a['race'] = u32(ap)
                    elif an == b'MOD2':
                        a['model'][0] = z(ap)
                    elif an == b'MOD3':
                        a['model'][1] = z(ap)
                    elif an == b'MODL' and len(ap) == 4:
                        a['more'].append(u32(ap))
                if a['race'] in races or races & set(a['more']):
                    model = a['model'][sex] or a['model'][0]
                    if model:
                        pieces.append((a['slots'], model))
        return slots, pieces

    def outfit_item(self, form, armors, depth=0):
        """Appends the armors an outfit entry stands for; returns the dice rolls left out."""
        k = self.kind(form)
        if k == b'ARMO':
            armors.append(form)
            return 0
        if k == b'LVLI':
            entries, dice = self.leveled(form, b'LVLI', True)
            if dice or depth >= 6:
                return 1
            return sum(self.outfit_item(e, armors, depth + 1) for e in entries)
        return 0


def mesh_file(data, model):
    m = model.replace('\\', '/')
    if not m.lower().startswith('meshes/'):
        m = 'meshes/' + m
    return os.path.join(data, m)


def resolve(pl, data, flags, f):
    """One placed actor -> a dict with 'fate' and, when drawn, what is drawn."""
    out = dict(fate='hidden', dead=bool(flags & 0x200))
    if flags & 0x20 or b'NAME' not in f or b'DATA' not in f or not u32(f[b'NAME']):
        return out
    off = bool(flags & 0x800)
    if b'XESP' in f and len(f[b'XESP']) >= 8 and u32(f[b'XESP']) and u32(f[b'XESP'], 4) & 1:
        off = not off
    if off:
        return out
    base = u32(f[b'NAME'])
    looks = pl.owner(base, 0)
    if isinstance(looks, str):
        out['fate'] = looks
        return out
    n = pl.npc(looks)
    race = pl.race(n['race'])
    if race is None:
        out['fate'] = 'norace'
        return out
    sex = 1 if n['female'] else 0
    skel = race['skel'][sex]
    if not skel or not os.path.isfile(mesh_file(data, skel)):
        skel = race['skel'][0]
    if not skel or not os.path.isfile(mesh_file(data, skel)):
        out['fate'] = 'noskeleton'
        return out
    races = {n['race'], race['armor_race']} - {0}
    worn, outfit_pieces, outfit = 0, [], 'whole'
    inv = pl.owner(base, 8)
    if isinstance(inv, str):
        outfit = 'unknown'
    else:
        ol = pl.rec(pl.npc(inv)['outfit'], b'OTFT')
        armors, dice = [], 0
        for name, p in ol or []:
            if name == b'INAM':
                for k in range(len(p) // 4):
                    dice += pl.outfit_item(u32(p, 4 * k), armors)
        if dice:
            outfit = 'short'
        for a in armors:
            slots, pieces = pl.armor(a, races, sex)
            worn |= slots
            outfit_pieces += pieces
    _, skin = pl.armor(n['skin'] or race['skin'], races, sex)
    if not skin:
        out['fate'] = 'nobody'
        return out
    parts = [m for s, m in skin if not s & worn]
    hidden = [m for s, m in skin if s & worn]
    for _, m in outfit_pieces:
        if m.lower() not in [p.lower() for p in parts]:
            parts.append(m)
    face = ''
    if race['face'] and not worn & 4:
        cand = 'actors\\character\\facegendata\\facegeom\\%s\\%08X.nif' % (pl.name, looks & 0xFFFFFF)
        if os.path.isfile(mesh_file(data, cand)):
            face = cand
    pos = np.array(struct.unpack_from('<3f', f[b'DATA']), dtype=np.float64)
    rot = struct.unpack_from('<3f', f[b'DATA'], 12)
    xscl = struct.unpack('<f', f[b'XSCL'][:4])[0] if b'XSCL' in f else 1.0
    out.update(fate='drawn', looks=looks, race=race['edid'], sex='F' if sex else 'M', skeleton=skel, parts=parts,
               hidden=hidden, face=face, outfit=outfit, worn=worn, pos=pos, rot=rot,
               scale=xscl * race['height'][sex] * n['height'], headless=race['face'] and not worn & 4 and not face)
    return out


# ---- the pose ---------------------------------------------------------------------------------------------

def node_world(n, i):
    R, t, s = np.eye(3), np.zeros(3), 1.0
    chain = []
    while i is not None:
        chain.append(n.nodes[i])
        i = n.nodes[i]['parent']
    for nd in reversed(chain):
        t = R @ np.array(nd['t']) * s + t
        R = R @ np.array(nd['r']).reshape(3, 3)
        s = s * nd['s']
    return R, t, s


_rest, _part = {}, {}


def skeleton_rest(path):
    if path not in _rest:
        sk = Nif(path)
        rest = {}
        for i, nd in sk.nodes.items():
            rest.setdefault(nd['name'], node_world(sk, i))
        _rest[path] = rest
    return _rest[path]


def posed_part(data, skel, model):
    """[(shape name, Nx3 actor-space vertices, Mx3 triangles)] of one part on the skeleton's rest pose."""
    key = (skel.lower(), model.lower())
    if key in _part:
        return _part[key]
    rest = skeleton_rest(mesh_file(data, skel))
    out = []
    path = mesh_file(data, model)
    pt = Nif(path) if os.path.isfile(path) else None
    for _, sh in sorted(pt.shapes.items()) if pt else []:
        if not sh['verts'] or not sh['tris']:
            continue
        v = np.array(sh['verts'], dtype=np.float64).reshape(-1, 3)
        bones = pt.skin_bones(sh)
        if bones and sh['weights']:
            sk = pt.skins[sh['skin']]
            MR, Mt = [], []
            for k, (name, bd) in enumerate(bones):
                R, t, s = rest[name] if name in rest else node_world(pt, sk['bones'][k])
                MR.append(R @ np.array(bd['r']).reshape(3, 3) * (s * bd['s']))
                Mt.append(R @ np.array(bd['t']) * s + t)
            MR, Mt = np.array(MR), np.array(Mt)
            w = np.array(sh['weights'], dtype=np.float64)
            bi = np.minimum(np.array(sh['boneIndices'], dtype=np.int64), len(MR) - 1)
            res = np.zeros_like(v)
            for k in range(4):
                res += w[:, k:k + 1] * (np.einsum('nij,nj->ni', MR[bi[:, k]], v) + Mt[bi[:, k]])
            tot = w.sum(axis=1)
            v = np.where(tot[:, None] > 1e-6, res / np.maximum(tot, 1e-6)[:, None], v)
        else:
            # rigid: under its parent chain; the nearest parent the skeleton names takes the skeleton's place
            R, t, s = np.eye(3), np.zeros(3), 1.0
            chain, i = [], sh['parent']
            while i is not None:
                if pt.nodes[i]['name'] in rest:
                    R, t, s = rest[pt.nodes[i]['name']]
                    break
                chain.append(pt.nodes[i])
                i = pt.nodes[i]['parent']
            for nd in reversed(chain + [sh]) if False else list(reversed(chain)) + [sh]:
                t = R @ np.array(nd['t']) * s + t
                R = R @ np.array(nd['r']).reshape(3, 3)
                s = s * nd['s']
            v = (R @ v.T).T * s + t
        out.append((sh['name'], v, np.array(sh['tris'], dtype=np.int64).reshape(-1, 3)))
    _part[key] = out
    return out


def posed_actor(pl, data, a):
    """(vertices Nx3, triangles Mx3) in actor space."""
    verts, tris, base = [], [], 0
    worn = a['worn']
    for model, is_face in [(m, False) for m in a['parts']] + ([(a['face'], True)] if a['face'] else []):
        for name, v, t in posed_part(data, a['skeleton'], model):
            if is_face:
                kind = pl.head_part.get(name.lower(), -1)
                if kind == 7 or (kind == 3 and worn & 3) or (kind == 4 and worn & (1 << 18)):
                    continue
            verts.append(v)
            tris.append(t + base)
            base += len(v)
    if not verts:
        return np.zeros((0, 3)), np.zeros((0, 3), dtype=np.int64)
    return np.concatenate(verts), np.concatenate(tris)


def walk_cell(esm, data, cell):
    pl = Plugin(esm, cell)
    actors = []
    for form, off in pl.refs:
        flags = struct.unpack_from('<I', pl.buf, off + 8)[0]
        f = dict(pl.fields(off)[::-1])
        a = resolve(pl, data, flags, f)
        a['form'] = form
        if a['fate'] == 'drawn':
            v, t = posed_actor(pl, data, a)
            if not len(t):
                a['fate'] = 'nogeometry'
            else:
                a['verts'], a['tris'] = v, t
        actors.append(a)
    return pl, actors


def clip_near(poly, near):
    out = []
    for i in range(len(poly)):
        a, b = poly[i], poly[(i + 1) % len(poly)]
        if a[2] >= near:
            out.append(a)
        if (a[2] >= near) != (b[2] >= near):
            out.append(a + (b - a) * ((near - a[2]) / (b[2] - a[2])))
    return out


def main():
    esm, data, cell = sys.argv[1:4]
    pl, actors = walk_cell(esm, data, cell)
    count = {}
    for a in actors:
        count[a['fate']] = count.get(a['fate'], 0) + 1
    drawn = [a for a in actors if a['fate'] == 'drawn']
    refused = {k: v for k, v in count.items() if k not in ('drawn', 'hidden')}
    print('walk: %d placed actors; drawn %d (%d dead on start, %d with a hidden skin part, %d headless), not shown '
          '%d, refused %d %s' % (len(actors), len(drawn), sum(a['dead'] for a in drawn),
                                 sum(bool(a['hidden']) for a in drawn), sum(a['headless'] for a in drawn),
                                 count.get('hidden', 0), sum(refused.values()), dict(sorted(refused.items()))))
    if sys.argv[4] == '--walk':
        fh = open(sys.argv[5], 'w') if len(sys.argv) > 5 else None
        for a in actors:
            line = '%08x %s' % (a['form'], a['fate'])
            if a['fate'] == 'drawn':
                lo, hi = a['verts'].min(axis=0), a['verts'].max(axis=0)
                line += ' %s %s%s pos %.0f,%.0f,%.0f scale %.3f z %.1f..%.1f parts %d hidden %d face %s outfit %s' % (
                    a['race'], a['sex'], ' DEAD' if a['dead'] else '', a['pos'][0], a['pos'][1], a['pos'][2],
                    a['scale'], lo[2], hi[2], len(a['parts']), len(a['hidden']), 'yes' if a['face'] else 'no',
                    a['outfit'])
            if fh:
                fh.write(line + '\n')
        return 0
    from PIL import Image, ImageDraw
    run, at = sys.argv[4:6]
    stages = (sys.argv[6] if len(sys.argv) > 6 and sys.argv[6] else 'KFPGNC').upper()
    word = lambda st, good: ('PASS' if good else 'FAIL') if st in stages else 'n/a '
    ok = True

    # ---- K: the census against the walk
    notes = open(f'{run}/on.notes', errors='replace').read()
    m = re.search(r'placed actors: (\d+) read, drawn (\d+) in the skeleton\'s bind pose \((\d+) triangles; (\d+) dead '
                  r'on start[^;]*; (\d+) without a head[^;]*; (\d+) in their skin and (\d+) short of outfit pieces'
                  r'[^)]*\), not shown (\d+) \([^)]*\), refused (\d+)([^\n\[]*)(\[RED CONTROL (\w+)\])?', notes)
    if not m:
        print('K FAIL  no "placed actors" line in the census')
        k_ok = False
    else:
        read, c_drawn, _tris, c_dead, c_headless, _skin, _short, c_hidden, c_refused = (int(m.group(k))
                                                                                     for k in range(1, 10))
        why = {a.strip(): int(b) for a, b in re.findall(r'([a-z][a-z ()]+?) (\d+)(?:,|\s*$)', m.group(10).lstrip(': '))}
        want = {REASONS[k]: v for k, v in refused.items()}
        k_ok = (read == len(actors) and c_drawn == len(drawn) and c_hidden == count.get('hidden', 0)
                and c_refused == sum(refused.values()) and why == want and c_drawn > 0
                and c_dead == sum(a['dead'] for a in drawn) and c_headless == sum(a['headless'] for a in drawn)
                and c_drawn + c_hidden + c_refused == read and not m.group(11))
        print(f"K {word('K', k_ok)}  census: read {read}, drawn {c_drawn} ({c_dead} dead on start, {c_headless} "
              f"headless), not shown {c_hidden}, refused {c_refused} {why}"
              f"{', RED CONTROL ' + m.group(12) if m.group(11) else ''}; the walk: read {len(actors)}, drawn "
              f"{len(drawn)} ({sum(a['dead'] for a in drawn)} dead on start, {sum(a['headless'] for a in drawn)} "
              f"headless), not shown {count.get('hidden', 0)}, refused {sum(refused.values())} {want}")
    ok &= k_ok or 'K' not in stages

    # ---- F, P, G: actor by actor against the dump
    theirs = {}
    dump = f'{run}/on.actors.txt'
    if os.path.isfile(dump):
        for line in open(dump, errors='replace'):
            c = line.rstrip('\n').split('\t')
            if line.startswith('#') or len(c) < 18:
                continue
            lst = lambda s: [] if s == '-' else [x.lower() for x in s.split(';')]
            theirs[int(c[0], 16)] = dict(
                fate=c[2], dead=c[3] == '1', looks=int(c[4], 16), race=c[5], sex=c[6], scale=float(c[7]),
                pos=np.array([float(v) for v in c[8].split()]), rot=[float(v) for v in c[9].split()],
                tris=int(c[10]), lo=np.array([float(v) for v in c[11].split()]),
                hi=np.array([float(v) for v in c[12].split()]), skeleton=c[13].lower(), parts=lst(c[14]),
                hidden=lst(c[15]), face='' if c[16] == '-' else c[16].lower(), outfit=c[17])
    f_bad, p_bad, g_bad, g_done, hides = [], [], [], 0, 0
    for a in actors:
        t = theirs.get(a['form'])
        if t is None:
            f_bad.append('%08x not in the dump' % a['form'])
            continue
        if t['fate'] != a['fate']:
            f_bad.append('%08x %s, the walk says %s' % (a['form'], t['fate'], a['fate']))
            continue
        if a['fate'] != 'drawn':
            continue
        if (t['looks'] != a['looks'] or t['race'] != a['race'] or t['sex'] != a['sex']
                or t['skeleton'] != a['skeleton'].lower() or t['dead'] != a['dead']
                or np.abs(t['pos'] - a['pos']).max() > 0.01 or np.abs(np.array(t['rot']) - a['rot']).max() > 1e-4
                or abs(t['scale'] - a['scale']) > 1e-4):
            f_bad.append('%08x placed or resolved differently (scale %.4f vs %.4f, %s vs %s)' % (
                a['form'], t['scale'], a['scale'], t['race'], a['race']))
        hides += bool(a['hidden'])
        if (sorted(t['parts']) != sorted(p.lower() for p in a['parts'])
                or sorted(t['hidden']) != sorted(p.lower() for p in a['hidden'])
                or t['face'] != a['face'].lower() or t['outfit'] != a['outfit']):
            p_bad.append('%08x parts %d/%d hidden %d/%d face %s/%s outfit %s/%s' % (
                a['form'], len(t['parts']), len(a['parts']), len(t['hidden']), len(a['hidden']),
                bool(t['face']), bool(a['face']), t['outfit'], a['outfit']))
        lo, hi = a['verts'].min(axis=0), a['verts'].max(axis=0)
        g_done += 1
        worst = max(np.abs(lo - t['lo']).max(), np.abs(hi - t['hi']).max())
        if worst > GTOL or t['tris'] != len(a['tris']):
            g_bad.append('%08x bounds off by %.2f, triangles %d vs %d' % (a['form'], worst, t['tris'], len(a['tris'])))
    extra = sorted(set(theirs) - {a['form'] for a in actors})
    f_ok = not f_bad and not extra and len(theirs) > 0
    print(f"F {word('F', f_ok)}  {len(actors) - len(f_bad)} of {len(actors)} placed actors have the walk's fate, looks "
          f"record, race, sex, skeleton, position, rotation and scale; in the dump alone {len(extra)}")
    for line in f_bad[:8]:
        print('     ' + line)
    ok &= f_ok or 'F' not in stages
    p_ok = not p_bad and g_done > 0
    print(f"P {word('P', p_ok)}  {g_done - len(p_bad)} of {g_done} drawn actors draw the walk's models, hide the walk's "
          f"skin parts ({hides} actors hide one) and carry the walk's face mesh")
    for line in p_bad[:8]:
        print('     ' + line)
    ok &= p_ok or 'P' not in stages
    g_ok = not g_bad and g_done > 0
    print(f"G {word('G', g_ok)}  {g_done - len(g_bad)} of {g_done} drawn actors: posed bounds within {GTOL} unit of "
          f"the walk's and the same triangle count")
    for line in g_bad[:8]:
        print('     ' + line)
    ok &= g_ok or 'G' not in stages

    # ---- N and C: the pixels
    off = np.asarray(Image.open(f'{run}/off.png').convert('RGB')).astype(np.int32)
    on = np.asarray(Image.open(f'{run}/on.png').convert('RGB')).astype(np.int32)
    H, W = on.shape[:2]
    cam = np.array([float(v) for v in re.search(r'cam=([-\d.]+),([-\d.]+),([-\d.]+)',
                                                 open(f'{run}/cam.txt').read()).groups()])
    look = np.array([float(v) for v in at.split(',')])
    fwd = look - cam
    fwd /= np.linalg.norm(fwd)
    right = np.cross(fwd, [0.0, 0.0, 1.0])
    right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    focal = (H / 2.0) / math.tan(math.radians(35.0))
    view = np.stack([right, up, fwd])
    mask_img = Image.new('L', (W, H), 0)
    draw = ImageDraw.Draw(mask_img)
    on_screen = 0
    for a in drawn:
        R = euler(-a['rot'][0], -a['rot'][1], -a['rot'][2])
        wv = a['pos'] + (a['verts'] * a['scale']) @ R.T
        cv = (wv - cam) @ view.T
        if cv[:, 2].max() < 2.0:
            continue
        seen = False
        front = cv[:, 2] >= 2.0
        sx = W / 2.0 + focal * cv[:, 0] / np.where(front, cv[:, 2], 1.0)
        sy = H / 2.0 - focal * cv[:, 1] / np.where(front, cv[:, 2], 1.0)
        for tri in a['tris']:
            if front[tri].all():
                xs, ys = sx[tri], sy[tri]
                if xs.max() < 0 or xs.min() >= W or ys.max() < 0 or ys.min() >= H:
                    continue
                pts = list(zip(np.clip(xs, -4.0 * W, 5.0 * W), np.clip(ys, -4.0 * H, 5.0 * H)))
            else:
                poly = clip_near([cv[k] for k in tri], 2.0)
                if len(poly) < 3:
                    continue
                pts = [(min(max(W / 2.0 + focal * p[0] / p[2], -4.0 * W), 5.0 * W),
                        min(max(H / 2.0 - focal * p[1] / p[2], -4.0 * H), 5.0 * H)) for p in poly]
                if max(x for x, _ in pts) < 0 or min(x for x, _ in pts) >= W or \
                        max(y for _, y in pts) < 0 or min(y for _, y in pts) >= H:
                    continue
            draw.polygon(pts, fill=255, outline=255)
            seen = True
        on_screen += seen
    inside = np.asarray(mask_img) > 0
    grown = inside.copy()
    for _ in range(3):
        g = grown.copy()
        g[1:] |= grown[:-1]
        g[:-1] |= grown[1:]
        g[:, 1:] |= grown[:, :-1]
        g[:, :-1] |= grown[:, 1:]
        grown = g
    diff = np.abs(on - off).max(axis=2)
    outside = ~grown
    n_out, n_in = int(outside.sum()), int(inside.sum())
    print(f"actors on screen {on_screen} of {len(drawn)}; {n_in} pixels inside their posed triangles, {n_out} outside")
    if n_out < 1000:
        n_ok = False
        print(f"N {word('N', False)}  only {n_out} pixels outside the actors: nothing to judge")
    else:
        still = float((diff[outside] <= 3).mean())
        n_ok = still >= 0.995
        print(f"N {word('N', n_ok)}  {100.0 * still:.3f}% of {n_out} pixels outside the actors equal the actor-less "
              f"shot (>= 99.5%)")
    ok &= n_ok or 'N' not in stages
    moved_in = int((diff[inside] > 3).sum()) if n_in else 0
    share = moved_in / n_in if n_in else 0.0
    c_ok = n_in >= 1000 and share >= CSHARE
    print(f"C {word('C', c_ok)}  {moved_in} of {n_in} pixels inside the actors changed by more than 3/255 "
          f"({100.0 * share:.1f}%, >= {100.0 * CSHARE:.0f}%); {int((diff > 3).sum())} changed in the whole frame")
    ok &= c_ok or 'C' not in stages
    print('actor PASS' if ok else 'actor FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
