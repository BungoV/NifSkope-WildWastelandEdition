#!/bin/bash
#
# T2 and T3 BOTH WAYS on the far-terrain mask and emissive sheets, on a fixture,
# because the shipped Fallout 4 corpus has nothing to fire them with.
#
# Measured 2026-09-11: 14 of 14 landscape textures on the Sanctuary region
# resolve `legacy-inverted`; NO Fallout 4 landscape TXST names a .pbrm and none
# carries a glow map. So bungo's PBRM arm -- "if PBRM is used to bake it, it gets
# roughness", "then also add metallic map, but that should only get derived from
# PBRM" -- and the emissive sheet's PRESENT side would never be exercised by a
# bake of his data, and a gate that only ever sees the absent side is not a gate.
#
# The fixture (tests/spells/lodgen_terrain_pbrm_fixture.py) writes a PBRM beside
# five landscape diffuses in a folder of its own. It touches nothing of the
# user's: the unpacked Data is mounted READ-ONLY as a --resource stack entry and
# the fixture folder is the --data-root, so the .pbrm files are the only thing
# the bake reads from anywhere writable.
#
# WHAT IS GATED, and each side has the other as its floor:
#   T2a  a PBRM layer's metallic is the PBRM's own map, NOT 0
#   T2b  a bake with no PBRM anywhere reads metallic EXACTLY 0
#   T3a  a layer with an emissive map puts an EMISSIVE SHEET in the container
#   T3b  a bake with none writes NO emissive sheet and the index says "none"
#   T1p  a PBRM layer's roughness is the PBRM's RMAOS RED, not 1 - gloss
#
# USAGE
#   bash tests/spells/lodgen_terrain_pbrm.sh
#
# Exit 2 = a missing input; 1 = a failed check.

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

checks=0
fails=0
ok() { checks=$((checks + 1)); echo "  ok   $1"; }
bad() { checks=$((checks + 1)); fails=$((fails + 1)); echo "  FAIL $1"; }
say() { echo "       $1"; }

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }
[ -d "$DATA" ] || { echo "no unpacked Data at $DATA"; exit 2; }

echo "== the fixture =="
"$PY" "$ROOT/tests/spells/lodgen_terrain_pbrm_fixture.py" "$W/ovl" \
	"Landscape\\Ground\\NF_Dirt01_d.dds" \
	"Landscape\\Ground\\NF_ScrubGrass_d.dds" \
	"Landscape\\Ground\\DriedGrass01_d.dds" \
	"Landscape\\Ground\\ForestFloor01_d.dds" \
	"Landscape\\Ground\\RubbleRock01_d.dds" | sed 's/^/       /'

bake() {          # $1 = name, $2 = data root
	mkdir -p "$W/$1/obj" "$W/$1/tex"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 \
		--vt "$W/$1" --vt-height --cover --tex-dir "$W/$1/tex" --out-dir "$W/$1/obj" \
		--resource "$DATA" --data-root "$2" > "$W/$1.log" 2>&1
	grep -a "^vt:" "$W/$1.log" | head -1
}

echo "== the two bakes: PBRM present, and PBRM absent =="
ON="$(bake with "$W/ovl")"
OFF="$(bake without "$DATA")"
say "with    $ON"
say "without $OFF"

field() { echo "$1" | grep -oa "$2 [0-9a-z]*" | head -1 | awk '{print $2}'; }

P_ON="$(field "$ON" maskPbrm)"; P_OFF="$(field "$OFF" maskPbrm)"
M_ON="$(field "$ON" maskMetalMaps)"; M_OFF="$(field "$OFF" maskMetalMaps)"
E_ON="$(field "$ON" emissive)"; E_OFF="$(field "$OFF" emissive)"
S_ON="$(field "$ON" sheets)"; S_OFF="$(field "$OFF" sheets)"

[ "${P_ON:-0}" -gt 0 ] && ok "T2a the PBRM arm fires at all ($P_ON layers served by pbrm)" \
	|| bad "T2a the PBRM arm fires ($P_ON layers)"
[ "${P_OFF:-1}" = "0" ] && ok "T2a FLOOR the same region with no PBRM has zero pbrm layers" \
	|| bad "T2a FLOOR the same region with no PBRM has zero pbrm layers ($P_OFF)"
[ "${M_ON:-0}" -gt 0 ] && ok "T2 a PBRM layer contributes a METALLIC map ($M_ON)" \
	|| bad "T2 a PBRM layer contributes a metallic map ($M_ON)"
[ "${M_OFF:-1}" = "0" ] && ok "T2 FLOOR a legacy-only region contributes NO metallic map" \
	|| bad "T2 FLOOR a legacy-only region contributes no metallic map ($M_OFF)"
