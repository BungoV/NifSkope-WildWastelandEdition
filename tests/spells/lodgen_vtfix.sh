#!/bin/bash
# lodgen_vtfix.sh -- lane VTFIX1 (2026-09-24), the terrain-pyramid correctness fixes before the whole bake.
#
#   G1  sum(maskRules) == distinctLtex on a WHOLE-Commonwealth --vt bake. The bake takes ~10 min and 4 GB,
#       so this leg checks a .lodm handed in as G1_LODM=<path> (scratchpad/vtfix1_20260924/run_g1.sh makes
#       one); without it the leg is a NAMED SKIP, never a pass. Red on the pre-VTFIX1 exe: 100 != 101 (the
#       null LTEX, form 0, was stored in the mask cache and never counted).
#   G2  --land-detail-source vanilla-blend adds vanilla's detail on the RIGHT axes. Known answer: a fixture
#       vanilla _msn whose fine relief runs along EAST only (lodgen_vtfix_check.py fixture). Baked with and
#       without the blend on chunk 4.-20.24; the east channel (R) must move and north (B) must not
#       (PASS iff mean|dR| >= 8 and mean|dB| <= 0.25 mean|dR|). Red on the pre-VTFIX1 exe: R 4.21, B 77.15.
#       REFUTER built in: the none arm is the same exe, same chunk, so the only thing that differs is the blend.
#   G3  --incremental rebakes after a DEFAULT FLIP. The flip is real and moves bytes: a copy of the exe with
#       its default vanilla LOD root (UTF-16 "E:/Tools/Fallout 4/DataUnpacked/Data", 4 sites) respelled
#       ".../DatX", so the default --land-detail-source vanilla no longer finds vanilla's _msn and bakes ours.
#       Full bake with EXE, then `--incremental` with the flipped copy over it, then a full bake with the
#       flipped copy elsewhere: PASS iff the incremental run's files == the flipped full bake's files.
#       Red on the pre-VTFIX1 exe: the incremental run calls every chunk clean and keeps the old _msn.
#       Every run passes --data-root explicitly, so the respelled string reaches only the vanilla root.
#
# USAGE  bash tests/spells/lodgen_vtfix.sh        EXE=<exe> PHASES=123 (default 23) G1_LODM=<.lodm>
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PY="${PY:-/c/Users/bungo/AppData/Local/Programs/Python/Python39/python}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PHASES="${PHASES:-23}"
CHK="$ROOT/tests/spells/lodgen_vtfix_check.py"
W="${OUT:-$ROOT/scratchpad/vtfix_gate}"
rm -rf "$W"; mkdir -p "$W"
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"
checks=0; fails=0; skips=0
ok ()   { checks=$((checks+1)); echo "  ok    $*"; }
bad ()  { checks=$((checks+1)); fails=$((fails+1)); echo "  FAIL  $*"; }
skip () { skips=$((skips+1)); echo "  SKIP  $*"; }
echo "lodgen_vtfix: $EXE  $(sha1sum "$EXE" | cut -c1-8)  phases $PHASES"

if [ "${PHASES#*1}" != "$PHASES" ]; then
	echo "== G1 maskRules sum == distinctLtex =="
	if [ -n "${G1_LODM:-}" ] && [ -f "$G1_LODM" ]; then
		v="$("$PY" "$CHK" g1 "$G1_LODM")"; echo "      $v"
		case "$v" in PASS*) ok "G1 $v";; *) bad "G1 $v";; esac
	else
		skip "G1 needs G1_LODM=<whole-Commonwealth VT .lodm> (scratchpad/vtfix1_20260924/run_g1.sh)"
	fi
fi

