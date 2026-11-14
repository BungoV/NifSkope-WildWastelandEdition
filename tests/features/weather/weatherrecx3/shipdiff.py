"""lane WEATHERRECX3: per-file diff of the shipped weather set, before (WEATHERRECX2's regenerated files) -> after
(the migrated originals), flattened to key paths; plus the sha256 of every file written.
usage: python -I shipdiff.py <before Weathers> <after Weathers> <diff.tsv> <after_sha256.txt>"""
import hashlib, json, os, sys

before, after, out, shaf = sys.argv[1:5]


def flat(v, p, o):
    if isinstance(v, dict):
        for k, x in v.items():
            flat(x, p + '.' + k if p else k, o)
    elif isinstance(v, list) and v and all(isinstance(x, dict) for x in v):
        for i, x in enumerate(v):
            flat(x, '%s[%d]' % (p, i), o)
    else:
        o[p] = v


def short(v):
    s = json.dumps(v, ensure_ascii=False)
    return s if len(s) <= 60 else s[:57] + '...'


rows, shas, tot = [], [], {'changed': 0, 'removed': 0, 'added': 0}
for dp, dn, fn in os.walk(after):
    for f in sorted(fn):
        if not f.endswith('.recx'):
            continue
        pa = os.path.join(dp, f)
        rel = os.path.relpath(pa, after).replace('\\', '/')
        shas.append('%s  %s' % (hashlib.sha256(open(pa, 'rb').read()).hexdigest(), rel))
        a, b = {}, {}
        pb = os.path.join(before, rel)
        if os.path.isfile(pb):
            flat(json.load(open(pb, encoding='utf-8')), '', b)
        flat(json.load(open(pa, encoding='utf-8')), '', a)
        ch, rm, ad = [], [], []
        vr = sum(1 for k in b if '.vanillaRecord.' in k and b[k] != a.get(k))
        for k in sorted(set(a) | set(b)):
            if '.vanillaRecord.' in k:
                continue
            if k not in a:
                rm.append('%s %s' % (k, short(b[k])))
            elif k not in b:
                ad.append('%s %s' % (k, short(a[k])))
            elif a[k] != b[k]:
                ch.append('%s %s->%s' % (k, short(b[k]), short(a[k])))
        tot['changed'] += len(ch)
        tot['removed'] += len(rm)
        tot['added'] += len(ad)
        rows.append('\t'.join([rel, str(len(ch)), str(len(rm)), str(len(ad)), str(vr), '; '.join(ch), '; '.join(rm),
                               '; '.join(ad)]))
with open(out, 'w', encoding='utf-8', newline='\n') as o:
    o.write('file\tchanged\tremoved\tadded\tvanillaRecord fields removed\tchanged old->new\tremoved (old value)\t'
            'added (new value)\n' + '\n'.join(rows) + '\n')
with open(shaf, 'w', encoding='utf-8', newline='\n') as o:
    o.write('\n'.join(shas) + '\n')
print('shipdiff: files %d keys changed %d removed %d added %d -> %s' % (len(rows), tot['changed'], tot['removed'],
                                                                      tot['added'], out))
