"""GATEFIX1: lodgen_btofree_ledger.py learns --generators-differ.

Since lane VTFIX1 (2026-09-24) every chunk's inputs digest starts with the sha1
of the exe that baked it, so a rung and a different exe under test can never
record the same inputs. The harness knows whether the two exes' bytes differ and
says so; the checker then CHECKS that every chunk's inputs moved (the word works)
and compares the rest of the record without that one field. Same exe bytes: the
field is compared exactly as before.
"""
P = 'E:/Projects/NifskopeWWE-gatefix1/tests/spells/lodgen_btofree_ledger.py'
with open(P, 'rb') as fh:
	src = fh.read()
cr0 = src.count(b'\r')


def rep(s, old, new):
	n = s.count(old)
	assert n == 1, (n, old[:70])
	return s.replace(old, new)


src = rep(src, b'''def main(argv):
    fo4cs_vs_stock = '--fo4cs-vs-stock' in argv
    argv = [a for a in argv if a != '--fo4cs-vs-stock']
''', b'''def without_inputs(doc):
    """the same ledger with every chunk's inputs digest taken off"""
    d = json.loads(json.dumps(doc))
    for ch in d.get('chunks', []):
        ch.pop('inputs', None)
    return d


def main(argv):
    fo4cs_vs_stock = '--fo4cs-vs-stock' in argv
    # THE GENERATOR WORD (lane VTFIX1, 2026-09-24; flag added by GATEFIX1). Every
    # chunk's inputs digest starts with the sha1 of the exe that baked it, so two
    # DIFFERENT exes never record the same inputs, by design. The caller says
    # when the two exes' bytes differ; the inputs are then CHECKED to have moved,
    # chunk by chunk, and only after that left out of the comparison below.
    generators_differ = '--generators-differ' in argv
    argv = [a for a in argv if a not in ('--fo4cs-vs-stock', '--generators-differ')]
''')

src = rep(src, b'''    sa, sb = a.get('switches'), b.get('switches')
''', b'''    if generators_differ:
        def inputs_of(doc):
            return dict(((c.get('cx'), c.get('cy'), c.get('dim')), c.get('inputs'))
                        for c in doc.get('chunks', []))
        ia, ib = inputs_of(a), inputs_of(b)
        common = sorted(set(ia) & set(ib))
        moved = [k for k in common if ia[k] != ib[k]]
        check('the exes differ, and the generator word moved every chunk inputs digest '
              '(%d of %d)' % (len(moved), len(common)),
              len(common) > 0 and len(moved) == len(common))
        a, b = without_inputs(a), without_inputs(b)

    sa, sb = a.get('switches'), b.get('switches')
''')

assert src.count(b'\r') == cr0
with open(P + '.new', 'wb') as fh:
	fh.write(src)
print('patched')
