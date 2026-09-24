"""AUDIT1: teach lodgen_btofree_ledger.py the ONE thing a stock record and an
FO4CS record legitimately disagree about -- the per-chunk native cache row.

Writes to a temp file and renames, and parses the result BEFORE the rename, so
a failure cannot leave the target truncated (MISTAKES 2026-09-17 16:27)."""
import ast
import io
import os

p = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_btofree_ledger.py'
s = io.open(p, encoding='utf-8', newline='').read()

# 1. the usage block
old = """  usage: lodgen_btofree_ledger.py keep <a.lodb> <b.lodb> [<a-root> <b-root>]
         lodgen_btofree_ledger.py drop <rung.lodb> <drop.lodb> [<a-root> <b-root>]
         lodgen_btofree_ledger.py same <a.lodb> <b.lodb> [<a-root> <b-root>]
"""
new = """  usage: lodgen_btofree_ledger.py keep <a.lodb> <b.lodb> [<a-root> <b-root>]
         lodgen_btofree_ledger.py drop <rung.lodb> <drop.lodb> [<a-root> <b-root>]
         lodgen_btofree_ledger.py same <a.lodb> <b.lodb> [<a-root> <b-root>]
         ... [--fo4cs-vs-stock]
"""
assert s.count(old) == 1
s = s.replace(old, new)

# 2. the paragraph that says WHY the flag exists, next to the two it joins
old = """`keep` is the general question "these two bakes recorded the same files, and
only the command line moved", asked by lodgen_btofree.sh leg (b) (rung vs
--keep-bto) and by lodgen_native.sh check 5 (stock vs --native --keep-bto).
"""
new = """`keep` is the general question "these two bakes recorded the same files, and
only the command line moved", asked by lodgen_btofree.sh leg (b) (rung vs
--keep-bto) and by lodgen_native.sh check 5 (stock vs --native --keep-bto).

`--fo4cs-vs-stock` (lane AUDIT1, 2026-09-17) is what check 5 needs since lane
INCR1: the FO4CS target writes a per-chunk native cache, `<ws>.<dim>.<cx>.<cy>
.lodj`, and BAKEREC1's record records it because it is a file the bake left on
disk. The stock target cannot have one. So the two records legitimately name
DIFFERENT files and `keep` -- which demands the same rows on both sides --
could no longer be asked of that pair at all: it went red on 2026-09-16 and the
red was the gate, not the bake (lane AUDIT1 step 1, standing red 1 of 3).

The flag does not excuse the difference, it CHECKS it: the FO4CS record must
carry at least one `.lodj` row and the stock record none, and only then are
those rows taken off both sides before the row-for-row comparison. A bake that
stops writing the cache, or a stock bake that starts, still goes red -- and so
does any other moved row, which is the whole point of the check.
"""
assert s.count(old) == 1
s = s.replace(old, new)

# 3. a helper beside without_bto(), spelled the same way
old = """def main(argv):
    if len(argv) not in (3, 5) or argv[0] not in ('keep', 'drop', 'same'):
        raise SystemExit(__doc__)
    mode, pa, pb = argv[:3]
    root_a, root_b = (argv[3], argv[4]) if len(argv) == 5 else (None, None)
"""
new = '''def cache_rows(doc):
    """every recorded output row that is a per-chunk native cache (`.lodj`)"""
    return [o for ch in doc.get('chunks', []) for o in ch.get('out', [])
            if o.split(' ')[0].lower().endswith('.lodj')]


def without_cache(doc):
    """the same ledger with every `.lodj` output row taken out"""
    d = json.loads(json.dumps(doc))
    for ch in d.get('chunks', []):
        ch['out'] = [o for o in ch.get('out', [])
                     if not o.split(' ')[0].lower().endswith('.lodj')]
    return d


def main(argv):
    fo4cs_vs_stock = '--fo4cs-vs-stock' in argv
    argv = [a for a in argv if a != '--fo4cs-vs-stock']
    if len(argv) not in (3, 5) or argv[0] not in ('keep', 'drop', 'same'):
        raise SystemExit(__doc__)
    mode, pa, pb = argv[:3]
    root_a, root_b = (argv[3], argv[4]) if len(argv) == 5 else (None, None)
'''
assert s.count(old) == 1
s = s.replace(old, new)

# 4. the flag's own two checks, and the strip, before anything is compared
old = """    out_a = [o for c in a.get('chunks', []) for o in c.get('out', [])]
    out_b = [o for c in b.get('chunks', []) for o in c.get('out', [])]
    ca = canon_doc(a2)
    cb = canon_doc(b2)
"""
new = """    if fo4cs_vs_stock:
        # THE ONE DIFFERENCE THAT IS ALLOWED, AND IT IS CHECKED, NOT EXCUSED.
        ja, jb = cache_rows(a), cache_rows(b)
        check('the FO4CS record carries the per-chunk native cache it wrote '
              '(%d .lodj row(s))' % len(jb), len(jb) > 0)
        check('and the stock record carries none, because the stock target has no cache '
              '(%d .lodj row(s))' % len(ja), len(ja) == 0)
        a2, b2 = without_cache(a2), without_cache(b2)
        a, b = without_cache(a), without_cache(b)

    out_a = [o for c in a.get('chunks', []) for o in c.get('out', [])]
    out_b = [o for c in b.get('chunks', []) for o in c.get('out', [])]
    ca = canon_doc(a2)
    cb = canon_doc(b2)
"""
assert s.count(old) == 1
s = s.replace(old, new)

# `check` is defined after `switches` are read; move nothing -- verify order.
i_check = s.index('    def check(what, ok):')
i_use = s.index('    if fo4cs_vs_stock:')
assert i_check < i_use, 'check() must be defined before the new block uses it'

ast.parse(s)
tmp = p + '.tmp'
io.open(tmp, 'w', encoding='utf-8', newline='').write(s)
os.replace(tmp, p)
b = open(p, 'rb').read()
print('patched; CR %d LF %d' % (b.count(b'\r'), b.count(b'\n')))
