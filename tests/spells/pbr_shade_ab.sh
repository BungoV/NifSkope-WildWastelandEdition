#!/usr/bin/env bash
# pbr_shade_ab.sh -- the ONE old-vs-new shading harness of the PBR renderer
# (lane PBRR0, docs/NIFSKOPE_PBR_RENDERER.md s9; gates in s13 and the RULINGS).
#
#   OLD arm = a frozen rung FOLDER (exe + shaders/ + runtime), because shaders
#             load from applicationDirPath; default release/before_pbrr0.
#   NEW arm = the built tree, release/.
#
# Every WW_* pin below is set IDENTICALLY in both arms; a case line may override a
# pin, and the override goes to both arms. Per case: OLD three times (a, b, c) ->
# the noise bar from OLD-vs-OLD twice (a|b, a|c), then NEW once, judged against
# OLD a by pbr_shade_ab.py, which writes <case>_diff.png and one verdict line.
#
# usage: bash tests/spells/pbr_shade_ab.sh [--old DIR] [--new DIR] [--out DIR]
#            [--cases FILE] [--only a,b] [--red shader|census] [--old-runs N]
#   --red shader  NEW arm = a copy of --new with fo4_default.frag,
#                 fo4_effectshader.frag and particles.frag each clamping their
#                 output to 1 and scaling it by 0.9: `zero` must FAIL on every
#                 case with pixels (not @empty), census identical.
#   --red census  NEW arm = a copy of --new whose fo4_default.prog can never
#                 match: the program census must FAIL.
# The red arm is built under release/pbr_ab_red_<kind>/ and removed afterwards.
#
# One NifSkope instance at a time, second monitor, --port unused; bungo's own
# window (no --port on its command line) is never touched.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OLD="$ROOT/release/before_pbrr0"
NEW="$ROOT/release"
OUT="$ROOT/scratchpad/pbr_shade_ab_$(date +%Y%m%d_%H%M%S)"
CASES="$HERE/pbr_shade_ab_cases.txt"
ONLY=""
RED=""
OLD_RUNS=3
while [ $# -gt 0 ]; do
	case "$1" in
		--old) OLD="$2"; shift 2 ;;
		--new) NEW="$2"; shift 2 ;;
		--out) OUT="$2"; shift 2 ;;
		--cases) CASES="$2"; shift 2 ;;
		--only) ONLY=",$2,"; shift 2 ;;
		--red) RED="$2"; shift 2 ;;
		--old-runs) OLD_RUNS="$2"; shift 2 ;;
		*) echo "unknown argument $1"; exit 2 ;;
	esac
done

DATA="/e/Tools/Fallout 4/DataUnpacked/Data"
PBRDATA="$ROOT/tests/fixtures/pbr_data"
PORT="${PBR_AB_PORT:-43217}"
mkdir -p "$OUT"

