"""GATEFIX1: lodgen_btofree.sh re-pinned to a rung from after BTOFREE1.

* RUNG defaults to release/NifSkope.before_gatefix1.exe (b2f3073, sha1 dca43d83).
* The rung is asked for what it knows, not assumed to predate everything: its
  FO4CS bake gets `--keep-bto` when it accepts it (a rung from after 2026-09-16
  drops its chunks by default, and leg (a)'s refuter needs a tree that HAS them),
  and both sides get the pre-2026-09-17 defaults by name when the rung accepts
  them (only the exe under test did before, because the old rung could not).
* Leg (b)'s ledger question follows: against a rung that also kept its chunks,
  the command lines are the same, so the ledger mode is `same`; against an old
  rung it stays `keep`.
* The generator word (lane VTFIX1): every chunk's inputs digest starts with the
  sha1 of the exe that baked it, so two different exes never share one. The
  harness says which case it is (`--generators-differ`) and the ledger checker
  then CHECKS that every chunk's inputs moved instead of demanding they did not.
"""
P = 'E:/Projects/NifskopeWWE-gatefix1/tests/spells/lodgen_btofree.sh'
with open(P, 'rb') as fh:
	src = fh.read()
cr0 = src.count(b'\r')


def rep(s, old, new):
	n = s.count(old)
	assert n == 1, (n, old[:70])
	return s.replace(old, new)


src = rep(src, b'''#   RUNG=release/NifSkope.before_btofree1.exe  the exe the bytes are pinned to
''', b'''#   RUNG=release/NifSkope.before_gatefix1.exe  the exe the bytes are pinned to
#        (b2f3073, sha1 dca43d83; before_btofree1 still works, see bake())
''')

src = rep(src, b'''RUNG="${RUNG:-$ROOT/release/NifSkope.before_btofree1.exe}"
''', b'''# RE-PINNED 2026-09-24 (lane GATEFIX1). The pin was before_btofree1 (09-16) and
# ruled changes since then moved four of its files -- the native .lodo/.lodi, the
# chunk .BTO and the cover sheet -- so leg (a) and (c) went red on every exe with
# nothing wrong in the drop itself. Each move is named in
# scratchpad/gatefix1_20260924/DONE.md, rung by rung. A rung from after BTOFREE1
# is baked with --keep-bto (bake() below), which leg (b) proved on 09-16 is the
# old bytes exactly.
RUNG="${RUNG:-$ROOT/release/NifSkope.before_gatefix1.exe}"
''')

src = rep(src, b'''	local exe="$1" name="$2"; shift 2
	local stock=0
	# bungo 2026-09-17, "Authored LODs only": the ladder and the near library ship
	# OFF. The rung exes predate that and cannot take the switch, so the exe under
	# test is asked for the rung's old default by name and the bytes stay comparable.
	local eq=""
	[ "$exe" != "$RUNG" ] && eq="--library near --native-ladder"
	if [ "${1:-}" = "--stock" ]; then stock=1; shift; fi
	mkdir -p "$WA/$name" "$WA/$name/tex" "$WA/$name/nat"
	local nat=""
	[ "$stock" = "0" ] && nat="--native $WA/$name/nat"
	# shellcheck disable=SC2086
	"$exe" -no-gui lodgen "$ESM" --worldspace 3C \\
		--terrain-region $REGION --dim "$DIM" --data-root "$DATA" \\
		--out-dir "$WA/$name" --tex-dir "$WA/$name/tex" $nat \\
		--cover --arrays --road-detail 1 $eq "$@" \\
		> "$W/$name.log" 2>&1
	local rc=$?
	echo "  $name: rc=$rc, $(find "$W/$name" -type f | wc -l) file(s)"
	return $rc
}
''', b'''	local exe="$1" name="$2"; shift 2
	local stock=0
	if [ "${1:-}" = "--stock" ]; then stock=1; shift; fi
	# bungo 2026-09-17, "Authored LODs only": the ladder and the near library ship
	# OFF. The exe under test is always asked for the old default by name, so its
	# bytes stay comparable with a rung from before that ruling, which cannot take
	# the switches. A RUNG THAT CAN is asked the same thing, and a rung from after
	# BTOFREE1 also gets --keep-bto on its FO4CS bake: it drops its chunks by
	# default, and leg (a) compares against a tree that HAS them (its refuter says
	# so). What the rung was actually given is written to <name>.flags and printed;
	# an option the rung refuses is taken off and the bake run again.
	local eqs=("--library near --native-ladder")
	if [ "$exe" = "$RUNG" ]; then
		if [ "$stock" = "0" ]; then
			eqs=("--library near --native-ladder --keep-bto" "--keep-bto" "--library near --native-ladder" "")
		else
			eqs=("--library near --native-ladder" "")
		fi
	fi
	local eq rc
	for eq in "${eqs[@]}"; do
		rm -rf "$W/$name"
		mkdir -p "$WA/$name" "$WA/$name/tex" "$WA/$name/nat"
		local nat=""
		[ "$stock" = "0" ] && nat="--native $WA/$name/nat"
		# shellcheck disable=SC2086
		"$exe" -no-gui lodgen "$ESM" --worldspace 3C \\
			--terrain-region $REGION --dim "$DIM" --data-root "$DATA" \\
			--out-dir "$WA/$name" --tex-dir "$WA/$name/tex" $nat \\
			--cover --arrays --road-detail 1 $eq "$@" \\
			> "$W/$name.log" 2>&1
		rc=$?
		grep -q "error: unknown option" "$W/$name.log" || break
	done
	echo "$eq" > "$W/$name.flags"
	echo "  $name: rc=$rc, $(find "$W/$name" -type f | wc -l) file(s), given [${eq}${eq:+ }$*]"
	return $rc
}
''')

