#!/usr/bin/env python3
"""Compare two `.lodb` ledgers field by field -- lane BTOFREE1, 2026-09-16.

WHY THIS EXISTS. `lodgen_btofree.sh` legs (a) and (b) claim that a bake from the
new exe writes the rung's bytes. For every output file that is true and `cmp`
says so. For the LEDGER it is true of everything EXCEPT two fields, and both
exceptions are the ledger doing its job:

  * `switches` is a digest of the command line, and `--keep-bto` is one more
    token on it. It MUST move, or an `--incremental` run would reuse chunks
    baked by a command line that asked for different files on disk. So the gate
    asks for the difference rather than excusing it.
  * a chunk's `out` list is what that bake left on disk. The default bake leaves
    no `.BTO`, so the ledger must not carry one -- digesting a file that is
    about to be deleted is the defect that once forced full rebakes.

Everything else -- the input digest, the load order, the region, the dim, the
worldspace, and every surviving output file's own sha1 -- has to be identical,
and that is what makes this a byte gate rather than a shrug.

  usage: lodgen_btofree_ledger.py keep <a.lodb> <b.lodb> [<a-root> <b-root>]
         lodgen_btofree_ledger.py drop <rung.lodb> <drop.lodb> [<a-root> <b-root>]
         lodgen_btofree_ledger.py same <a.lodb> <b.lodb> [<a-root> <b-root>]
         ... [--fo4cs-vs-stock]

`same` (lane AUDIT1, 2026-09-17) is `keep`'s other half: the SAME command line on
both sides, so the recorded outputs must match row for row AND the command-line
digest must NOT have moved. It is what leg (c) -- the stock target against the
rung, spelled identically -- can ask; `keep` cannot, because it requires the
digest to move.

THE MOVE (lane LAYOUT1, 2026-09-16).  Every FO4CS-target output now lands under
`FO4CSLOD/<ws>/`, so a row RECORDED by a rung exe and the same row recorded by
this one no longer read alike, in two ways, and both are the ledger telling the
truth rather than a difference to excuse:

  * the recorded relative path moved.  Rows are compared with that prefix taken
    off both sides, so `x.BTO.manifest.txt` and `FO4CSLOD/<ws>/x.BTO.manifest.txt`
    are the same row -- and a row that moved anywhere ELSE still fails.
  * a recorded sha1 may have moved WITH it, because some of those files carry
    the game-relative path inside them.  A changed digest is accepted only when
    the two trees are given and the file itself, put through the same rewrite
    the layout gate uses, comes out byte-equal.  Without the roots a changed
    digest is a failure, which is what lodgen_native.sh (both sides baked by
    the same exe) wants.

`keep` is the general question "these two bakes recorded the same files, and
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

Prints `ok` / `FAIL` lines and exits non-zero on any failure.
"""
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lodgen_layout_diff as LD           # noqa: E402  the ONE path rewriter
import lodb_read                         # noqa: E402  the ONE record reader


def canon(row, ws='Commonwealth'):
    """a row (`<relpath> <sha1>`) reduced to the file it names, so the two
    spellings of the same output compare as one row

    Two things move a spelling without moving the file:

      * the FO4CS root. `FO4CSLOD/<ws>/x` and `x` are the same output; the
        prefix comes off and a row that moved anywhere ELSE still fails.
      * the record's own folder (lane BAKEREC1, 2026-09-17). Paths are
        relative to the RECORD, which sits in FO4CSLOD/<ws>/ under the FO4CS
        target and in the out-dir under the stock one, so the same `.BTO` is
        `x.BTO` on one side and `../../x.BTO` on the other. The leading `../`
        hops come off for the same reason the prefix does.

    Nothing else is forgiven: this is a name reduction, not a sweep."""
    parts = row.split(' ')
    p = parts[0].replace(chr(92), '/')
    while p.startswith('../'):
        p = p[3:]
    for pre in ('FO4CSLOD/%s/' % ws, 'FO4CSLOD/'):
        if p.startswith(pre):
            p = p[len(pre):]
            break
    return ' '.join([p] + parts[1:])


def canon_doc(doc, ws="Commonwealth"):
    """the same ledger with every output row canonicalised, and -- when both
    tree roots are known -- with a digest that the path rewrite explains
    replaced by the digest it is explained BY, so the comparison that follows
    is still `==` and still fails on anything else"""
    d = json.loads(json.dumps(doc))
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


def rewritten_sha1(path, ws='Commonwealth'):
    """sha1 of <path> after the FO4CSLOD path rewrite, or None if unreadable"""
    try:
        data = open(path, 'rb').read()
    except OSError:
        return None
    out, _ = LD.rewrite(data, LD.spellings(ws))
    out, _ = LD.repack_lodm(out, data)
    return hashlib.sha1(out).hexdigest()


def explained(rows_a, rows_b, root_a, root_b, ws='Commonwealth'):
    """True when every canonical row pairs up and each digest either matches or
    is explained, file in hand, by the path rewrite.  Prints what it excused."""
    ma = dict((canon(r, ws).split(' ')[0], r) for r in rows_a)
    mb = dict((canon(r, ws).split(' ')[0], r) for r in rows_b)
    if sorted(ma) != sorted(mb):
        return False
    ok = True
    for key in sorted(ma):
        da = ma[key].split(' ')[-1]
        db = mb[key].split(' ')[-1]
        if da == db:
            continue
        if not root_a or not root_b:
            ok = False
            continue
        got = rewritten_sha1(os.path.join(root_a, ma[key].split(' ')[0]), ws)
        if got and got == db:
            print('    digest moved with the path, and the rewrite explains it: %s' % key)
        else:
            print('    digest moved and the rewrite does NOT explain it: %s' % key)
            ok = False
    return ok


