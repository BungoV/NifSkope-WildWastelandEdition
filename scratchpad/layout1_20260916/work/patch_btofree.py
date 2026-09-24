"""Finish the re-base of lodgen_btofree.sh + its ledger tool (lane LAYOUT1).

Written as a script rather than typed as edits because every anchor below has
to match EXACTLY once; the script refuses instead of half-applying.
"""
import io
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def patch(path, pairs):
    b = io.open(ROOT + path, 'rb').read()
    assert b.count(b'\r\n') == 0, (path, 'CRLF')
    for old, new in pairs:
        n = b.count(old)
        assert n == 1, (path, n, old[:60])
        b = b.replace(old, new, 1)
    io.open(ROOT + path, 'wb').write(b)
    print('patched', path, len(b), 'bytes,', b.count(b'\n'), 'LF')


SH = 'tests/spells/lodgen_btofree.sh'

# 1. the relpath map, right after pairdir()
old1 = b"""pairdir () {
	if [ -d "$1/FO4CSLOD/Commonwealth" ]; then echo "$1/FO4CSLOD/Commonwealth"
	else echo "$1"; fi
}
"""
new1 = b"""pairdir () {
	if [ -d "$1/FO4CSLOD/Commonwealth" ]; then echo "$1/FO4CSLOD/Commonwealth"
	else echo "$1"; fi
}

# WHERE A RUNG FILE IS NOW (lane LAYOUT1, 2026-09-16).  The tree comparisons
# below pair a file baked by the RUNG exe with the same file baked by this one,
# and after the move the two have different relative paths.  This is the map,
# and it is written for THIS harness's own switches -- `--out-dir $WA/<name>`,
# `--tex-dir $WA/<name>/tex`, `--native $WA/<name>/nat` -- because a file's
# relative path depends on which folder the operator named, while the game-path
# rewrite in lodgen_layout_diff.py does not.  A name this map does not know
# stays as it is and shows up as "only in ..." rather than passing quietly.
relmap () {
	echo "$1" | sed \\
		-e 's|^tex/Objects/|FO4CSLOD/Commonwealth/Objects/|' \\
		-e 's|^Terrain/Commonwealth[.]|FO4CSLOD/Commonwealth/Commonwealth.|' \\
		-e 's|^Textures/Lodgen/Aggregate/Commonwealth/|FO4CSLOD/Commonwealth/Aggregate/|' \\
		-e 's|^Textures/Lodgen/Cards/|FO4CSLOD/Cards/|' \\
		-e 's|^\\(Commonwealth[.].*[.]BTO[.]manifest[.]txt\\)$|FO4CSLOD/Commonwealth/\\1|' \\
		-e 's|^nat/\\(Commonwealth[.]lod[oi]\\)$|nat/FO4CSLOD/Commonwealth/\\1|'
}
"""

