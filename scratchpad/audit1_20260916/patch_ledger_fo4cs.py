"""AUDIT1 step 6: make the ledger comparison ask its own sentence.

STANDING RED 1. `lodgen_native.sh` section 5 compares the stock bake's record
with the `--native --keep-bto` bake's record and says

    the second ledger differs from the first ONLY in the command-line digest

but what it RUNS is a whole-document JSON comparison of everything the record
reader returns. Measured on this lane's own Sanctuary pair, the same region
baked both ways, the two documents differ in ten keys and not one of them is an
output file:

    baked         two clock readings, seconds apart
    bytes         6,258 vs 10,984         lineCount   90 vs 113
    switchTokens  the second carries --native and --keep-bto
    target        stock vs fo4cs          endFiles    52 vs 11
    census        4 rows vs 12            endBytes    11,512,374 vs 226,572,422
    hashes        1 (loadOrder) vs 5 (the four corpus digests too)
    chunks        the parallel outFiles/outDigests lists, un-canonicalised

The last one is the tell that this is a gate defect and not a bake defect. Lane
BAKEREC1 gave the record reader `outFiles` and `outDigests` beside `out`, and
`canon_doc()` and `strip_digests()` only ever knew about `out`. So every row
reduction this file exists for -- the FO4CSLOD prefix off, the `../` hops off,
the digests held back for `explained()` -- was being defeated by the raw copies
sitting in the same dict. That alone made the comparison unpassable across the
LAYOUT1 move, whatever the bake did.

The other nine are per-RUN or per-TARGET bookkeeping. `baked` proves it: a copy
of one stock record with nothing changed but its clock line already fails the
check that claims to compare outputs --

    $ ... same self.lodb timeshift.lodb
      FAIL the two records name the same outputs, digest for digest

-- so `keep` and `same` could never pass on two real bakes at all. They pass in
the suite today only where they are skipped (both legs that would reach them hit
the version-1 rung guard first), which is why this went unnoticed until an FO4CS
pair had to be compared for real.

THE FIX is to compare what the sentence says: the output rows, and the bake
identity that owns them. `shape()` names three groups and the check FAILS if the
reader ever returns a key none of them names, so the list cannot rot quietly
into a sweep. Digests stay where they were, checked row by row by `explained()`.

Verified before the edit, on the Sanctuary pair: shape equal True, 52 rows equal
True, 0 unaccounted keys, and the refuters -- a dropped row, a moved input
digest, a moved load order -- all still unequal.
"""
import io
import os
import sys
import tempfile

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_btofree_ledger.py'

# ---------------------------------------------------------------- 1. canon_doc
OLD_CANON = """    d = json.loads(json.dumps(doc))
    for ch in d.get('chunks', []):
        ch['out'] = [canon(o, ws) for o in ch.get('out', [])]
    return d
"""
NEW_CANON = """    d = json.loads(json.dumps(doc))
    for ch in d.get('chunks', []):
        # SORTED, because the order two bakes happen to write their outputs in
        # is not a property of the bytes. It is a multiset, so a duplicated or
        # a dropped row still fails.
        ch['out'] = sorted(canon(o, ws) for o in ch.get('out', []))
        # AND THE PARALLEL LISTS GO (lane AUDIT1, 2026-09-17). lodb_read hands
        # back `outFiles` and `outDigests` beside `out`, carrying the same rows
        # UN-canonicalised; leaving them in a dict that is then compared with
        # `==` silently undoes every reduction above, which is exactly what
        # standing red 1 turned out to be.
        ch.pop('outFiles', None)
        ch.pop('outDigests', None)
    return d
"""

