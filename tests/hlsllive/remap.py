"""HLSLBULK1 round 3: one TEXCOORD number per varying NAME across every VS/HS/DS/GS -> PS pair.
  python -I remap.py <tree> plan     compute components + global name->index map, write ../locmap.json, report
  python -I remap.py <tree> apply    rewrite the interface structs of the files in locmap.json (plus the shim macro)
convert.py imports apply_text() so a regenerated file gets the same numbers.
Why: glslang auto-maps varying locations per stage, so VS output and PS input of one program disagree
(DXIL matches by semantic name+index; SPIR-V by Location, which DXC gives by member DECLARATION order unless
[[vk::location(n)]] is written). The member line gets WW_LOC(n) (shim macro: [[vk::location(n)]] under __spirv__)."""
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LOCMAP = os.path.join(HERE, 'locmap.json')
SUFFIX = {'vert': 'vs', 'tesc': 'hs', 'tese': 'ds', 'geom': 'gs', 'frag': 'ps'}
ORDER = ['vert', 'tesc', 'tese', 'geom', 'frag']
PROTECT = ('post/', 'rtddgi/', 'bakert/')   # VKFRAME1 / VKCOMPUTE1 files: never rewritten
EXEMPT_IN_ONLY = {'wwPointCoord'}           # written by the point-to-quad geometry shader twin
MEMB = re.compile(r'^(\s*)(?:WW_LOC\(\d+\) )?((?:(?:nointerpolation|noperspective|centroid|linear|sample|precise) )*)(\w+) (\w+)(\[(\d+)\])?(\s*:\s*)([A-Za-z_]+?)(\d*)(\s*;.*)$', re.S)
OUT_NAMES = ('SPIRV_Cross_Output', 'StageOutput')
IN_NAMES = ('SPIRV_Cross_Input', 'StageInput')
MACRO = ['#ifdef __spirv__', '#define WW_LOC(n) [[vk::location(n)]]', '#else', '#define WW_LOC(n)', '#endif']


def module_dir(rel):
    p = rel.split('/')
    if p[:3] == ['res', 'shaders', 'features']:
        return p[3]
    if p[:2] == ['src', 'features']:
        return p[2]
    return 'core'


def width(ty, arr):
    m = re.match(r'float(\d)x\d$', ty)
    return (int(m.group(1)) if m else 1) * arr


def struct_span(text, names):
    for nm in names:
        m = re.search(r'\bstruct\s+' + nm + r'\s*\{(.*?)\n\s*\}\s*;', text, re.S)
        if m:
            return m.start(1), m.end(1), m.group(1)
    return None


def members(body):
    out = []
    for ln in body.split('\n'):
        m = MEMB.match(ln) if not ln.lstrip().startswith('//') else None
        if m:
            _i, q, ty, nm, _a, arr, _s, sem, idx, _r = m.groups()
            out.append(dict(name=nm, type=ty, arr=int(arr) if arr else 1, sem=sem, idx=int(idx) if idx else 0))
    return out


def iface(path, outs):
    if not os.path.exists(path):
        return None
    t = open(path, encoding='utf-8', errors='replace', newline='').read()
    sp = struct_span(t, OUT_NAMES if outs else IN_NAMES)
    return None if sp is None else members(sp[2])


_BN = {}


def _byname(tree):
    if tree not in _BN:
        m = {}
        root = os.path.join(tree, 'res/shaders')
        for dp, _dn, fs in os.walk(root):
            if os.sep + 'hlsl' in dp.replace(root, ''):
                continue
            for f in fs:
                m.setdefault(f, os.path.relpath(os.path.join(dp, f), tree).replace('\\', '/'))
        _BN[tree] = m
    return _BN[tree]


