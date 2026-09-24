"""AUDIT1: lodgen_native.sh check 5 -- the stale half of standing red 1.

The gate asked `keep` of a stock record and an FO4CS record. Since lane INCR1
the FO4CS target also writes a per-chunk native cache (`.lodj`) and BAKEREC1's
record records it, so the two records name different files and `keep` cannot be
asked of that pair unqualified. The flag added to lodgen_btofree_ledger.py
CHECKS that difference (FO4CS has the cache rows, stock has none) and takes
only those rows off before the row-for-row comparison.

Writes through a temp file and renames (MISTAKES 2026-09-17 16:27)."""
import io
import os

p = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_native.sh'
s = io.open(p, encoding='utf-8', newline='').read()

old = """if [ -n "$SREC" ] && [ -n "$NREC" ]; then
	if "$PY" "$ROOT/tests/spells/lodgen_btofree_ledger.py" keep \\
		"$SREC" "$NREC"; then
		note "and the two ledgers differ ONLY in the command-line digest, every recorded chunk digest alike"
	else bad "and the two ledgers differ ONLY in the command-line digest, every recorded chunk digest alike"; fi
else
	bad "and the two ledgers differ ONLY in the command-line digest (a .lodb is missing)"
fi
"""

new = """# WHAT THE TWO RECORDS MAY DISAGREE ABOUT, AND IT IS ONE THING (lane AUDIT1,
# 2026-09-17). Lane INCR1 gave the FO4CS target a per-chunk native cache,
# `<ws>.<dim>.<cx>.<cy>.lodj`, and BAKEREC1's record records it because the bake
# left it on disk. The stock target has no cache and no such row. So the two
# records legitimately name different files, `keep` went red on 2026-09-16, and
# the red was this gate rather than the bake. `--fo4cs-vs-stock` does not excuse
# the rows: it CHECKS them -- the FO4CS record must carry at least one and the
# stock record none -- and only then takes them off both sides. A bake that
# stops writing the cache still goes red here, and so does any other moved row.
if [ -n "$SREC" ] && [ -n "$NREC" ]; then
	if "$PY" "$ROOT/tests/spells/lodgen_btofree_ledger.py" keep \\
		"$SREC" "$NREC" --fo4cs-vs-stock; then
		note "and the two ledgers differ ONLY in the command-line digest and the cache rows the FO4CS target alone writes"
	else bad "and the two ledgers differ ONLY in the command-line digest and the cache rows the FO4CS target alone writes"; fi
	# THE REFUTER, in this run, on these two records: move ONE recorded digest
	# that is not a cache row and the same comparison must go red. Without it
	# the check above cannot be told from a comparison that forgave everything.
	sed '0,/^out\\t.*\\.BTR\\t/s/\\(^out\\t.*\\.BTR\\t\\)\\([0-9a-f]\\)/\\1f/' "$NREC" > "$W/native_doctored.lodb"
	if cmp -s "$NREC" "$W/native_doctored.lodb"; then
		bad "THE REFUTER: the doctored record differs from the real one (nothing was doctored)"
	elif "$PY" "$ROOT/tests/spells/lodgen_btofree_ledger.py" keep \\
		"$SREC" "$W/native_doctored.lodb" --fo4cs-vs-stock > "$W/doctored_ledger.log" 2>&1; then
		bad "THE REFUTER: one moved .BTR digest in the FO4CS record turns this check red"
	else
		note "THE REFUTER: one moved .BTR digest in the FO4CS record turns this check red"
	fi
else
	bad "and the two ledgers differ ONLY in the command-line digest (a .lodb is missing)"
fi
"""

assert s.count(old) == 1, 'check 5 block not found verbatim'
s = s.replace(old, new)
tmp = p + '.tmp'
io.open(tmp, 'w', encoding='utf-8', newline='').write(s)
os.replace(tmp, p)
b = open(p, 'rb').read()
print('patched; CR %d LF %d' % (b.count(b'\r'), b.count(b'\n')))