# ------------------------------------------------------------------- 2. shape()
OLD_STRIP_TAIL = """def without_bto(doc):"""
NEW_SHAPE = '''# WHAT A LEDGER COMPARISON IS ABOUT, BY NAME (lane AUDIT1, 2026-09-17).
# Everything the reader returns is in exactly one of these three groups, and
# shape() FAILS on a key that is in none, so a key added to the record later
# forces a decision here instead of being swept into the comparison or out of it.
RUN_KEYS = ('baked', 'bytes', 'lineCount', 'exe', 'exeBytes', 'version',
            'switches', 'switchTokens')
"""per-RUN bookkeeping: the clock, the file's own size and line count, which
exe wrote it, and the command line. The command line is not ignored -- it is
the ONE field these modes ask about directly, by digest, and `switchTokens` is
that same digest spelled out."""

TARGET_KEYS = ('target', 'census', 'hashes', 'endFiles', 'endBytes')
"""what a bake TARGET decides. The FO4CS target names itself `fo4cs`, prints
six more census rows, carries four more corpus digests, and leaves a different
count of files and bytes on disk. None of that is an output row, and across the
two targets every one of them differs by design, so `--fo4cs-vs-stock` drops
them. `keep` and `same` -- both sides on the same target -- still compare them."""

SHAPE_KEYS = ('worldspace', 'worldEdid', 'dim', 'region', 'loadOrder', 'alg',
              'plugins', 'resources', 'chunks', 'unknown')
"""the bake and its outputs: which worldspace, which region at which dim, the
load order, the algorithm knobs, the plugins and resources that fed it, and the
chunk rows themselves. This is what "the two records name the same outputs"
means, and it is what both check sentences claim to be comparing."""


def shape(doc, drop_target, ws='Commonwealth'):
    """the part of a ledger two bakes must agree on, as a comparable string

    Returns (text, unaccounted). `unaccounted` is every key the reader returned
    that none of the three groups names; a caller that finds one must go red,
    because this file cannot know whether it was supposed to match."""
    d = strip_digests(canon_doc(doc, ws))
    drop = set(RUN_KEYS) | (set(TARGET_KEYS) if drop_target else set())
    unaccounted = sorted(k for k in d
                         if k not in drop and k not in SHAPE_KEYS
                         and k not in TARGET_KEYS and k not in RUN_KEYS)
    for k in drop:
        d.pop(k, None)
    return json.dumps(d, sort_keys=True), unaccounted


def without_bto(doc):'''

# --------------------------------------------------------------- 3. the modes
OLD_MODES = """    out_a = [o for c in a.get('chunks', []) for o in c.get('out', [])]
    out_b = [o for c in b.get('chunks', []) for o in c.get('out', [])]
    ca = canon_doc(a2)
    cb = canon_doc(b2)
"""
NEW_MODES = """    out_a = [o for c in a.get('chunks', []) for o in c.get('out', [])]
    out_b = [o for c in b.get('chunks', []) for o in c.get('out', [])]
    ca, ua = shape(a2, fo4cs_vs_stock)
    cb, ub = shape(b2, fo4cs_vs_stock)
    if ua or ub:
        check('every field of the record is one this comparison has decided about '
              '(unaccounted: %s)' % ', '.join(sorted(set(ua) | set(ub))), False)
"""

OLD_SAME = """              json.dumps(strip_digests(ca), sort_keys=True)
              == json.dumps(strip_digests(cb), sort_keys=True)
              and explained(out_a, out_b, root_a, root_b))
        check('and the command-line digest did NOT move, because the command line did not '"""
NEW_SAME = """              ca == cb and explained(out_a, out_b, root_a, root_b))
        check('and the command-line digest did NOT move, because the command line did not '"""

OLD_KEEP = """              json.dumps(strip_digests(ca), sort_keys=True)
              == json.dumps(strip_digests(cb), sort_keys=True)
              and explained(out_a, out_b, root_a, root_b))
        check('and that digest DID move, so an --incremental run cannot reuse the other bake',"""
NEW_KEEP = """              ca == cb and explained(out_a, out_b, root_a, root_b))
        check('and that digest DID move, so an --incremental run cannot reuse the other bake',"""


def main():
    s = io.open(P, encoding='utf-8', newline='').read()
    if 'SHAPE_KEYS' in s:
        print('ABORT: the tool already carries shape()')
        return 1
    edits = [(OLD_CANON, NEW_CANON), (OLD_STRIP_TAIL, NEW_SHAPE),
             (OLD_MODES, NEW_MODES), (OLD_SAME, NEW_SAME), (OLD_KEEP, NEW_KEEP)]
    for old, _ in edits:
        if s.count(old) != 1:
            print('ABORT: an anchor appears %d times: %r' % (s.count(old), old[:60]))
            return 1
    for old, new in edits:
        s = s.replace(old, new, 1)
    if s.count('\r'):
        print('ABORT: CR in the result')
        return 1
    compile(s, P, 'exec')
    d = os.path.dirname(P)
    f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d,
                                    delete=False, suffix='.tmp')
    f.write(s)
    f.close()
    os.replace(f.name, P)
    print('lodgen_btofree_ledger.py: %d bytes, CR %d, compiles' % (len(s), s.count('\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
