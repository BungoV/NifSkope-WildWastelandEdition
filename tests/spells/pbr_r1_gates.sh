#!/usr/bin/env bash
# pbr_r1_gates.sh -- the R1 gates of the PBR renderer (lane PBRR1,
# docs/NIFSKOPE_PBR_RENDERER.md s13 + RULINGS 2026-09-23), judged by
# pbr_r1_gates.py. Gate (f), the standing zero set, is pbr_shade_ab.sh.
#
#   (a) coverage   PBR mode covers >= 99% of the pixels Legacy covers (duct cases)
#   (b) routes     the census route of every fixture == the independent Python
#                  resolver (pbr_r1_gates.py) over tests/fixtures/pbr_data
#   (c) f0         the uploaded pbrF0 (read back) of the v6 twin 0.040, the v5
#                  twin 0.040; the v6 twin under the v5 law 1.000 (red)
#   (d) nifx       -no-gui nifx round trip byte-identical; --canonical differs (red)
#   (e) direct     the direct-link NIF reads route=direct and draws PBR
#   route view     every fixture's route view is its route's colour
#
# usage: bash tests/spells/pbr_r1_gates.sh [--out DIR] [--only a,b] [--red coverage|order|order_e|f0law|nifx]
#   --red coverage  the PBR arm is a copy of release/ whose pbrm_default.frag
#                   discards every fragment: (a) must FAIL
#   --red order     WW_PBRM_ORDER=nifx,swap,direct,sibling,fo76 (swap<->nifx): (b) must FAIL
#   --red order_e   WW_PBRM_ORDER=swap,direct,nifx,sibling,fo76 (nifx<->direct): (b) must FAIL
#   --red f0law     WW_PBRM_F0_LAW=v5: (c) must FAIL (v6 twin reads 1.000)
#   --red nifx      (d) judged on the --canonical output: must FAIL
# One NifSkope instance at a time, second monitor, --port unused; bungo's own
# window (no --port) is never touched.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/pbr_r1_gates_$(date +%Y%m%d_%H%M%S)"
ONLY=""
RED=""
while [ $# -gt 0 ]; do
	case "$1" in
		--out) OUT="$2"; shift 2 ;;
		--only) ONLY=",$2,"; shift 2 ;;
		--red) RED="$2"; shift 2 ;;
		*) echo "unknown argument $1"; exit 2 ;;
	esac