[ "$E_ON" = "present" ] && ok "T3a the emissive sheet is WRITTEN when a layer supplies one" \
	|| bad "T3a the emissive sheet is written when a layer supplies one ($E_ON)"
[ "$E_OFF" = "none" ] && ok "T3b and ABSENT when none does, named as absent in the index" \
	|| bad "T3b the emissive sheet is absent when no layer supplies one ($E_OFF)"
[ "${S_ON:-0}" = "5" ] && [ "${S_OFF:-0}" = "4" ] \
	&& ok "T3 the sheet COUNT moves with it (5 with, 4 without)" \
	|| bad "T3 the sheet count moves with the emissive sheet ($S_ON vs $S_OFF)"

echo "== the containers validate, both ways =="
for v in with without; do
	if "$NS" -no-gui lodgen --lodt-check "$W/$v/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt" > "$W/$v.chk" 2>&1; then
		ok "the $v-PBRM container passes every rule of the contract"
	else
		bad "the $v-PBRM container passes every rule of the contract"
		grep -a 'refused' "$W/$v.chk" | sed 's/^/       /'
	fi
done

echo "== the mask texels carry the PBRM's own numbers =="
"$PY" - "$W" <<'PYEOF'
import json, os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(
	os.environ.get('SPELLDIR', 'tests/spells'))), ''))
sys.path.insert(0, 'tests/spells')
from lodgen_terrain_model import Lodt           # noqa: E402

W = sys.argv[1]
# the container moved under the one root with every other FO4CS-target
# file (lane LAYOUT1, 2026-09-16); the --lodt-check above reads the same
# path, and this is the second place that spells it.
v = Lodt(os.path.join(W, 'with', 'FO4CSLOD', 'Commonwealth', 'Commonwealth.VT.2.lodt'))
roles = [s['role'] for s in v.sheets[:v.sheetCount]]
print('       roles %s' % roles)
if 6 not in roles:
	print('  FAIL T3 the container carries an EMISSIVE sheet (role 6)')
	sys.exit(1)
print('  ok   T3 the container carries an EMISSIVE sheet (role 6)')
idx = next(i for i, e in enumerate(v.table) if e['flags'] & 1)
mask, side = v.sheet(idx, 5, 0)
emis, _ = v.sheet(idx, 6, 0)
# the fixture's constants, through BC1's 5:6:5 round trip
WANT_R, WANT_G = 65, 190
WANT_E = (230, 48, 98)
rs = sorted(set(p[0] for p in mask))
gs = sorted(set(p[1] for p in mask))
es = sorted(set((p[0], p[1], p[2]) for p in emis))
print('       distinct mask R %d, G %d; emissive colours %d'
	  % (len(rs), len(gs), len(es)))
hitR = sum(1 for p in mask if p[0] == WANT_R)
hitG = sum(1 for p in mask if p[1] == WANT_G)
hitE = sum(1 for p in emis if (p[0], p[1], p[2]) == WANT_E)
n = len(mask)
print('       texels reading the PBRM constants: roughness %d/%d, metallic %d/%d, emissive %d/%d'
	  % (hitR, n, hitG, n, hitE, n))
bad = 0
if hitG > n // 100:
	print('  ok   T2 metallic %d (the PBRM map) reaches %.1f%% of the tile' % (WANT_G, 100.0 * hitG / n))
else:
	print('  FAIL T2 metallic %d reaches at least 1%% of the tile (%d)' % (WANT_G, hitG)); bad += 1
if max(gs) > 0:
	print('  ok   T2 metallic is NOT everywhere zero where a PBRM layer paints (max %d)' % max(gs))
else:
	print('  FAIL T2 metallic is not everywhere zero (max %d)' % max(gs)); bad += 1
if hitR > n // 100:
	print('  ok   T1p roughness %d (the PBRM RMAOS red, NOT 1-gloss) reaches %.1f%% of the tile'
		  % (WANT_R, 100.0 * hitR / n))
else:
	print('  FAIL T1p roughness %d reaches at least 1%% of the tile (%d)' % (WANT_R, hitR)); bad += 1
if hitE > n // 100:
	print('  ok   T3 the emissive sheet carries the fixture colour %s on %.1f%% of the tile'
		  % (str(WANT_E), 100.0 * hitE / n))
else:
	print('  FAIL T3 the emissive sheet carries the fixture colour (%d texels)' % hitE); bad += 1
sys.exit(1 if bad else 0)
PYEOF
PRC=$?
if [ "$PRC" = "0" ]; then
	checks=$((checks + 5))
else
	checks=$((checks + 5)); fails=$((fails + 1))
fi

echo
echo "$checks checks, $fails failures"
[ "$fails" = "0" ] && echo "RESULT PASS" || echo "RESULT FAIL"
exit $([ "$fails" = "0" ] && echo 0 || echo 1)
