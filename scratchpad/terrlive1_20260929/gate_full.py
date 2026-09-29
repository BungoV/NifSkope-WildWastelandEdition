"""TERRLIVE1 byte gate: FULL (new exe, --terrain-option full) against the rung's bake of the same box.
usage: python gate_full.py <rung root> <new root> [--expect-red]
Every file under mod/ and scr/ must be byte-identical, except:
  - the two NEW files <ws>.lodd / <ws>.lodg (additive; listed, never compared against the rung, which has none);
  - *.lodb: compared line by line after masking the rows the new option itself changes
    (baked, switch, switches, terrain, the stage-times census, the decals census, the terrain-option census,
     product rows naming .lodd/.lodg, end, and the per-chunk input digest that folds the switch vector in).
Prints one verdict line: GATE GREEN / GATE RED, plus every finding. With --expect-red the exit code is 0 only when red
(the sabotage run)."""
import os, sys, hashlib, collections, re

A, B = sys.argv[1:3]
expect_red = '--expect-red' in sys.argv

def tree(root):
    d = {}
    for sub in ('mod', 'scr'):
        for dp, dn, fn in os.walk(os.path.join(root, sub)):
            for f in fn:
                if f.endswith('.log') or f.startswith('.'):
                    continue
                p = os.path.join(dp, f)
                d[os.path.relpath(p, root).replace(chr(92), '/')] = p
    return d

def h(p):
    x = hashlib.sha1()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 22), b''):
            x.update(b)
    return x.digest()

NEW = re.compile(r'\.(lodd|lodg)$', re.I)

ROOTS = {}   # tree root -> '<root>' (the two bakes live in different folders and run from different exe copies)
RUN = re.compile(r'run_(rung|new)')
SECS = re.compile(r'\b\d+(\.\d+)? s\b')

def norm(p, root):
    # the environment, never the content: the bake folder, the run folder, wall-clock seconds
    t = open(p, 'rb').read().decode('utf-8', 'replace').replace(os.path.abspath(root).replace(chr(92), '/'), '<root>')
    t = t.replace(root.replace(chr(92), '/').rstrip('/'), '<root>')
    return SECS.sub('<t> s', RUN.sub('run_X', t))

def key_mask(p, root):
    # the chunk-cache key: 'inputs' and 'switches' fold in the switch vector, which now carries the option
    return [l for l in norm(p, root).splitlines() if not l.startswith(('inputs ', 'switches '))]

def lodb_mask(p, root='.'):
    out = []
    for l in norm(p, root).splitlines():
        f = l.split('\t')
        k = f[0]
        if k in ('baked', 'switch', 'switches', 'terrain', 'end', 'resource'):
            continue
        if k == 'census' and len(f) > 1 and (f[1].startswith('stage times:') or f[1].startswith('decals:')
                                              or 'terrain option' in f[1] or 'peak working set' in f[1]):
            continue
        if k == 'product' and len(f) > 1 and NEW.search(f[1]):
            continue
        if k == 'product' and len(f) > 2 and f[1].endswith('.txt'):
            l = '	'.join(f[:2] + ['<hash: the .txt is compared masked>'] + f[3:])
        if k == 'lodb' and len(f) > 4:
            l = '	'.join(f[:4] + ['<exe bytes>'])
        if k == 'chunk' and len(f) > 4:
            l = '\t'.join(f[:4] + ['<inputs>'])
        out.append(l)
    return out

TA, TB = tree(A), tree(B)
cnt = collections.defaultdict(lambda: [0, 0, 0, 0])
bad, newfiles, lodbnotes = [], [], []
for n in sorted(set(TA) | set(TB)):
    e = os.path.splitext(n)[1].lower() or n
    if NEW.search(n):
        newfiles.append((n, os.path.getsize(TB[n]) if n in TB else -1, n in TA))
        continue
    if n not in TB:
        cnt[e][2] += 1; bad.append(('onlyRung', n)); continue
    if n not in TA:
        cnt[e][3] += 1; bad.append(('onlyNew', n)); continue
    if h(TA[n]) == h(TB[n]):
        cnt[e][0] += 1
        continue
    if e in ('.key', '.txt'):
        f = key_mask if e == '.key' else (lambda p, r: norm(p, r).splitlines())
        if f(TA[n], A) == f(TB[n], B):
            cnt[e][0] += 1
            lodbnotes.append(n)
            continue
        cnt[e][1] += 1
        bad.append(('DIFF-masked', n))
        continue
    if e == '.lodb':
        ma, mb = lodb_mask(TA[n], A), lodb_mask(TB[n], B)
        if ma == mb:
            cnt[e][0] += 1
            lodbnotes.append(n)
            continue
        diff = [x for x in set(ma) ^ set(mb)][:4]
        bad.append(('DIFF-lodb', n + ' ' + repr(diff)[:400]))
        cnt[e][1] += 1
        continue
    cnt[e][1] += 1
    bad.append(('DIFF', n))

print('files rung %d  new %d' % (len(TA), len(TB)))
for e, (s, d, a, b) in sorted(cnt.items()):
    print('  %-10s same %6d  differ %5d  onlyRung %4d  onlyNew %4d' % (e, s, d, a, b))
for k, n in bad[:40]:
    print('  %s %s' % (k, n))
if len(bad) > 40:
    print('  ... %d more' % (len(bad) - 40))
print('new files (additive):', ', '.join('%s %d bytes%s' % (n, s, ' (ALSO IN RUNG)' if r else '') for n, s, r in newfiles) or 'none')
print('equal only after the mask (.lodb/.key/.txt):', len(lodbnotes))
red = bool(bad)
print('GATE %s: %d same, %d finding(s)' % ('RED' if red else 'GREEN', sum(v[0] for v in cnt.values()), len(bad)))
sys.exit(0 if red == expect_red else 1)