def load(path):
    """the record, through the ONE reader (lane BAKEREC1, 2026-09-17)

    This used to find the first `{` in the v1 binary container and parse the
    JSON after it. The record is version 2 plain text now and there is exactly
    one parser for it, tests/spells/lodb_read.py, which every harness imports;
    the v1 keys this file uses (`chunks`, `out`, `switches`) are spelled the
    same way by it on purpose, so nothing below this line changed."""
    try:
        return lodb_read.read(path)
    except lodb_read.LodbRefused as e:
        raise SystemExit('%s' % e)


def strip_digests(doc):
    """the ledger with every output row's SHA1 taken off, so the structural
    comparison is about WHICH files were recorded; the digests are then checked
    row by row by explained(), which can excuse one only with the file in hand"""
    d = json.loads(json.dumps(doc))
    for ch in d.get('chunks', []):
        ch['out'] = [o.split(' ')[0] for o in ch.get('out', [])]
    return d


# WHAT A LEDGER COMPARISON IS ABOUT, BY NAME (lane AUDIT1, 2026-09-17).
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


def without_bto(doc):
    """the same ledger with every bare `.BTO` output row taken out"""
    d = json.loads(json.dumps(doc))
    for ch in d.get('chunks', []):
        ch['out'] = [o for o in ch.get('out', [])
                     if not o.split(' ')[0].upper().endswith('.BTO')]
    return d


def cache_rows(doc):
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
    a, b = load(pa), load(pb)
    fails = 0

    def check(what, ok):
        nonlocal fails
        print('  %s %s' % ('ok  ' if ok else 'FAIL', what))
        if not ok:
            fails += 1

    sa, sb = a.get('switches'), b.get('switches')
    a2 = dict(a); a2.pop('switches', None)
    b2 = dict(b); b2.pop('switches', None)

    if fo4cs_vs_stock:
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
    ca, ua = shape(a2, fo4cs_vs_stock)
    cb, ub = shape(b2, fo4cs_vs_stock)
    if ua or ub:
        check('every field of the record is one this comparison has decided about '
              '(unaccounted: %s)' % ', '.join(sorted(set(ua) | set(ub))), False)

    if mode == 'same':
        # THE SAME COMMAND LINE ON BOTH SIDES (lane AUDIT1, 2026-09-17), which
        # is lodgen_btofree.sh leg (c): the rung's stock bake and this exe's
        # stock bake are spelled identically, so the digest must NOT move, and
        # the recorded outputs must be the same rows with the same digests.
        # `keep` cannot ask this: it REQUIRES the digest to have moved.
        check('the two records name the same outputs, digest for digest',
              ca == cb and explained(out_a, out_b, root_a, root_b))
        check('and the command-line digest did NOT move, because the command line did not '
              '(%s == %s)' % (str(sa)[:12], str(sb)[:12]), sa == sb)
    elif mode == 'keep':
        check('the second ledger differs from the first ONLY in the command-line digest '
              '(%s -> %s)' % (str(sa)[:12], str(sb)[:12]),
              ca == cb and explained(out_a, out_b, root_a, root_b))
        check('and that digest DID move, so an --incremental run cannot reuse the other bake',
              sa != sb)
    else:
        rows_a = sum(len(c.get('out', [])) for c in a.get('chunks', []))
        rows_b = sum(len(c.get('out', [])) for c in b.get('chunks', []))
        bto_b = [o for c in b.get('chunks', []) for o in c.get('out', [])
                 if o.split(' ')[0].upper().endswith('.BTO')]
        bto_a = [o for c in a.get('chunks', []) for o in c.get('out', [])
                 if o.split(' ')[0].upper().endswith('.BTO')]
        print('    rung %d output row(s), of which %d are .BTO; default bake %d row(s), of which %d'
              % (rows_a, len(bto_a), rows_b, len(bto_b)))
        check('the rung ledger DID carry .BTO rows, so this is not a vacuous check',
              len(bto_a) > 0)
        check('the default bake records NO .BTO row (it is not an output any more)',
              len(bto_b) == 0)
        check('every other recorded file, digest for digest, is what the rung recorded '
              '(or differs by the path rewrite, proved on the files themselves)',
              json.dumps(strip_digests(canon_doc(without_bto(a2))), sort_keys=True)
              == json.dumps(strip_digests(cb), sort_keys=True)
              and explained([r for r in out_a
                             if not r.split(' ')[0].upper().endswith('.BTO')],
                            out_b, root_a, root_b))
        man_a = [o for c in a.get('chunks', []) for o in c.get('out', [])
                 if o.split(' ')[0].upper().endswith('.BTO.MANIFEST.TXT')]
        man_b = [o for c in b.get('chunks', []) for o in c.get('out', [])
                 if o.split(' ')[0].upper().endswith('.BTO.MANIFEST.TXT')]
        print('    manifest rows: rung %d, default %d' % (len(man_a), len(man_b)))
        check('the manifest sidecar is still recorded, at its new path, with a digest '
              'the path rewrite accounts for',
              len(man_b) > 0
              # THE PATHS, not the rows: a row carries its digest, and the
              # manifest's digest moved WITH the path (the file names its own
              # folder inside itself), which explained() is what settles.
              and (sorted(canon(o).split(' ')[0] for o in man_a)
                   == sorted(canon(o).split(' ')[0] for o in man_b))
              and explained(man_a, man_b, root_a, root_b))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