done
DATA="/e/Tools/Fallout 4/DataUnpacked/Data"
PBRDATA="$ROOT/tests/fixtures/pbr_data"
ARM="$ROOT/release"
PORT="${PBR_R1_PORT:-43219}"
mkdir -p "$OUT"
[ "$(head -c 2 "$ARM/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
[ -f "$PBRDATA/cases.json" ] || python "$HERE/pbr_r1_fixtures.py" || exit 2

ORDER_PIN=""
case "$RED" in
	'') ;;
	coverage)
		REDDIR="$ROOT/release/pbr_r1_red_coverage"
		rm -rf "$REDDIR"; mkdir -p "$REDDIR"
		cp -p "$ARM/NifSkope.exe" "$ARM"/*.dll "$ARM/nif.xml" "$ARM/kfm.xml" "$ARM/qt.conf" "$ARM/style.qss" "$REDDIR/" 2>/dev/null
		for d in shaders platforms imageformats styles; do [ -d "$ARM/$d" ] && cp -rp "$ARM/$d" "$REDDIR/"; done
		python - "$REDDIR/shaders/pbrm_default.frag" <<'PYEOF' || { echo "REFUSED: sabotage did not apply"; exit 2; }
import sys
p = sys.argv[1]; b = open(p, "rb").read()
i = b.rindex(b"}")
open(p, "wb").write(b[:i] + b"\tdiscard;\n" + b[i:])
print("sabotaged pbrm_default.frag: discard")
PYEOF
		;;
	order) ORDER_PIN="WW_PBRM_ORDER=nifx,swap,direct,sibling,fo76" ;;
	order_e) ORDER_PIN="WW_PBRM_ORDER=swap,direct,nifx,sibling,fo76" ;;
	f0law|nifx) ;;
	*) echo "unknown red $RED"; exit 2 ;;
esac
echo "red=${RED:-none}" > "$OUT/arms.txt"
sha1sum "$ARM/NifSkope.exe" >> "$OUT/arms.txt"

harness_alive() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

run_one() {  # run_one <exe dir> <tag> <nif> [KEY=VALUE ...]
	local arm="$1" tag="$2" nif="$3"; shift 3
	local png="$OUT/$tag.png"
	rm -f "$png" "$OUT/$tag.prog.txt" "$OUT/$tag.pbrm.txt"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "HKCU\\Software\\NifTools\\NifSkope 2.0 pbrr1gates" //f > /dev/null 2>&1
	env WW_SETTINGS_SCOPE=pbrr1gates \
		WW_WINDOW_AT=1960,40 \
		WW_LODGEN_RESOURCES="$(winpath "$DATA");$(winpath "$PBRDATA")" \
		WW_RENDER_SHOT="$(winpath "$png")" \
		WW_RENDER_SIZE=1280x859 \
		WW_RENDER_VIEW=8 \
		WW_RENDER_TIME=1.0 \
		WW_RENDER_CLEAN=1 \
		WW_RENDER_FLAT=0 \
		WW_RENDER_REFRACTION=1 \
		WW_RENDER_SS=0 \
		WW_PBRM_MODE=legacy \
		WW_PBRM_AUTOREPLACE=1 \
		WW_LIGHTING_MODE=legacy \
		WW_LOOKDEV=0 \
		WW_RENDER_PARTICLES=1 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0 \
		WW_PROGRAM_CENSUS="$(winpath "$OUT/$tag.prog.txt")" \
		WW_PBRM_CENSUS="$(winpath "$OUT/$tag.pbrm.txt")" \
		"$@" \
		timeout 180 "$arm/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
}

want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }

# the case table: name|nif|pins (pins separated by spaces, no spaces inside)
CASES="$(python - "$PBRDATA/cases.json" <<'PYEOF'
import json, sys
c = json.load(open(sys.argv[1]))
for n, v in c.items():
    print("%s|%s|%s" % (n, v["nif"], " ".join("%s=%s" % kv for kv in v["pins"].items())))
PYEOF
)"
CASES="$(printf '%s\n' "$CASES" | tr -d '\r')"
nifpath() { case "$1" in \$DATA/*) echo "$DATA/${1#\$DATA/}" ;; *) echo "$PBRDATA/$1" ;; esac; }

PBRARM="$ARM"; [ "$RED" = "coverage" ] && PBRARM="$REDDIR"
F0PIN=""; [ "$RED" = "f0law" ] && F0PIN="WW_PBRM_F0_LAW=v5"

while IFS='|' read -r name nif pins; do
	[ -n "$name" ] || continue
	want "$name" || continue
	n="$(nifpath "$nif")"
	echo "case $name"
	# shellcheck disable=SC2086
	if [ "$RED" = "" ] || [ "$RED" = "coverage" ]; then
		run_one "$ARM" "${name}_legacy" "$n" $pins
	fi
	# shellcheck disable=SC2086
	run_one "$PBRARM" "${name}_pbr" "$n" $pins WW_PBRM_MODE=pbr $ORDER_PIN $F0PIN
	if [ "$RED" = "" ]; then
		# shellcheck disable=SC2086
		run_one "$ARM" "${name}_route" "$n" $pins WW_PBRM_ROUTE_VIEW=1
	fi
done <<< "$CASES"

# (d) the .nifx round trip, headless
RT="$PBRDATA/nifx_roundtrip.nifx"
if want nifx_rt; then
	"$ARM/NifSkope.exe" -no-gui nifx "$(winpath "$RT")" --out "$(winpath "$OUT/nifx_rt_same.nifx")" > "$OUT/nifx_rt.log" 2>&1
	echo "nifx-rc=$?" >> "$OUT/nifx_rt.log"
	"$ARM/NifSkope.exe" -no-gui nifx "$(winpath "$RT")" --canonical --out "$(winpath "$OUT/nifx_rt_canonical.nifx")" >> "$OUT/nifx_rt.log" 2>&1
	"$ARM/NifSkope.exe" -no-gui nifx "$(winpath "$RT")" --set "NewNode=Materials\\WWPbrTest01\\SwapDuct.pbrm" \
		--set "ACDuctConnector01:0=Materials\\WWPbrTest01\\SiblingV5Duct.pbrm" --out "$(winpath "$OUT/nifx_rt_edit.nifx")" >> "$OUT/nifx_rt.log" 2>&1
	"$ARM/NifSkope.exe" -no-gui nifx "$(winpath "$OUT/nifx_rt_edit.nifx")" --remove NewNode \
		--set "ACDuctConnector01:0=Materials\\WWPbrTest01\\NifxTarget.pbrm" --out "$(winpath "$OUT/nifx_rt_undo.nifx")" >> "$OUT/nifx_rt.log" 2>&1
fi

[ -n "${REDDIR:-}" ] && rm -rf "$REDDIR"
python "$HERE/pbr_r1_gates.py" --out "$OUT" --data "$PBRDATA" --red "${RED:-none}"