# 2. treecmp: map the A-side relpath, and let the path rewrite explain a diff
old2 = b"""	TC_SAME=0; TC_DIFF=0; TC_ONLYA=0; TC_ONLYB=0; TC_LIST=""
	local rel
	for rel in $(cd "$a" && find . -type f | sed 's|^\\./||' | sort); do
		case "$ex" in -) ;; *) echo "$rel" | grep -qE "$ex" && continue ;; esac
		if [ ! -f "$b/$rel" ]; then
			TC_ONLYA=$((TC_ONLYA+1)); TC_LIST="$TC_LIST
    only in $(basename "$a"): $rel"
		elif cmp -s "$a/$rel" "$b/$rel"; then
			TC_SAME=$((TC_SAME+1))
		else
			TC_DIFF=$((TC_DIFF+1)); TC_LIST="$TC_LIST
    DIFFERS: $rel ($(stat -c%s "$a/$rel") vs $(stat -c%s "$b/$rel"))"
		fi
	done
	for rel in $(cd "$b" && find . -type f | sed 's|^\\./||' | sort); do
		case "$ex" in -) ;; *) echo "$rel" | grep -qE "$ex" && continue ;; esac
		[ -f "$a/$rel" ] || { TC_ONLYB=$((TC_ONLYB+1)); TC_LIST="$TC_LIST
    only in $(basename "$b"): $rel"; }
	done
	echo "    $TC_SAME identical, $TC_DIFF differ, $TC_ONLYA only in $(basename "$a"), $TC_ONLYB only in $(basename "$b")"
"""
new2 = b"""	TC_SAME=0; TC_DIFF=0; TC_ONLYA=0; TC_ONLYB=0; TC_PATH=0; TC_LIST=""; TC_SEEN=""
	local rel m
	for rel in $(cd "$a" && find . -type f | sed 's|^\\./||' | sort); do
		case "$ex" in -) ;; *) echo "$rel" | grep -qE "$ex" && continue ;; esac
		m="$(relmap "$rel")"
		TC_SEEN="$TC_SEEN $m"
		if [ ! -f "$b/$m" ]; then
			TC_ONLYA=$((TC_ONLYA+1)); TC_LIST="$TC_LIST
    only in $(basename "$a"): $rel"
		elif cmp -s "$a/$rel" "$b/$m"; then
			TC_SAME=$((TC_SAME+1))
		elif "$PY" "$ROOT/tests/spells/lodgen_layout_diff.py" --ws Commonwealth \\
			--quiet --pair "$a/$rel" "$b/$m" > /dev/null 2>&1; then
			# THE FILE CARRIES A GAME PATH (lane LAYOUT1): the same tool the
			# layout gate's leg (b) uses says these two differ ONLY in the
			# path strings the move rewrote, and in the length words those
			# strings force.  Counted apart from `identical` so the number
			# is never quietly folded into it.
			TC_PATH=$((TC_PATH+1)); TC_LIST="$TC_LIST
    path-rewrite only: $rel -> $m"
		else
			TC_DIFF=$((TC_DIFF+1)); TC_LIST="$TC_LIST
    DIFFERS: $rel -> $m ($(stat -c%s "$a/$rel") vs $(stat -c%s "$b/$m"))"
		fi
	done
	for rel in $(cd "$b" && find . -type f | sed 's|^\\./||' | sort); do
		case "$ex" in -) ;; *) echo "$rel" | grep -qE "$ex" && continue ;; esac
		case " $TC_SEEN " in *" $rel "*) continue ;; esac
		[ -f "$a/$rel" ] || { TC_ONLYB=$((TC_ONLYB+1)); TC_LIST="$TC_LIST
    only in $(basename "$b"): $rel"; }
	done
	echo "    $TC_SAME identical, $TC_PATH path-rewrite only, $TC_DIFF differ, $TC_ONLYA only in $(basename "$a"), $TC_ONLYB only in $(basename "$b")"
"""

# 3. the two verdicts that read TC_*, and the ledger calls that now carry roots
old3 = b"""		note "(a) every other file is byte-identical to the rung's ($TC_SAME files)"
"""
new3 = b"""		note "(a) every other file is byte-identical to the rung's ($TC_SAME files, $TC_PATH more equal after the path rewrite)"
"""

old4 = b"""	if "$PY" "$LEDGER" drop "$W/rung_native/Commonwealth.lodb" "$W/drop/Commonwealth.lodb"; then
"""
new4 = b"""	if "$PY" "$LEDGER" drop "$W/rung_native/Commonwealth.lodb" "$W/drop/Commonwealth.lodb" \\
		"$W/rung_native" "$W/drop"; then
"""

old5 = b"""		note "(b) every output file is byte-identical to the rung's ($TC_SAME files, the chunks included)"
"""
new5 = b"""		note "(b) every output file is byte-identical to the rung's ($TC_SAME files, the chunks included, $TC_PATH more equal after the path rewrite)"
"""

old6 = b"""	if "$PY" "$LEDGER" keep "$W/rung_native/Commonwealth.lodb" "$W/keep/Commonwealth.lodb"; then
"""
new6 = b"""	if "$PY" "$LEDGER" keep "$W/rung_native/Commonwealth.lodb" "$W/keep/Commonwealth.lodb" \\
		"$W/rung_native" "$W/keep"; then
"""

# 4. the census clause grew a layout sentence, so the pattern stops anchoring
#    at the end of the line
old7 = b"""	"bto built in the mod folder, "*", 0 dropped, 0 bytes freed") note "(d) --keep-bto reads 0 dropped, 0 bytes freed" ;;
"""
new7 = b"""	"bto built in the mod folder, "*", 0 dropped, 0 bytes freed"*) note "(d) --keep-bto reads 0 dropped, 0 bytes freed" ;;
"""

patch(SH, [(old1, new1), (old2, new2), (old3, new3), (old4, new4),
           (old5, new5), (old6, new6), (old7, new7)])

