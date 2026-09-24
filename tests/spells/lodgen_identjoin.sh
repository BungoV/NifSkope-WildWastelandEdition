#!/usr/bin/env bash
# lodgen_identjoin.sh -- the `.lodi` v7 GROUP table and the rule that builds it.
#
# WHAT THIS GATE IS FOR. bungo dropped the baked-horizon far-shadow route on
# 2026-09-19 and ruled the far shadow back onto the IDENTITY: the shadow map is
# keyed on the `.lodi` group id, and a caster never darkens a receiver of its
# own group. That makes the GROUP the whole contract, and a bake that groups
# badly is now a bake that shadows badly -- a building split into forty groups
# shadows itself forty times, and two buildings welded across a street stop
# shadowing each other at all.
#
# The same ruling changed the rule. The default is now PROXIMITY: every NON-TREE
# placement with a drawn LOD mesh joins at a MESH-TO-MESH gap of 64 u
# (`--identity-join-gap`), measured over dense sample points -- level-0 vertices
# plus three edge midpoints plus the centroid of every triangle, placed. The way
# back is `--identity-join legacy`, the pre-ruling rule: an `architecture` path
# component and a WORLD AXIS-ALIGNED BOX gap of 16 u.
#
# THE LEGS
#
#   G1 THE CENSUS NAMES THE RULE IT RAN. `native-identity-join:` says PROXIMITY
#      or LEGACY with its cost, on both bake logs. A bake that silently ran the
#      other rule would otherwise be indistinguishable from a bake that ran this
#      one badly.
#
#   G2 THE COUNTS, out of the FILES and not out of the logs. Chunk 4.4.-12:
#      588 groups under legacy, 167 under the ruled rule (lane IDENTPROX,
#      `scratchpad/identprox_20260919/RECOMMENDATION.txt`). Singletons 468 -> 62.
#
#   G3 THE RED CONTROL IS THE LEGACY SWITCH. `--identity-join legacy` has to put
#      588 back exactly. A rule whose way back does not reproduce the shipped
#      number is a rule that changed two things.
#
#   G4 THE LAYER CROSS-CHECK. The bake cannot mark its own homework, so the
#      groups are crossed with the CREATION KIT's own `XLYR` layers, read from
#      `Fallout4.esm` by `lodgen_identjoin_layers.py`, which shares no code with
#      the bake. `DN135_GwinnettExt` -- one building -- falls in 205 groups under
#      legacy and 6 under the ruled rule.
#
#   G5 THE SPANNING LEG, the refuter for the OTHER failure. A group holding
#      placements of more than one layer may be welding two buildings. The count
#      is printed for both files and the worst groups are NAMED so a person can
#      open one in the Creation Kit. `SPAN=<n>` turns it into a ceiling; with
#      `SPAN=0` (the default) it MEASURES and the report quotes it.
#
# FIXTURES. Two bakes of chunk 4.4.-12 at the DEFAULT library -- one plain, one
# `--identity-join legacy` -- under `scratchpad/horizonout_20260919/join/`. The
# library matters: `--library near` places a different set and gives different
# counts (584 -> 158), which is not a defect and is not comparable to the numbers
# above. When a fixture is absent the gate SKIPS with the path NAMED and does not
# pass.
#
# ONE NifSkope at a time, second monitor, never a desktop capture.
#
# USAGE
#   bash tests/spells/lodgen_identjoin.sh
#   PROX=<prefix> LEG=<prefix> GPROX=167 GLEG=588 bash tests/spells/lodgen_identjoin.sh

set -u

. "$(dirname "$0")/_harness.sh" 2>/dev/null || true
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"

LANE="$ROOT/scratchpad/horizonout_20260919/join"
PROX="${PROX:-$LANE/prox/nat/FO4CSLOD/Commonwealth/Commonwealth}"
LEG="${LEG:-$LANE/legacy/nat/FO4CSLOD/Commonwealth/Commonwealth}"
PROXLOG="${PROXLOG:-$LANE/prox/bake.log}"
LEGLOG="${LEGLOG:-$LANE/legacy/bake.log}"
OUT="${OUT:-$ROOT/scratchpad/lodgen_identjoin_gate}"

GPROX="${GPROX:-167}"       # groups, the ruled rule
GLEG="${GLEG:-588}"         # groups, the way back -- and the shipped number
SPROX="${SPROX:-62}"        # singletons, the ruled rule
SLEG="${SLEG:-468}"         # singletons, the way back
LAYER="${LAYER:-DN135_GwinnettExt}"
LPROX="${LPROX:-6}"         # that layer's distinct groups, the ruled rule
LLEG="${LLEG:-205}"         # that layer's distinct groups, the way back
SPAN="${SPAN:-0}"           # 0 = measure and print; >0 = a ceiling the gate holds

pass=0; fail=0
ck () {  # ck <name> <rc> <detail>
	if [ "$2" -eq 0 ]; then echo "  ok   $1 -- $3"; pass=$((pass+1));
	else echo "  FAIL $1 -- $3"; fail=$((fail+1)); fi
}