for arm in "$OLD" "$NEW"; do
	[ -f "$arm/NifSkope.exe" ] || { echo "REFUSED: no NifSkope.exe in $arm"; exit 2; }
	[ -d "$arm/shaders" ] || { echo "REFUSED: no shaders/ in $arm"; exit 2; }
	[ "$(head -c 2 "$arm/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: $arm/NifSkope.exe is not a finished image"; exit 2; }
done

# ---- the red arm: a copy of NEW with one deliberate sabotage
mkarm() {  # mkarm <src> <dst>
	rm -rf "$2"; mkdir -p "$2"
	cp -p "$1/NifSkope.exe" "$1"/*.dll "$1/nif.xml" "$1/kfm.xml" "$1/qt.conf" "$1/style.qss" "$2/" 2>/dev/null
	for d in shaders platforms imageformats styles; do [ -d "$1/$d" ] && cp -rp "$1/$d" "$2/"; done
}
if [ -n "$RED" ]; then
	REDDIR="$ROOT/release/pbr_ab_red_$RED"
	mkarm "$NEW" "$REDDIR"
	python - "$REDDIR/shaders" "$RED" <<'PYEOF' || { echo "REFUSED: sabotage did not apply"; exit 2; }
import sys, os
d, kind = sys.argv[1], sys.argv[2]
def sub(fn, old, new):
    p = os.path.join(d, fn); b = open(p, "rb").read()
    assert b.count(old) >= 1, (fn, old)
    i = b.rindex(old); open(p, "wb").write(b[:i] + new + b[i + len(old):])
    print("sabotaged", fn)
if kind == "shader":
    for fn in ("fo4_default.frag", "fo4_effectshader.frag", "particles.frag"):
        # clamp THEN scale: a plain *0.9 left the over-bright BGEM glow (>1.11)
        # saturated at 1 and that case passed the first red run byte-identical
        sub(fn, b"}", b"\tfragColor.rgb = clamp( fragColor.rgb, 0.0, 1.0 ) * 0.9;\n}")
elif kind == "census":
    sub("fo4_default.prog", b"check BSVersion >= 130", b"check BSVersion >= 999")
else:
    sys.exit("unknown red kind " + kind)
PYEOF
	NEW="$REDDIR"
fi

sha1sum "$OLD/NifSkope.exe" "$NEW/NifSkope.exe" | sed 's#  *\*\?# #' > "$OUT/arms.txt"
echo "old=$OLD" >> "$OUT/arms.txt"; echo "new=$NEW" >> "$OUT/arms.txt"; echo "red=${RED:-none}" >> "$OUT/arms.txt"

# ---- one instance at a time: a harness NifSkope (has --port) still alive = refuse
harness_alive() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

# ---- the pins, identical in both arms
run_one() {  # run_one <arm dir> <tag> <nif> [KEY=VALUE ...]
	local arm="$1" tag="$2" nif="$3"; shift 3
	local png="$OUT/$tag.png"
	rm -f "$png" "$OUT/$tag.prog.txt" "$OUT/$tag.cam.txt" "$OUT/$tag.pbrm.txt"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: harness NifSkope still running (pid $alive)"; return 1; fi
	# Every run starts from an EMPTY settings scope. The app writes into the scope
	# (game paths, view state), and measured on the first dry run the first run of
	# a fresh scope framed 1280x826 and every later one 1280x824 -- so without this
	# the arm that ran first differed from the other by the machine's history.
	reg delete "HKCU\\Software\\NifTools\\NifSkope 2.0 pbrshadeab" //f > /dev/null 2>&1
	env WW_SETTINGS_SCOPE=pbrshadeab \
		WW_WINDOW_AT=1960,40 \
		WW_LODGEN_RESOURCES="$(winpath "$DATA")" \
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
		WW_CAMERA_CENSUS="$(winpath "$OUT/$tag.cam.txt")" \
		WW_PBRM_CENSUS="$(winpath "$OUT/$tag.pbrm.txt")" \
		"$@" \
		timeout 180 "$arm/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
}

n=0
while read -r name gate nif rest; do
	case "$name" in ''|'#'*) continue ;; esac
	if [ -n "$ONLY" ] && [[ "$ONLY" != *",$name,"* ]]; then continue; fi
	nif="${nif/\$DATA/$DATA}"; nif="${nif/\$PBRDATA/$PBRDATA}"
	[ -f "$nif" ] || { echo "case $name: fixture missing: $nif"; echo "$name $gate MISSING" >> "$OUT/cases.run"; continue; }
	# @directives go to the judge, KEY=VALUE pins go to BOTH arms
	pins=""; dirs=""
	for w in $rest; do
		case "$w" in @*) dirs="$dirs $w" ;; *) pins="$pins $w" ;; esac
	done
	echo "case $name ($gate) $nif${dirs}"
	# shellcheck disable=SC2086
	for i in $(seq 1 "$OLD_RUNS"); do
		tagi=$(printf "\\x$(printf %x $((96 + i)))")
		run_one "$OLD" "${name}_old_$tagi" "$nif" $pins
	done
	# shellcheck disable=SC2086
	run_one "$NEW" "${name}_new" "$nif" $pins
	echo "$name $gate$dirs" >> "$OUT/cases.run"
	n=$((n + 1))
done < "$CASES"

[ -n "$RED" ] && rm -rf "$REDDIR"
echo "ran $n cases -> $OUT"
python "$HERE/pbr_shade_ab.py" --out "$OUT" --red "${RED:-none}"