# ---------------------------------------------------------------------------
PY = 'tests/spells/lodgen_btofree_ledger.py'

oldp1 = b'''  usage: lodgen_btofree_ledger.py keep <a.lodb> <b.lodb>
         lodgen_btofree_ledger.py drop <rung.lodb> <drop.lodb>
'''
newp1 = b'''  usage: lodgen_btofree_ledger.py keep <a.lodb> <b.lodb> [<a-root> <b-root>]
         lodgen_btofree_ledger.py drop <rung.lodb> <drop.lodb> [<a-root> <b-root>]

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
'''

oldp2 = b'''import json
import sys
'''
newp2 = b'''import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lodgen_layout_diff as LD           # noqa: E402  the ONE path rewriter


def canon(row, ws='Commonwealth'):
    """a row (`<relpath> <sha1>`) with the new root taken off its path, so the
    two spellings of the same output compare as one row"""
    parts = row.split(' ')
    p = parts[0].replace('\\\\', '/')
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
        ch['out'] = [canon(o, ws) for o in ch.get('out', [])]
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
'''

oldp3 = b'''def main(argv):
    if len(argv) != 3 or argv[0] not in ('keep', 'drop'):
        raise SystemExit(__doc__)
    mode, pa, pb = argv
    a, b = load(pa), load(pb)
'''
newp3 = b'''def main(argv):
    if len(argv) not in (3, 5) or argv[0] not in ('keep', 'drop'):
        raise SystemExit(__doc__)
    mode, pa, pb = argv[:3]
    root_a, root_b = (argv[3], argv[4]) if len(argv) == 5 else (None, None)
    a, b = load(pa), load(pb)
'''

oldp4 = b'''    if mode == 'keep':
        check('the second ledger differs from the first ONLY in the command-line digest '
              '(%s -> %s)' % (str(sa)[:12], str(sb)[:12]),
              json.dumps(a2, sort_keys=True) == json.dumps(b2, sort_keys=True))
'''
newp4 = b'''    rows_a = [o for c in a.get('chunks', []) for o in c.get('out', [])]
    rows_b = [o for c in b.get('chunks', []) for o in c.get('out', [])]
    ca = canon_doc(a2)
    cb = canon_doc(b2)

    if mode == 'keep':
        check('the second ledger differs from the first ONLY in the command-line digest '
              '(%s -> %s)' % (str(sa)[:12], str(sb)[:12]),
              json.dumps(strip_digests(ca), sort_keys=True)
              == json.dumps(strip_digests(cb), sort_keys=True)
              and explained(rows_a, rows_b, root_a, root_b))
'''

oldp5 = b'''        check('every other recorded file, digest for digest, is what the rung recorded',
              json.dumps(without_bto(a2), sort_keys=True) == json.dumps(b2, sort_keys=True))
'''
newp5 = b'''        check('every other recorded file, digest for digest, is what the rung recorded '
              '(or differs by the path rewrite, proved on the files themselves)',
              json.dumps(strip_digests(canon_doc(without_bto(a2))), sort_keys=True)
              == json.dumps(strip_digests(cb), sort_keys=True)
              and explained([r for r in rows_a
                             if not r.split(' ')[0].upper().endswith('.BTO')],
                            rows_b, root_a, root_b))
'''

oldp6 = b'''        check('the manifest sidecar is still recorded, with the digest it had before the move',
              len(man_b) > 0 and sorted(man_a) == sorted(man_b))
'''
newp6 = b'''        check('the manifest sidecar is still recorded, at its new path, with a digest '
              'the path rewrite accounts for',
              len(man_b) > 0
              and sorted(canon(o) for o in man_a) == sorted(canon(o) for o in man_b)
              and explained(man_a, man_b, root_a, root_b))
'''

oldp7 = b'''def without_bto(doc):
'''
newp7 = b'''def strip_digests(doc):
    """the ledger with every output row's SHA1 taken off, so the structural
    comparison is about WHICH files were recorded; the digests are then checked
    row by row by explained(), which can excuse one only with the file in hand"""
    d = json.loads(json.dumps(doc))
    for ch in d.get('chunks', []):
        ch['out'] = [o.split(' ')[0] for o in ch.get('out', [])]
    return d


def without_bto(doc):
'''

patch(PY, [(oldp1, newp1), (oldp2, newp2), (oldp3, newp3), (oldp4, newp4),
           (oldp5, newp5), (oldp6, newp6), (oldp7, newp7)])