def programs(tree):
    """yield (prog, [(ext, hlsl_path)...]) with the points-GS twin inserted where the frag has one."""
    files = subprocess.run(['git', '-C', tree, 'ls-files', '*.prog'], capture_output=True, text=True).stdout.split()
    for pf in files:
        pdir = os.path.dirname(pf)
        for ln in open(os.path.join(tree, pf), encoding='utf-8', errors='replace'):
            w = ln.split()
            if not w or w[0] != 'shaders':
                continue
            st = []
            for s in w[1:]:
                rel = os.path.normpath(os.path.join(pdir, s)).replace('\\', '/')
                if not os.path.exists(os.path.join(tree, rel)):
                    rel = 'res/shaders/' + s
                if not os.path.exists(os.path.join(tree, rel)):
                    hit = _byname(tree).get(os.path.basename(s))   # the loader finds a shader by file name anywhere under shaders/
                    if hit:
                        rel = hit
                ext = os.path.splitext(s)[1][1:]
                stem = os.path.splitext(os.path.basename(s))[0]
                st.append((ext, os.path.join(tree, 'res/shaders/hlsl', module_dir(rel), stem + '_' + SUFFIX[ext] + '.hlsl'), stem))
            st.sort(key=lambda x: ORDER.index(x[0]))
            if not any(e == 'geom' for e, _p, _s in st):
                fr = [x for x in st if x[0] == 'frag']
                if fr:
                    twin = os.path.join(os.path.dirname(fr[0][1]), fr[0][2] + '_points_gs.hlsl')
                    if os.path.exists(twin):
                        st.insert(len(st) - 1, ('geom', twin, fr[0][2]))
            yield pf, st


def edge_bad(a, b):
    A = {m['name']: m for m in a if not m['sem'].startswith('SV_')}
    bad = []
    for m in b:
        if m['sem'].startswith('SV_'):
            continue
        x = A.get(m['name'])
        if x is None:
            if m['name'] not in EXEMPT_IN_ONLY:
                bad.append('in-only ' + m['name'])
        elif (x['sem'], x['idx']) != (m['sem'], m['idx']):
            bad.append('%s %s%d vs %s%d' % (m['name'], x['sem'], x['idx'], m['sem'], m['idx']))
    return bad