if [ "${PHASES#*2}" != "$PHASES" ]; then
	echo "== G2 vanilla-blend detail lands on the right axes =="
	"$PY" "$CHK" fixture "$WA/fixroot" Commonwealth 4 -20 24 | sed 's/^/      /'
	for arm in none blend; do
		if [ $arm = none ]; then X=(--land-detail-source none)
		else X=(--land-detail-source vanilla-blend --vanilla-lod-root "$WA/fixroot"); fi
		"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -17 27 --dim 4 "${X[@]}" \
			--out-dir "$WA/g2_$arm" --tex-dir "$WA/g2_$arm/tex" > "$W/g2_$arm.log" 2>&1
		echo "      [$arm] rc=$?"
	done
	v="$("$PY" "$CHK" g2 "$WA/g2_none/tex/Commonwealth.4.-20.24_msn.DDS" "$WA/g2_blend/tex/Commonwealth.4.-20.24_msn.DDS")"
	echo "      $v"
	case "$v" in PASS*) ok "G2 $v";; *) bad "G2 $v";; esac
fi

if [ "${PHASES#*3}" != "$PHASES" ]; then
	echo "== G3 --incremental rebakes after a default flip =="
	FLIP="$(dirname "$EXE")/NifSkope.vtfixflip.exe"
	"$PY" - "$EXE" "$FLIP" <<'PYEOF'
import sys
b = open(sys.argv[1], 'rb').read()
a = 'E:/Tools/Fallout 4/DataUnpacked/Data'.encode('utf-16-le')
n = b.count(a)
b = b.replace(a, 'E:/Tools/Fallout 4/DataUnpacked/DatX'.encode('utf-16-le'))
open(sys.argv[2], 'wb').write(b)
print('      flipped copy: %d site(s) of the default vanilla LOD root respelled' % n)
PYEOF
	bake () {  # bake <exe> <dir> [extra]
		local e="$1" d="$2"; shift 2
		"$e" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -13 27 --dim 4 \
			--data-root "$DATA" --out-dir "$d/out" --tex-dir "$d/tex" "$@"
	}
	treesha () { ( cd "$1" && find . -type f ! -name '*.lodb' ! -name 'bake.log' | LC_ALL=C sort | while IFS= read -r f; do
		printf '%s  %s\n' "$(sha1sum "$f" | cut -c1-40)" "$f"; done ); }
	bake "$EXE" "$WA/g3_inc" > "$W/g3_full.log" 2>&1;  echo "      [full, EXE] rc=$?"
	treesha "$W/g3_inc" > "$W/g3_before.sha"
	bake "$FLIP" "$WA/g3_inc" --incremental "$WA/g3_inc/out" > "$W/g3_incr.log" 2>&1; echo "      [incremental, flipped] rc=$?"
	treesha "$W/g3_inc" > "$W/g3_incr.sha"
	bake "$FLIP" "$WA/g3_ref" > "$W/g3_ref.log" 2>&1;   echo "      [full, flipped] rc=$?"
	treesha "$W/g3_ref" > "$W/g3_ref.sha"
	grep -E "^incremental: " "$W/g3_incr.log" | head -2 | sed 's/^/      log: /'
	nm="$(diff "$W/g3_before.sha" "$W/g3_ref.sha" | grep -c '^<')"
	echo "      the flip moves $nm file(s) of $(wc -l < "$W/g3_before.sha") in a full bake"
	[ "$nm" -ge 1 ] && ok "G3 floor: the flipped default moves output bytes ($nm file(s))" \
		|| bad "G3 floor: the flipped default moved no output byte -- the gate is vacuous"
	ns="$(diff "$W/g3_incr.sha" "$W/g3_ref.sha" | grep -c '^<')"
	[ "$ns" -eq 0 ] && ok "G3 the incremental run over the old bake == the flipped full bake" \
		|| bad "G3 the incremental run left $ns stale file(s) against the flipped full bake"
	rm -f "$FLIP"
fi

echo "$checks checks, $fails failures, $skips skips"
[ $fails -eq 0 ] && echo PASS || echo FAIL
[ $fails -eq 0 ]