[ -x "$EXE" ] || { echo "SKIP: no NifSkope.exe at $EXE"; exit 2; }
for f in "$PROX.lodi" "$LEG.lodi" "$ESM" \
         "$ROOT/tests/spells/lodgen_identjoin_layers.py"; do
	[ -f "$f" ] || { echo "SKIP: fixture missing -- $f"; exit 2; }
done
if tasklist 2>/dev/null | grep -qi -E "Fallout4"; then
	echo "SKIP: Fallout4 is up -- no exe runs while the game holds the files"; exit 2
fi

if ! type winpath >/dev/null 2>&1; then
	winpath() {
		case "$1" in
			/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
			*) printf '%s' "$1" ;;
		esac
	}
fi
mkdir -p "$OUT"; rm -f "$OUT"/*.log

echo "exe   $EXE  ($(stat -c '%y %s B' "$EXE" 2>/dev/null))"
echo "prox  $PROX.lodi  ($(stat -c '%s B' "$PROX.lodi" 2>/dev/null))"
echo "leg   $LEG.lodi  ($(stat -c '%s B' "$LEG.lodi" 2>/dev/null))"

# --------------------------------------------------------------- G1
echo "== G1 the census names the rule each bake ran"
for pair in "PROXIMITY:$PROXLOG" "LEGACY:$LEGLOG"; do
	want="${pair%%:*}"; log="${pair#*:}"
	if [ -f "$log" ]; then
		line="$(tr -d '\r' < "$log" | grep -m1 "native-identity-join: ")"
		echo "  $line" | cut -c1-200
		printf '%s' "$line" | grep -q "$want"
		ck "the $want bake's census says $want" $? "$log"
	else
		echo "  (no bake log at $log)"
		ck "the $want bake's census says $want" 1 "no log"
	fi
done

# --------------------------------------------------------------- G2 + G3 + G4 + G5
echo "== G2/G3/G4/G5 the counts, read back out of the FILES"
"$PY" "$ROOT/tests/spells/lodgen_identjoin_layers.py" \
	"$(winpath "$LEG.lodi")" "$(winpath "$PROX.lodi")" \
	--esm "$ESM" --layer "$LAYER" --top 8 > "$OUT/layers.log" 2>&1
lrc=$?
cat "$OUT/layers.log" | cut -c1-160
ck "the layer cross-check ran" $lrc "$OUT/layers.log"

# Each file prints one `placements N, groups G, singletons S, largest L` line,
# in the order the files were given: legacy first, proximity second.
read_n () {  # read_n <which 1|2> <field regex>
	tr -d '\r' < "$OUT/layers.log" | grep -oE "$2 [0-9]+" | sed -n "${1}p" \
		| grep -oE '[0-9]+'
}
gleg="$(read_n 1 'groups')";  gprox="$(read_n 2 'groups')"
sleg="$(read_n 1 'singletons')"; sprox="$(read_n 2 'singletons')"

[ "$gleg" = "$GLEG" ];   ck "the way back reproduces the shipped group count" $? "legacy groups $gleg, want $GLEG"
[ "$gprox" = "$GPROX" ]; ck "the ruled rule's group count"                    $? "proximity groups $gprox, want $GPROX"
[ "$sleg" = "$SLEG" ];   ck "the way back's singleton count"                  $? "legacy singletons $sleg, want $SLEG"
[ "$sprox" = "$SPROX" ]; ck "the ruled rule's singleton count"                $? "proximity singletons $sprox, want $SPROX"
[ "$gprox" -lt "$gleg" ] 2>/dev/null
ck "the join JOINS (a rule that changed nothing would tie)" $? "$gleg -> $gprox"

lleg="$(tr -d '\r' < "$OUT/layers.log" | grep -E "layer +$LAYER" | sed -n 1p | grep -oE 'in [0-9]+ group' | grep -oE '[0-9]+')"
lprox="$(tr -d '\r' < "$OUT/layers.log" | grep -E "layer +$LAYER" | sed -n 2p | grep -oE 'in [0-9]+ group' | grep -oE '[0-9]+')"
[ "$lleg" = "$LLEG" ];   ck "$LAYER under the way back"  $? "$lleg group(s), want $LLEG"
[ "$lprox" = "$LPROX" ]; ck "$LAYER under the ruled rule" $? "$lprox group(s), want $LPROX"

spanprox="$(tr -d '\r' < "$OUT/layers.log" | grep -E 'groups spanning more than one layer' | sed -n 2p | grep -oE ': [0-9]+' | grep -oE '[0-9]+')"
if [ "$SPAN" = "0" ]; then
	echo "  MEASURED, not a floor: $spanprox proximity group(s) span more than one layer"
	echo "  (set SPAN=<n> to hold a ceiling once bungo has ruled on the worst of them)"
else
	[ "${spanprox:-999999}" -le "$SPAN" ] 2>/dev/null
	ck "groups spanning more than one layer stay under the ceiling" $? "$spanprox <= $SPAN"
fi

echo
echo "$pass checks passed, $fail failed"
[ "$fail" -eq 0 ] || exit 1
