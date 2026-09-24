"""DEFAULTS2 card byte gate checker."""
import hashlib
import os
import sys

from PIL import Image

L = sys.argv[1]
fails = 0


def tree(v):
    root = os.path.join(L, v)
    if not os.path.isdir(root):
        return {}
    return {f: hashlib.sha1(open(os.path.join(root, f), 'rb').read()).hexdigest()
            for f in sorted(os.listdir(root))}


def verdict(name, ok, msg):
    global fails
    print('%s %s -> %s' % (name, msg, 'PASS' if ok else 'FAIL'))
    fails += not ok


T = {v: tree(v) for v in ('C1', 'C2', 'C3', 'D1', 'D2')}

diff = sorted(k for k in set(T['C1']) | set(T['C2']) if T['C1'].get(k) != T['C2'].get(k))
verdict('G5', bool(T['C1']) and any('_oct_' in k for k in T['C1']) and not diff,
        'C1 (new, bare) == C2 (rung, TILE=256): %d files %s, differ %s' % (len(T['C1']), sorted(T['C1']), diff))


def albedo_side(v):
    for k in T[v]:
        if k.endswith('_oct_albedo.png'):
            return Image.open(os.path.join(L, v, k)).size
    return None


def octline(v):
    for k in T[v]:
        if k.endswith('.txt'):
            for ln in open(os.path.join(L, v, k)):
                if ln.startswith('oct '):
                    return ln.split()
    return None


moved = sorted(k for k in T['C3'] if T['C1'].get(k) != T['C3'][k])
octs = sorted(k for k in T['C3'] if '_oct_' in k or k.endswith('.txt'))
s1, s3 = albedo_side('C1'), albedo_side('C3')
o1, o3 = octline('C1'), octline('C3')
print('   C1 albedo %s oct line %s' % (s1, o1))
print('   C3 albedo %s oct line %s' % (s3, o3))
verdict('G6', bool(octs) and set(octs) <= set(moved) and s1 and s3 and max(s1) == 2048 and max(s3) == 1024
        and o1 and o3 and o1[11] == '256' and o3[11] == '128',  # AMENDED 14:3x: base is token 11 (spec); a 13th token 'spec1' now follows it
        'C3 (rung, bare = old default) vs C1: moved %s; unmoved %s' % (moved, sorted(set(T['C3']) - set(moved))))

diffd = sorted(k for k in set(T['D1']) | set(T['D2']) if T['D1'].get(k) != T['D2'].get(k))
lib = {}
p = os.path.join(L, 'D1', 'library.txt')
if os.path.exists(p):
    lib = dict(ln.split(None, 1) for ln in open(p).read().split('\n') if ln.strip())
verdict('G7', bool(T['D1']) and any('_oct_' in k for k in T['D1']) and not diffd
        and lib.get('oct', '').strip() == '8' and lib.get('tile', '').strip() == '256',
        'D1 (driver bare) == D2 (OCT=8 TILE=256): %d files, differ %s, library %s' % (len(T['D1']), diffd, lib))
print('RESULT %s (%d fails)' % ('PASS' if fails == 0 else 'FAIL', fails))
