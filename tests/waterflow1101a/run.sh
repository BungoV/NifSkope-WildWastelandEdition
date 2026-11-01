#!/bin/bash
# Lane FIX1101A flow (LATE ITEM 10): the water FLOW rebuild on the Commonwealth .lodl, gated offline.
#
#   The bake's water stage reads the flow OVERRIDE sidecar <ws>.waterflow.txt (bungo's verdicts + the final WATERMAP
#   proposal: body classes, dam and tributary joins, the sea-level channel split, drawn currents). Gate (gate.py):
#     A  every gated reference reach (the 8 proposal rivers, bungo's drawn Neponset bay current C1, the lower Mystic
#        and the two arms he drew into it, every proposal channel branch >= 2x as long as it is wide): mean angle
#        error of the flow vs the upstream -> downstream path < 20 deg;
#     B  zero current on every lake / swamp / calm body of the sidecar.
#   Red = TODAY's rule: the bake with --water-no-overrides (the sidecar is not read) FAILS the gate; it is byte-equal
#   to the bake with WW_LODL_WATERFLOW_RED=1 and to a bake with no sidecar at all (the bytes of before). When the
#   shipped bake is given (SHIPPED=<file.lodl>) it must FAIL too. Also: a bad sidecar line refuses the bake, two
#   default bakes are byte-equal, and the water marking tool's self-test on the new file shows the registered FAILs
#   only (F2, F5 p99; FARWATER4).
# usage: bash tests/waterflow1101a/run.sh <exe> <out dir>   (Fallout4.exe must not run; ESM= overrides the master)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$ROOT/release/NifSkope.exe}"
OUT="${2:-$ROOT/tests/waterflow1101a/out}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PY="${PY:-$(command -v python)}"
T="$(cygpath -m "$ROOT/tests/waterflow1101a" 2>/dev/null || echo "$ROOT/tests/waterflow1101a")"
SIDE="$T/Commonwealth.waterflow.txt"
unset WW_LODL_WATERFLOW_RED WW_LODL_FLOW_DIR_RED WW_WATER_SPEED_FLOOR_RED WW_WATER_FIELD_DUMP WW_WATER_MARK_BODY
tasklist 2>/dev/null | grep -qi fallout4.exe && { echo "Fallout4.exe is running: no bake"; exit 3; }
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }
mkdir -p "$OUT"
W="$(cygpath -m "$OUT" 2>/dev/null || echo "$OUT")"
fails=0
ok()  { echo "OK   $*"; }
bad() { echo "FAIL $*"; fails=$((fails+1)); }
bake() {   # bake <tag> <env...> -- <extra args...>  ->  $OUT/<tag>.lodl, $OUT/<tag>_bake.log; rc of the exe
	local tag=$1; shift
	local envs=()
	while [ $# -gt 0 ] && [ "$1" != "--" ]; do envs+=("$1"); shift; done
	[ $# -gt 0 ] && shift
	rm -rf "$OUT/bake_$tag"
	env ${envs[@]+"${envs[@]}"} "$EXE" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$W/bake_$tag" "$@" < /dev/null > "$OUT/${tag}_bake.log" 2>&1
	local rc=$?
	if [ -f "$OUT/bake_$tag/FO4CSLOD/Commonwealth/Commonwealth.lodl" ]; then
		cp "$OUT/bake_$tag/FO4CSLOD/Commonwealth/Commonwealth.lodl" "$OUT/$tag.lodl"
	fi
	rm -rf "$OUT/bake_$tag"
	echo "[$tag] rc=$rc : $(grep -ao 'flow overrides[^;]*;[^;]*' "$OUT/${tag}_bake.log" | head -1)"
	return $rc
}
gate() { "$PY" -I "$T/gate.py" "$@" < /dev/null; }

echo "== 1. bakes: the sidecar, its red, the env red, no sidecar, a second default"
rm -f "$OUT"/*.lodl
bake new -- --water-flow-overrides "$SIDE" --water-report "$W/new_report.txt" || bad "the default bake failed"
bake red -- --water-flow-overrides "$SIDE" --water-no-overrides || bad "the red bake failed"
bake envred WW_LODL_WATERFLOW_RED=1 -- --water-flow-overrides "$SIDE" || bad "the env red bake failed"
bake none -- || bad "the bake with no sidecar failed"
bake new2 -- --water-flow-overrides "$SIDE" || bad "the second default bake failed"
cmp -s "$OUT/new.lodl" "$OUT/new2.lodl" && ok "two default bakes are byte-equal (deterministic)" || bad "two default bakes differ"
cmp -s "$OUT/red.lodl" "$OUT/envred.lodl" && ok "red flag == WW_LODL_WATERFLOW_RED=1, byte for byte" || bad "the two reds differ"
cmp -s "$OUT/red.lodl" "$OUT/none.lodl" && ok "red == no sidecar at all, byte for byte (the old rule)" || bad "the red differs from no sidecar"
cmp -s "$OUT/new.lodl" "$OUT/red.lodl" && bad "the sidecar changed nothing" || ok "the sidecar changes the file"
grep -a "== FLOW OVERRIDES ==" -A40 "$OUT/new_report.txt" 2>/dev/null | grep -a "^  " | head -40 | cut -c1-200 | sed 's/^/     /'

echo "== 2. the gate: new PASSES, the red FAILS"
gate "$W/new.lodl" "$SIDE" > "$OUT/gate_new.txt"; r1=$?
grep -av "^  OK  \|^  rep " "$OUT/gate_new.txt" | cut -c1-220 | sed 's/^/     /'
[ $r1 = 0 ] && ok "new: $(grep -a '^RESULT' "$OUT/gate_new.txt")" || bad "new: $(grep -a '^RESULT' "$OUT/gate_new.txt")"
gate "$W/red.lodl" "$SIDE" --expect-fail > "$OUT/gate_red.txt"; r2=$?
[ $r2 = 0 ] && ok "red (--water-no-overrides) FAILS: $(grep -a '^RESULT' "$OUT/gate_red.txt" | cut -c1-160)" \
	|| bad "red did not fail: $(grep -a '^RESULT' "$OUT/gate_red.txt")"
if [ -n "${SHIPPED:-}" ] && [ -f "$SHIPPED" ]; then
	gate "$(cygpath -m "$SHIPPED")" "$SIDE" --expect-fail > "$OUT/gate_shipped.txt"; r3=$?
	[ $r3 = 0 ] && ok "the shipped bake FAILS: $(grep -a '^RESULT' "$OUT/gate_shipped.txt" | cut -c1-160)" \
		|| bad "the shipped bake did not fail"
fi

echo "== 3. a bad sidecar refuses the bake"
printf 'wwflow 1\nbody 1 25.5 0.5 ocean\n' > "$OUT/bad.waterflow.txt"
bake bad -- --water-flow-overrides "$W/bad.waterflow.txt" && bad "a bad class line was accepted" \
	|| { grep -aq "water flow overrides: .*line 2: class 'ocean'" "$OUT/bad_bake.log" && ok "a bad class line refuses the bake with its line number" \
		|| bad "the refusal did not name the line: $(grep -a 'overrides' "$OUT/bad_bake.log" | head -2)"; }
printf 'body 1 25.5 0.5 sea\n' > "$OUT/nover.waterflow.txt"
bake nover -- --water-flow-overrides "$W/nover.waterflow.txt" && bad "a file with no wwflow line was accepted" \
	|| { grep -aq "no 'wwflow 1' line" "$OUT/nover_bake.log" && ok "a file with no 'wwflow 1' line refuses the bake" || bad "no-version refusal text"; }
rm -f "$OUT/bad.lodl" "$OUT/nover.lodl"

echo "== 4. the water marking tool on the new file (registered FAILs: F2, F5 p99)"
cp "$OUT/new.lodl" "$OUT/st_new.lodl"
"$EXE" -no-gui lodl "$W/st_new.lodl" --water-mark-selftest < /dev/null > "$OUT/st_new.log" 2>&1
rm -f "$OUT/st_new.lodl" "$OUT/st_new.lodl.bak-watermark"
echo "     $(grep -a 'checks, ' "$OUT/st_new.log" | tail -1)"
KNOWN="F2 island bank direction|F5 the 99th percentile of the adjacent angle difference"
unknown=$(grep -a '^  FAIL ' "$OUT/st_new.log" | grep -avE "$KNOWN")
known=$(grep -a '^  FAIL ' "$OUT/st_new.log" | grep -acE "$KNOWN")
[ -z "$unknown" ] && [ "$known" -le 2 ] && ok "marking-tool selftest on the new file: $known registered FAIL(s), nothing else" \
	|| { bad "marking-tool selftest: $known known FAILs, and beyond them:"; echo "$unknown" | cut -c1-200; }

rm -f "$OUT/new2.lodl" "$OUT/envred.lodl" "$OUT/none.lodl"
echo "== result: $fails FAIL"
[ "$fails" = 0 ] && echo "waterflow1101a PASS" || echo "waterflow1101a FAIL"
[ "$fails" = 0 ]
