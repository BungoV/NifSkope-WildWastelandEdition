"""GATEFIX1: lodgen_btofree.sh -- the .lodj exclusion narrows itself to a rung that
predates the native cache; a rung that writes one has it compared byte for byte."""
P = 'E:/Projects/NifskopeWWE-gatefix1/tests/spells/lodgen_btofree.sh'
with open(P, 'rb') as fh:
    src = fh.read()
cr0 = src.count(b'\r')
s = src.decode('utf-8')

reps = [
(
"""	echo "    everything except the chunks, the ledger and the native cache, against the rung:"
""",
"""	# THE EXCLUSION IS NARROWED BY THE RUNG ITSELF (lane GATEFIX1, 2026-09-24).
	# A rung from before INCR1 writes no .lodj, and the cache is left out of the
	# sweep as a NEW output, checked by name below. A rung that DOES write one
	# (every rung from before_msnfix on, the pin since GATEFIX1 included) has it
	# swept like any other file, byte for byte -- the check below said the
	# exclusion "would now hide a real difference", and it was right.
	RLODJ="$(find "$W/rung_native" -name "*.lodj" | wc -l)"
	LODJ_EX='|[.]lodj$'; [ "$RLODJ" -gt 0 ] && LODJ_EX=''
	if [ -n "$LODJ_EX" ]; then
		echo "    everything except the chunks, the ledger and the native cache, against the rung:"
	else
		echo "    everything except the chunks and the ledger, the native cache INCLUDED, against the rung:"
	fi
"""),
(
"""	treecmp "$W/rung_native" "$W/drop" '[.]BTO$|[.]lodb$|[.]lodj$'
""",
"""	treecmp "$W/rung_native" "$W/drop" "[.]BTO\\$|[.]lodb\\$$LODJ_EX"
"""),
(
"""	RLODJ="$(find "$W/rung_native" -name "*.lodj" | wc -l)"
	echo "    native cache files: this exe $NLODJ, rung $RLODJ"
""",
"""	echo "    native cache files: this exe $NLODJ, rung $RLODJ"
"""),
(
"""	[ "$RLODJ" -eq 0 ] && note "(a) and the rung wrote none, so the exclusion covers a NEW output and hides no change" \\
		|| bad "(a) and the rung wrote none (found $RLODJ .lodj: the exclusion would now hide a real difference)"
""",
"""	if [ -n "$LODJ_EX" ]; then
		[ "$RLODJ" -eq 0 ] && note "(a) and the rung wrote none, so the exclusion covers a NEW output and hides no change" \\
			|| bad "(a) and the rung wrote none (found $RLODJ .lodj: the exclusion would now hide a real difference)"
	else
		[ "$RLODJ" -eq "$NLODJ" ] && note "(a) and the rung wrote the same count ($RLODJ .lodj), so the sweep above compared them byte for byte, not excluded" \\
			|| bad "(a) and the rung wrote the same count of native cache files (rung $RLODJ, this exe $NLODJ)"
	fi
"""),
(
"""	treecmp "$W/rung_native" "$W/keep" '[.]lodb$|[.]lodj$'
""",
"""	# (GATEFIX1) and, as in leg (a), swept byte for byte when the rung writes one.
	treecmp "$W/rung_native" "$W/keep" "[.]lodb\\$$LODJ_EX"
"""),
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
