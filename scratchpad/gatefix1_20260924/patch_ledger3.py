"""GATEFIX1 step 3: the drop bake's layout clause counts the moved manifest too
(9 -> 10 files on the real pair); that delta is CHECKED to be the same out rows
that entered the record folder, then reduced -- never just masked."""
P = 'E:/Projects/NifskopeWWE-gatefix1/tests/spells/lodgen_btofree_ledger.py'
with open(P, 'rb') as fh:
    src = fh.read()
cr0 = src.count(b'\r')
s = src.decode('utf-8')

reps = [
(
'''        def census_for_drop(doc):
            rows = [disp.sub(r'bto built in <where>, \\2 chunk(s), <n> dropped, <n> bytes freed', r)
                    for r in steady_census(doc)]
            return rows
''',
'''        # the layout clause of the same row counts the FO4CS files, and the
        # manifest that moved into the record folder is one of them: its delta
        # is checked against the out rows below, then reduced here.
        lay = re.compile(r'(layout [^,]*), (\\d+) file\\(s\\)')

        def census_for_drop(doc):
            rows = [lay.sub(r'\\1, <n> file(s)',
                            disp.sub(r'bto built in <where>, \\2 chunk(s), <n> dropped, <n> bytes freed', r))
                    for r in steady_census(doc)]
            return rows

        def layout_files(doc):
            got = [int(m.group(2)) for r in steady_census(doc) for m in [lay.search(r)] if m]
            return got[0] if len(got) == 1 else None
'''),
(
'''        ia, ib = in_record(a2), in_record(b2)
        za, zb = sized(pa, ia), sized(pb, ib)
''',
'''        ia, ib = in_record(a2), in_record(b2)
        za, zb = sized(pa, ia), sized(pb, ib)
        la, lb = layout_files(a2), layout_files(b2)
        check('the census layout count moved by exactly the out rows that entered the '
              'record folder (%s -> %s; rows %+d)' % (la, lb, len(ib) - len(ia)),
              la is not None and lb is not None and lb - la == len(ib) - len(ia))
'''),
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