src = rep(src, b'''bake "$NS" drop        || bad "the FO4CS default bake ran"
''', b'''# what the rung's FO4CS bake was given decides two questions below
RUNG_KEPT=0
[ "$HAVE_RUNG" = "1" ] && grep -q -- "--keep-bto" "$W/rung_native.flags" 2>/dev/null && RUNG_KEPT=1
# THE GENERATOR WORD (lane VTFIX1, 2026-09-24): each chunk's inputs digest in the
# ledger starts with the sha1 of the exe that baked it, so two different exes
# can never record the same inputs. The ledger checker is told which case this
# is, and then checks that the word moved them rather than excusing anything.
GENFLAG=""
if [ "$HAVE_RUNG" = "1" ] && [ "$(sha1sum < "$NS" | cut -c1-40)" != "$(sha1sum < "$RUNG" | cut -c1-40)" ]; then
	GENFLAG="--generators-differ"
fi
echo "  rung kept its chunks: $RUNG_KEPT; generators: ${GENFLAG:-the same exe bytes}"
bake "$NS" drop        || bad "the FO4CS default bake ran"
''')

# leg (a) ledger call
src = rep(src, b'''	elif [ -n "$RREC" ] && [ -n "$DREC" ] && "$PY" "$LEDGER" drop "$RREC" "$DREC" \\
		"$W/rung_native" "$W/drop"; then''', b'''	elif [ -n "$RREC" ] && [ -n "$DREC" ] && "$PY" "$LEDGER" drop "$RREC" "$DREC" \\
		"$W/rung_native" "$W/drop" $GENFLAG; then''')

# leg (b): keep vs same
src = rep(src, b'''	elif [ -n "$RREC" ] && [ -n "$KREC" ] && "$PY" "$LEDGER" keep "$RREC" "$KREC" \\
		"$W/rung_native" "$W/keep"; then
		note "(b) the ledger differs only in the command-line digest, and that digest moved"
	else bad "(b) the ledger differs only in the command-line digest, and that digest moved"; fi''',
b'''	elif [ "$RUNG_KEPT" = "1" ]; then
		# the rung was given --keep-bto too, so the two command lines are the
		# same and the question is `same`: the same rows, the digest unmoved
		if [ -n "$RREC" ] && [ -n "$KREC" ] && "$PY" "$LEDGER" same "$RREC" "$KREC" \\
			"$W/rung_native" "$W/keep" $GENFLAG; then
			note "(b) the ledger names the rung's --keep-bto outputs, digest for digest, on the same command line"
		else bad "(b) the ledger names the rung's --keep-bto outputs, digest for digest, on the same command line"; fi
	elif [ -n "$RREC" ] && [ -n "$KREC" ] && "$PY" "$LEDGER" keep "$RREC" "$KREC" \\
		"$W/rung_native" "$W/keep" $GENFLAG; then
		note "(b) the ledger differs only in the command-line digest, and that digest moved"
	else bad "(b) the ledger differs only in the command-line digest, and that digest moved"; fi''')

# leg (c)
src = rep(src, b'''		if "$PY" "$LEDGER" same "$SRREC" "$SREC" "$W/rung_stock" "$W/stock"; then''',
b'''		if "$PY" "$LEDGER" same "$SRREC" "$SREC" "$W/rung_stock" "$W/stock" $GENFLAG; then''')

assert src.count(b'\r') == cr0
with open(P + '.new', 'wb') as fh:
	fh.write(src)
print('patched')
