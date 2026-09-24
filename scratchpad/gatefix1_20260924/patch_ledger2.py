"""GATEFIX1 step 2 on tests/spells/lodgen_btofree_ledger.py:
  1. the census is compared through the record's OWN volatile mask (lodb_read.normalise,
     docs/LODGEN_BAKE_RECORD.md section 3) and with each record's own --out-dir spelled
     <out-dir>, the way canon() reduces a row's path -- never dropped;
  2. drop mode compared a JSON string with strip_digests() (AttributeError on every run
     since AUDIT1 made shape() return text); it now asks, by name, the two things a drop
     moves in the record -- the bto disposition clause and the end count -- and holds
     everything else to ==."""
P = 'E:/Projects/NifskopeWWE-gatefix1/tests/spells/lodgen_btofree_ledger.py'
with open(P, 'rb') as fh:
    src = fh.read()
cr0 = src.count(b'\r')
s = src.decode('utf-8')

reps = [
# ---- 1. the census mask, applied in shape()
(
'''def shape(doc, drop_target, ws='Commonwealth'):''',
'''def out_dir_of(doc):
    """the record's own --out-dir, as its command line spelled it, or None"""
    toks = doc.get('switchTokens') or []
    for i, t in enumerate(toks[:-1]):
        if t == '--out-dir':
            return toks[i + 1].replace(chr(92), '/').rstrip('/')
    return None


def steady_census(doc):
    """the census rows with the record's OWN volatile parts masked, and nothing else
    (lane GATEFIX1, 2026-09-24)

    Two things in a census row move between two bakes of the same inputs, and
    neither is a property of the bake:
      * the two volatile things of docs/LODGEN_BAKE_RECORD.md section 3 that
        live in the census -- the `stage times:` line and the `peak working
        set:` clause. They are masked by lodb_read.normalise(), the ONE Python
        half of the record's own rule, so this file cannot drift from it.
      * the out-dir. `bake census:` names the layout root under it, and the
        drop bake names its scratch folder under it; two bakes into two
        folders spell those differently. The record's own `--out-dir` token
        becomes `<out-dir>`, the way canon() reduces a row's path: a row that
        differs anywhere else still fails.
    MASKED, never dropped: a lost or an added census row still differs."""
    od = out_dir_of(doc)
    rows = []
    for r in doc.get('census', []):
        t = lodb_read.normalise('census' + chr(9) + r).rstrip(chr(10))
        t = t.split(chr(9), 1)[1] if chr(9) in t else ''
        if od:
            t = t.replace(od, '<out-dir>')
        rows.append(t)
    return rows


def shape(doc, drop_target, ws='Commonwealth'):'''),
(
'''    d = strip_digests(canon_doc(doc, ws))
    drop = set(RUN_KEYS)''',
'''    d = strip_digests(canon_doc(doc, ws))
    if 'census' in d:
        d['census'] = steady_census(doc)
    drop = set(RUN_KEYS)'''),
# ---- 2. drop mode
(
'''        check('every other recorded file, digest for digest, is what the rung recorded '
              '(or differs by the path rewrite, proved on the files themselves)',
              json.dumps(strip_digests(canon_doc(without_bto(a2))), sort_keys=True)
              == json.dumps(strip_digests(cb), sort_keys=True)
              and explained([r for r in out_a
                             if not r.split(' ')[0].upper().endswith('.BTO')],
                            out_b, root_a, root_b))''',
'''        # WHAT A DROP MOVES IN THE RECORD, ASKED BY NAME (lane GATEFIX1,
        # 2026-09-24). This comparison used to hand shape()'s TEXT to
        # strip_digests() and died with an AttributeError on every run since
        # AUDIT1 made shape() return text; the rung wrote a version 1 record
        # until the re-pin, so leg (a) skipped before reaching it and nobody
        # saw. A drop bake moves exactly three things besides the .BTO row,
        # and each is checked here rather than left out:
        #   * the census gains one `bto scratch:` row, and the `bake census:`
        #     row's bto clause says scratch/dropped/freed where the rung's says
        #     the mod folder/0/0. The NUMBERS are leg (d)'s; here the clause is
        #     reduced to its chunk count and every other census byte is ==.
        #   * the manifest moves into the record's own folder, so the `end`
        #   line counts it: the file and byte deltas must be exactly the
        #   out rows that entered the record folder, sized on disk.
        import re
        disp = re.compile(r'bto built in (the mod folder|scratch [^,]*), '
                          r'(\\d+) chunk\\(s\\), \\d+ dropped, \\d+ bytes freed')

        def census_for_drop(doc):
            rows = [disp.sub(r'bto built in <where>, \\2 chunk(s), <n> dropped, <n> bytes freed', r)
                    for r in steady_census(doc)]
            return rows

        cen_a, cen_b = census_for_drop(a2), census_for_drop(b2)
        scr_a = [r for r in cen_a if r.startswith('bto scratch:')]
        scr_b = [r for r in cen_b if r.startswith('bto scratch:')]
        check('the census: the default bake prints ONE bto scratch row and the rung none '
              '(%d vs %d)' % (len(scr_b), len(scr_a)), len(scr_b) == 1 and len(scr_a) == 0)
        cen_b = [r for r in cen_b if not r.startswith('bto scratch:')]
        check('and every other census row, the bto clause reduced to its chunk count, '
              'is the rung\\'s (%d rows)' % len(cen_a), cen_a == cen_b)

        def in_record(doc):
            return [o.split(' ')[0] for c in doc.get('chunks', []) for o in c.get('out', [])
                    if not o.split(' ')[0].replace(chr(92), '/').startswith('../')]

        def sized(rec, rows):
            tot = 0
            for r in rows:
                try:
                    tot += os.path.getsize(os.path.join(os.path.dirname(rec), r))
                except OSError:
                    return None
            return tot
        ia, ib = in_record(a2), in_record(b2)
        za, zb = sized(pa, ia), sized(pb, ib)
        dfiles = b2.get('endFiles', -1) - a2.get('endFiles', -1)
        dbytes = b2.get('endBytes', -1) - a2.get('endBytes', -1)
        check('the end line moved by exactly the out rows that entered the record folder '
              '(%+d files %+d bytes; rows %+d, %s bytes)'
              % (dfiles, dbytes, len(ib) - len(ia),
                 'unreadable' if za is None or zb is None else '%+d' % (zb - za)),
              za is not None and zb is not None
              and dfiles == len(ib) - len(ia) and dbytes == zb - za)

        def rest(doc, cen):
            d = without_bto(doc)
            d['census'] = cen
            d.pop('endFiles', None)
            d.pop('endBytes', None)
            return d
        ra, ua2 = shape(rest(a2, cen_a), False)
        rb, ub2 = shape(rest(b2, cen_b), False)
        check('every other recorded file, digest for digest, is what the rung recorded '
              '(or differs by the path rewrite, proved on the files themselves)',
              ra == rb and not ua2 and not ub2
              and explained([r for r in out_a
                             if not r.split(' ')[0].upper().endswith('.BTO')],
                            out_b, root_a, root_b))'''),
]
for old, new in reps:
    n = s.count(old)
    assert n == 1, (n, old[:80])
    s = s.replace(old, new)
out = s.encode('utf-8')
assert out.count(b'\r') == cr0, (out.count(b'\r'), cr0)
with open(P, 'wb') as fh:
    fh.write(out)
print('patched; CR', cr0)
