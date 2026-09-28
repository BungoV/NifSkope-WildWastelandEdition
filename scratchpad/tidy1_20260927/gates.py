"""TIDY1 gates over a Boston-box --native bake.

  python gates.py snapshot <bake root> <out.json>
      every file under <root>/mod (size, sha1); per texture-array SOURCE the sha1 of its layer's colour +
      normal + mask slices (all mips) and whether its emissive is black; the merged-spelling lines the exe
      logged; card-array sheets; the manifests' A lines and whether each resolves to a listed layer.
  python gates.py compare <base.json> <test.json> <off|on>
      off: every file byte-identical.  on: the black emissive files gone, every source resolves to a layer
      with the SAME texel hash as the base, the layer count, the bytes saved, every A line resolves.
Prints one verdict line per gate and exits 1 on any failure.
"""
import glob
import hashlib
import json
import os
import re
import sys

BLOCK = {71: 8, 77: 16, 98: 16}


def sha1(b):
    return hashlib.sha1(b).hexdigest()


def dds_layers(path):
    """(dxgi, arraySize, per-layer bytes, raw) of a DX10 array DDS."""
    b = open(path, 'rb').read()
    h, w = int.from_bytes(b[12:16], 'little'), int.from_bytes(b[16:20], 'little')
    mips = max(1, int.from_bytes(b[28:32], 'little'))
    if b[84:88] != b'DX10':
        raise ValueError('%s: not DX10' % path)
    dxgi = int.from_bytes(b[128:132], 'little')
    asize = int.from_bytes(b[140:144], 'little')
    bb = BLOCK[dxgi]
    per = 0
    mw, mh = w, h
    for _ in range(mips):
        per += max(1, (mw + 3) // 4) * max(1, (mh + 3) // 4) * bb
        mw, mh = max(1, mw // 2), max(1, mh // 2)
    if len(b) != 148 + asize * per:
        raise ValueError('%s: size %d != 148 + %d x %d' % (path, len(b), asize, per))
    return dxgi, asize, per, b


def layer_slice(cache, path, layer):
    if path not in cache:
        cache[path] = dds_layers(path)
    dxgi, asize, per, b = cache[path]
    if layer >= asize:
        raise ValueError('%s: layer %d of %d' % (path, layer, asize))
    return b[148 + layer * per:148 + (layer + 1) * per], dxgi


def bc1_black(s):
    # a BC1 block is black when both colour endpoints are 0 (any indices then pick black)
    return all(s[k:k + 4] == b'\0\0\0\0' for k in range(0, len(s), 8))


def snapshot(root, out):
    mod = os.path.join(root, 'mod')
    files = {}
    for dp, _, fns in os.walk(mod):
        for fn in fns:
            p = os.path.join(dp, fn)
            rel = os.path.relpath(p, mod).replace('\\', '/').lower()
            b = open(p, 'rb').read()
            files[rel] = [len(b), sha1(b)]
    objs = glob.glob(os.path.join(mod, 'FO4CSLOD', '*', 'Objects'))[0]
    side = glob.glob(os.path.join(objs, '*.LodgenArrays.txt'))[0]
    cache = {}
    sources = {}
    layersOf = {}
    for line in open(side, encoding='utf-8'):
        if line.startswith('#') or not line.strip():
            continue
        t = line.split()
        fam, cls, layer, lodm, src = t[0], t[1], int(t[2]), t[3], t[8]
        stem = os.path.join(objs, lodm.replace('\\', '/').split('/')[-1][:-5])
        sfx = ('_bc', '_n', '_rmaos', '_e') if fam == 'pbr' else ('_d', '_n', '_gsaos', '_g')
        h = hashlib.sha1()
        for s in sfx[:3]:
            sl, _ = layer_slice(cache, stem + s + '.DDS', layer)
            h.update(sl)
        ep = stem + sfx[3] + '.DDS'
        if os.path.exists(ep):
            sl, _ = layer_slice(cache, ep, layer)
            emis = 'black' if bc1_black(sl) else sha1(sl)
        else:
            emis = 'black'          # absent = emits nothing
        sources[src.lower()] = {'lodm': lodm.lower(), 'layer': layer, 'texels': h.hexdigest(), 'emissive': emis}
        layersOf[lodm.lower()] = layersOf.get(lodm.lower(), 0) + 1
    alias = {}
    log = os.path.join(root, 'bake.log')
    if os.path.exists(log):
        for line in open(log, encoding='utf-8', errors='replace'):
            m = re.match(r'lodgen: arrays: (.+) = layer (\d+) \((.+)\), identical texels', line.strip())
            if m:
                alias[m.group(1).lower()] = m.group(3).lower()
    aLines = aOk = 0
    bad = []
    for man in glob.glob(os.path.join(mod, 'FO4CSLOD', '*', '*.manifest.txt')):
        for line in open(man, encoding='utf-8', errors='replace'):
            t = line.split()
            if len(t) >= 4 and t[0] == 'A':
                aLines += 1
                lodm, layer = ' '.join(t[3:]).lower(), int(t[2])
                n = layersOf.get(lodm, 0)
                if n and (layer == -1 or 0 <= layer < n):
                    aOk += 1
                elif len(bad) < 5:
                    bad.append(line.strip())
    emFiles = {}
    for p in glob.glob(os.path.join(objs, '*_g.DDS')) + glob.glob(os.path.join(objs, '*_e.DDS')):
        n = os.path.basename(p)
        dxgi, asize, per, b = dds_layers(p)
        emFiles[n.lower()] = {'size': len(b), 'black': all(bc1_black(b[148 + l * per:148 + (l + 1) * per]) for l in range(asize)),
                              'cards': '.lodgencards.' in n.lower()}
    json.dump({'root': root, 'files': files, 'sources': sources, 'layers': sum(layersOf.values()),
               'alias': alias, 'aLines': aLines, 'aOk': aOk, 'aBad': bad, 'emissiveFiles': emFiles},
              open(out, 'w'), indent=1)
    print('snapshot %s: %d files, %d sources, %d layers, %d aliases, A lines %d/%d resolve, %d emissive files'
          % (root, len(files), len(sources), sum(layersOf.values()), len(alias), aOk, aLines, len(emFiles)))


def compare(basep, testp, mode):
    B, T = json.load(open(basep)), json.load(open(testp))
    fails = 0

    def gate(name, ok, detail):
        nonlocal fails
        print('%s  %s: %s' % ('PASS' if ok else 'FAIL', name, detail))
        fails += (not ok)

    bf, tf = B['files'], T['files']
    same = [k for k in bf if k in tf and bf[k] == tf[k]]
    differ = sorted(k for k in bf if k in tf and bf[k] != tf[k])
    gone = sorted(k for k in bf if k not in tf)
    new = sorted(k for k in tf if k not in bf)
    bBytes, tBytes = sum(v[0] for v in bf.values()), sum(v[0] for v in tf.values())
    print('files: %d same, %d differ, %d only in base, %d only in test; bytes %d -> %d (%+d)'
          % (len(same), len(differ), len(gone), len(new), bBytes, tBytes, tBytes - bBytes))
    for k in differ[:12]:
        print('   differs: %s  %d -> %d' % (k, bf[k][0], tf[k][0]))
    for k in gone[:30]:
        print('   gone:    %s  %d' % (k, bf[k][0]))
    for k in new[:12]:
        print('   new:     %s  %d' % (k, tf[k][0]))
    if mode == 'off':
        gate('off is byte-identical to the base (every file under mod/)', not differ and not gone and not new,
             '%d of %d files identical' % (len(same), len(bf)))
        return fails
    # on
    bEm = B['emissiveFiles']
    blackBase = {k: v for k, v in bEm.items() if v['black']}
    lit = sorted(k for k, v in bEm.items() if not v['black'])
    print('base emissive files: %d, %d black, %d with light (%s)' % (len(bEm), len(blackBase), len(lit), ', '.join(lit)))
    goneEm = [k for k in blackBase if not any(g.endswith('/' + k) for g in tf)]
    meshSaved = sum(v['size'] for k, v in blackBase.items() if k in goneEm and not v['cards'])
    cardSaved = sum(v['size'] for k, v in blackBase.items() if k in goneEm and v['cards'])
    gate('every black emissive file is gone', len(goneEm) == len(blackBase),
         '%d of %d gone (mesh %d B, cards %d B, total %d B)'
         % (len(goneEm), len(blackBase), meshSaved, cardSaved, meshSaved + cardSaved))
    keptLit = [k for k in lit if any(g.endswith('/' + k) and tf[g] == bf[g] for g in tf if g in bf)]
    gate('every emissive file with light is kept byte-identical', len(keptLit) == len(lit) and len(T['emissiveFiles']) == len(lit),
         '%d of %d kept; %d emissive files in the test' % (len(keptLit), len(lit), len(T['emissiveFiles'])))
    unexplained = [k for k in gone if not any(k.endswith('/' + g) for g in goneEm)]
    gate('no other file is gone', not unexplained, '%d other files gone %s' % (len(unexplained), unexplained[:5]))
    ok = mism = miss = 0
    for k, v in B['sources'].items():
        rep = k if k in T['sources'] else T['alias'].get(k)
        tv = T['sources'].get(rep) if rep else None
        if tv is None:
            miss += 1
            print('   source with no layer: %s' % k)
        elif tv['texels'] == v['texels'] and tv['emissive'] == v['emissive']:
            ok += 1
        else:
            mism += 1
            print('   texels differ: %s (%s layer %d -> %s layer %d)' % (k, v['lodm'], v['layer'], tv['lodm'], tv['layer']))
    gate('every base source resolves to a layer with identical texels (colour+normal+mask, all mips; emissive both black)',
         ok == len(B['sources']) and not mism and not miss,
         '%d of %d identical, %d differ, %d unresolved' % (ok, len(B['sources']), mism, miss))
    gate('layer count falls by exactly the merged spellings', T['layers'] < B['layers'] and T['layers'] + len(T['alias']) == B['layers'], '%d -> %d layers (%d merged spellings logged)'
         % (B['layers'], T['layers'], len(T['alias'])))
    gate('every A line resolves to a listed layer', T['aOk'] == T['aLines'] and T['aLines'] > 0,
         'test %d/%d (base %d/%d) %s' % (T['aOk'], T['aLines'], B['aOk'], B['aLines'], T['aBad']))
    return fails


def predict(objs):
    """Read-only: how many layers of an existing Objects folder's mesh arrays would merge, and the bytes."""
    side = glob.glob(os.path.join(objs, '*.LodgenArrays.txt'))[0]
    cache, groups, per = {}, {}, {}
    for line in open(side, encoding='utf-8'):
        if line.startswith('#') or not line.strip():
            continue
        t = line.split()
        stem = os.path.join(objs, t[3].replace('\\', '/').split('/')[-1][:-5])
        h = hashlib.sha1()
        size = 0
        for s in ('_d', '_n', '_gsaos'):
            sl, _ = layer_slice(cache, stem + s + '.DDS', int(t[2]))
            h.update(sl)
            size += len(sl)
        groups.setdefault((stem, h.hexdigest()), []).append(t[8])
        per[stem] = size
    merged = sum(len(v) - 1 for v in groups.values())
    saved = sum((len(v) - 1) * per[k[0]] for k, v in groups.items())
    n = sum(len(v) for v in groups.values())
    print('%s: %d layers -> %d, %d B saved' % (objs, n, n - merged, saved))


if __name__ == '__main__':
    if sys.argv[1] == 'predict':
        for o in sys.argv[2:]:
            predict(o)
        sys.exit(0)
    if sys.argv[1] == 'snapshot':
        snapshot(sys.argv[2], sys.argv[3])
        sys.exit(0)
    sys.exit(1 if compare(sys.argv[2], sys.argv[3], sys.argv[4]) else 0)