def plan(tree):
    parent = {}

    def find(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    edges, badedges = [], []
    for pf, st in programs(tree):
        for (e1, p1, _s1), (e2, p2, _s2) in zip(st, st[1:]):
            a, b = iface(p1, True), iface(p2, False)
            if a is None or b is None:
                continue
            edges.append((p1, p2))
            find(p1)
            parent[find(p1)] = find(p2)
            bad = edge_bad(a, b)
            if bad:
                badedges.append((pf, p1, p2, bad))
    touched_roots = set(find(p1) for _pf, p1, _p2, _b in badedges)
    comp = sorted(set(p for e in edges for p in e if find(p) in touched_roots))
    prot = [p for p in comp if any(('/hlsl/' + k) in p.replace('\\', '/') for k in PROTECT)]
    # conflict graph over names: names sharing one interface struct
    wid, conf = {}, {}
    for p in comp:
        for outs in (True, False):
            ms = iface(p, outs)
            if not ms:
                continue
            ms = [m for m in ms if m['sem'] in ('TEXCOORD', 'POSITION')]   # SV_* are left as they are
            for m in ms:
                wid[m['name']] = max(wid.get(m['name'], 1), width(m['type'], m['arr']))
                conf.setdefault(m['name'], set()).update(x['name'] for x in ms if x['name'] != m['name'])
    order = sorted(wid, key=lambda n: (-len(conf[n]), -wid[n], n))
    amap = {}
    for n in order:
        w = wid[n]
        b = 0
        while True:
            if all(not (b < amap[o] + wid[o] and amap[o] < b + w) for o in conf[n] if o in amap):
                break
            b += 1
        amap[n] = b
    mx = max((amap[n] + wid[n] for n in amap), default=0)
    rel = [os.path.relpath(p, tree).replace('\\', '/') for p in comp]
    if rel:
        json.dump(dict(map=amap, files=rel), open(LOCMAP, 'w'), indent=1, sort_keys=True)
    else:
        print('PLAN nothing disagrees: locmap.json kept as it is (it lists the files already remapped; `rebuild` recreates it from the tree)')
    print('PLAN pairs %d disagreeing %d | files in touched components %d | names %d | highest slot used %d | protected files hit %d'
          % (len(edges), len(badedges), len(comp), len(amap), mx, len(prot)))
    for p in prot:
        print('PROTECTED', p)
    return amap


def apply_text(text, stem_path, amap, outs_and_ins):
    """rewrite the interface structs named in outs_and_ins (a list of struct-name tuples)"""
    for names in outs_and_ins:
        sp = struct_span(text, names)
        if sp is None:
            continue
        lines = sp[2].split('\n')
        new = []
        for ln in lines:
            m = MEMB.match(ln) if not ln.lstrip().startswith('//') else None
            if m and m.group(8) == 'POSITION' and any(x['sem'] == 'TEXCOORD' and x['name'] in amap for x in members(sp[2])):
                # the hull's gl_out clip position rides as a user semantic: its text stays POSITION (no index), but DXC refuses a
                # partial explicit assignment, so it gets the first Location the struct's mapped varyings leave free
                used = set()
                for x in members(sp[2]):
                    if x['sem'] == 'TEXCOORD' and x['name'] in amap:
                        used.update(range(amap[x['name']], amap[x['name']] + width(x['type'], x['arr'])))
                free = 0
                while free in used:
                    free += 1
                i, q, ty, nm, arr, _n, sep, sem, _idx, rest = m.groups()
                ln = '%sWW_LOC(%d) %s%s %s%s%s%s%s' % (i, free, q, ty, nm, arr or '', sep, sem, rest)
            elif m and m.group(8) == 'TEXCOORD' and m.group(4) in amap:
                i, q, ty, nm, arr, _n, sep, sem, _idx, rest = m.groups()
                n = amap[nm]
                ln = '%sWW_LOC(%d) %s%s %s%s%s%s%d%s' % (i, n, q, ty, nm, arr or '', sep, sem, n, rest)
            new.append(ln)
        text = text[:sp[0]] + '\n'.join(new) + text[sp[1]:]
    return text


def stage_structs(path):
    b = os.path.basename(path)
    if b.endswith('_vs.hlsl'):
        return [OUT_NAMES]
    if b.endswith('_ps.hlsl'):
        return [IN_NAMES]
    return [IN_NAMES, OUT_NAMES]


def shim_of(text):
    m = re.search(r'#include "(\w+BindingsVk\.hlsli)"', text)
    return m.group(1) if m else None


def ensure_macro(shim_path):
    t = open(shim_path, encoding='utf-8', newline='').read()
    if 'WW_LOC' in t:
        return False
    nl = '\r\n' if '\r\n' in t else '\n'
    if not t.endswith('\n'):
        t += nl
    t += nl.join(MACRO) + nl
    open(shim_path, 'w', encoding='utf-8', newline='').write(t)
    return True


def apply(tree):
    d = json.load(open(LOCMAP))
    amap, n, shims = d['map'], 0, set()
    for rel in d['files']:
        p = os.path.join(tree, rel)
        t = open(p, encoding='utf-8', newline='').read()
        t2 = apply_text(t, p, amap, stage_structs(p))
        sh = shim_of(t)
        if sh is None:
            print('NO SHIM include in', rel)
            sys.exit(2)
        shims.add(os.path.join(os.path.dirname(p), sh))
        if t2 != t:
            open(p, 'w', encoding='utf-8', newline='').write(t2)
            n += 1
    k = sum(1 for s in sorted(shims) if ensure_macro(s))
    print('APPLY files changed %d of %d, shims patched %d of %d' % (n, len(d['files']), k, len(shims)))


def rebuild(tree):
    """locmap.json from the tree: every hlsl file whose interface members carry WW_LOC(n) is in, name -> n from those lines"""
    amap, files = {}, []
    root = os.path.join(tree, 'res/shaders/hlsl')
    for dp, _, fs in os.walk(root):
        for f in sorted(fs):
            if not f.endswith('.hlsl'):
                continue
            p = os.path.join(dp, f)
            t = open(p, encoding='utf-8', errors='replace', newline='').read()
            found = False
            for m in re.finditer(r'WW_LOC\((\d+)\) (?:(?:nointerpolation|noperspective|centroid|linear|sample|precise) )*\w+ (\w+)', t):
                amap[m.group(2)] = int(m.group(1))
                found = True
            if found:
                files.append(os.path.relpath(p, tree).replace('\\', '/'))
    json.dump(dict(map=amap, files=sorted(files)), open(LOCMAP, 'w'), indent=1, sort_keys=True)
    print('REBUILD files %d names %d' % (len(files), len(amap)))


if __name__ == '__main__':
    tree, cmd = sys.argv[1], sys.argv[2]
    if cmd == 'rebuild':
        rebuild(tree)
    if cmd == 'plan':
        plan(tree)
    elif cmd == 'apply':
        apply(tree)
