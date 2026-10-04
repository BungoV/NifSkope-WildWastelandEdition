#!/bin/bash
# lane AODECAL1 (2026-10-04): the baked AO decals in a real cell (Concord, the street). The probe bake is READ
# (WW_CELL_GI_FROM, cell_gi.sh's own concord bake), so every run below differs only in the decal keys.
#   census   which placed statics got a decal and why the rest did not (WW_CELL_AODECAL_CENSUS)
#   gpu      the decal target (WW_CELL_AODECAL_DUMP) vs the twin's product of lookups (aodecal1_real.py gpu)
#            reds: add (summed, not multiplied), frozen (every copy at its model's first copy) -> must FAIL
#   d        gate D's table (WW_CELL_AODECAL_GATE): the copy-free sky; red nodivide -> must FAIL
#   clutter  red: small clutter let in -> the census must show more decals than the green
#   off      switch off: the shot and the GI dump byte-identical to the rung exe (BEFORE)
#   sunlamp  WW_CELL_GI=0 (sun and lamps only): decal on == decal off, byte for byte
# Usage: bash tests/spells/aodecal1_cell.sh [OUT]   (CAM=x,y,z/view/dist picks the shot; the census picks it next)
set -u
. "$(dirname "$0")/_harness.sh"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-$REPO/release/NifSkope.before_aodecal1.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
FROM="${FROM:-E:/Projects/NifskopeWildWastelandEdition/scratchpad/prtpgi_20261001/gate/concord/bake}"
OUT="${1:-$REPO/scratchpad/aodecal1_20261004/cell}"
CAM="${CAM:--60200,73500,6300/4/600}"
GPUCAM="${GPUCAM:--55200,67900,6080/1/1100}"
SIZE="${SIZE:-960x600}"
PORT="${PORT:-14761}"
STEPS="${STEPS:-census gpu d clutter off sunlamp sheet}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SCOPE="${SCOPE:-aodecal1_cell}"
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	printf '%s' "$SCOPE"
}
trap wipe_scope EXIT
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
AO="$OUT/ao"; mkdir -p "$AO"
LOG="$OUT/aodecal1_cell.log"; : > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
LD=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=12 WW_LOOKDEV_SUN=0 WW_LOOKDEV_GROUND=0
	WW_LOOKDEV_SKY=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_FOG=0 WW_LOOKDEV_SHADOWS=0 WW_LOOKDEV_PLUGINS="$ESM" )
say "aodecal1_cell.sh  $(date '+%Y-%m-%d %H:%M:%S')  cam $CAM"

shoot() {   # [XE=<exe>] shoot <tag> <env...>
	local tag="$1"; shift
	local shot="$OUT/$tag.png" notes="$OUT/$tag.notes" exe="${XE:-$EXE}" rest="${CAM#*/}"
	rm -f "$shot" "$notes"
	env WW_CELL_LIT=1 WW_CELL_BAKE_REFRACT_RED=keep WW_CELL_IS=0 "${LD[@]}" WW_CELL_OPEN="$ESM|Commonwealth|-15,17|1" \
		WW_CELL_GI_FROM="$(winpath "$FROM")" WW_CELL_PROBES="$(winpath "$OUT/$tag.probes.tsv")" WW_CELL_PROBES_HIDE=1 "$@" \
		WW_RENDER_CENTER="${CAM%%/*}" WW_RENDER_VIEW="${rest%%/*}" WW_RENDER_DIST="${rest#*/}" WW_RENDER_FOV=70 \
		WW_CELL_DATAROOT="$DATA" WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1500 "$exe" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}
ON=( WW_CELL_AODECAL=1 WW_CELL_AODECAL_DIR="$(winpath "$AO")" )
has() { case " $STEPS " in *" $1 "*) return 0 ;; esac; return 1; }
census_count() { awk -F'\t' '$6=="decal"' "$1" 2>/dev/null | wc -l; }

if has census || has d; then
	ok=$(shoot on "${ON[@]}" WW_CELL_GI=1 WW_CELL_AODECAL_CENSUS="$(winpath "$OUT/census.tsv")" \
		WW_CELL_AODECAL_GATE="$(winpath "$OUT/gate_d.txt")")
	check "decals on: the shot drew" "$ok"
	grep -E "aodecal" "$OUT/on.notes" | head -6 | sed 's/^/        /' | tee -a "$LOG"
	say "        census: $(census_count "$OUT/census.tsv") models get a decal of $(($(wc -l < "$OUT/census.tsv" 2>/dev/null || echo 0))) seen"
	ok=$(shoot off WW_CELL_GI=1)
	check "decals off: the shot drew" "$ok"
fi
if has gpu; then
	# its own camera: the crate pile from above (view 1 = top), where copies overlap (the stacking check needs 200 such pixels)
	ok=$(CAM="$GPUCAM" shoot gpu "${ON[@]}" WW_CELL_GI=1 WW_CELL_AODECAL_DUMP="$(winpath "$OUT/dump.bin")")
	check "gpu: the shot drew" "$ok"
	python "$(dirname "$0")/aodecal1_real.py" gpu "$(winpath "$OUT/dump.bin")" "$(winpath "$AO")" > "$OUT/gpu.txt" 2>&1
	rc=$?; sed 's/^/        /' "$OUT/gpu.txt" | tee -a "$LOG"
	check "gpu: the decal target is the product of the lookups" "$([ $rc -eq 0 ] && echo 1 || echo 0)"
	for red in add frozen; do
		CAM="$GPUCAM" shoot red_$red "${ON[@]}" WW_CELL_GI=1 WW_CELL_AODECAL_RED=$red WW_CELL_AODECAL_DUMP="$(winpath "$OUT/dump_$red.bin")" > /dev/null
		python "$(dirname "$0")/aodecal1_real.py" gpu "$(winpath "$OUT/dump_$red.bin")" "$(winpath "$AO")" > "$OUT/gpu_$red.txt" 2>&1
		rc=$?; sed 's/^/        /' "$OUT/gpu_$red.txt" | tee -a "$LOG"
		check "gpu red $red FAILS (on the numbers, not a crash)" "$([ $rc -ne 0 ] && grep -q '^GPU FAIL' "$OUT/gpu_$red.txt" && echo 1 || echo 0)"
	done
fi
if has d; then
	shoot red_nodivide "${ON[@]}" WW_CELL_GI=1 WW_CELL_AODECAL_RED=nodivide \
		WW_CELL_AODECAL_GATE="$(winpath "$OUT/gate_d_nodivide.txt")" > /dev/null
	python "$(dirname "$0")/aodecal1_real.py" d "$(winpath "$OUT/gate_d.txt")" "$(winpath "$OUT/gate_d_nodivide.txt")" \
		> "$OUT/d.txt" 2>&1
	rc=$?; sed 's/^/        /' "$OUT/d.txt" | tee -a "$LOG"
	check "d: the copy-free sky within the bars, the red nodivide out of them" "$([ $rc -eq 0 ] && echo 1 || echo 0)"
fi
if has clutter; then
	shoot red_clutter "${ON[@]}" WW_CELL_GI=1 WW_CELL_AODECAL_RED=clutter \
		WW_CELL_AODECAL_CENSUS="$(winpath "$OUT/census_clutter.tsv")" > /dev/null
	g=$(census_count "$OUT/census.tsv"); r=$(census_count "$OUT/census_clutter.tsv")
	small=$(awk -F'\t' '$6=="small clutter"' "$OUT/census.tsv" | wc -l)
	say "        green: $g decal models, $small small clutter skipped; red clutter: $r decal models"
	check "clutter red: small clutter let in shows (red $r > green $g), and the green skipped some ($small)" \
		"$([ "$r" -gt "$g" ] && [ "$small" -gt 0 ] && echo 1 || echo 0)"
fi
if has off; then
	rm -rf "$OUT/gi_new" "$OUT/gi_rung"
	a=$(shoot off_new WW_CELL_GI=1 WW_CELL_GI_DUMP="$(winpath "$OUT/gi_new")")
	b=$(XE="$BEFORE" shoot off_rung WW_CELL_GI=1 WW_CELL_GI_DUMP="$(winpath "$OUT/gi_rung")")
	same=1
	cmp -s "$OUT/off_new.png" "$OUT/off_rung.png" || { say "        the shots differ"; same=0; }
	# every binary byte for byte; gi_meta.txt with its stage timings ("ms N", "ms light N gather N grid N") taken out
	[ -d "$OUT/gi_new" ] && [ -d "$OUT/gi_rung" ] || { say "        a GI dump is missing"; same=0; }
	for f in "$OUT"/gi_new/*; do
		g="$OUT/gi_rung/$(basename "$f")"
		case "$f" in
			*.txt) norm() { sed -E 's/ms light [0-9]+ gather [0-9]+ grid [0-9]+//g; s/ms [0-9]+//g' "$1"; }
				[ "$(norm "$f")" = "$(norm "$g")" ] || { say "        $(basename "$f") differs (timings aside)"; same=0; } ;;
			*) cmp -s "$f" "$g" || { say "        $(basename "$f") differs"; same=0; } ;;
		esac
	done
	check "off: the shot and the GI dump byte-identical to the rung exe" "$([ "$a$b$same" = 111 ] && echo 1 || echo 0)"
fi
if has sunlamp; then
	a=$(shoot sun_on "${ON[@]}" WW_CELL_GI=0)
	b=$(shoot sun_off WW_CELL_GI=0)
	same=0; cmp -s "$OUT/sun_on.png" "$OUT/sun_off.png" && same=1
	check "sun and lamps only (GI off): decals on == off, byte for byte" "$([ "$a$b$same" = 111 ] && echo 1 || echo 0)"
fi
if has sheet; then
	# SHEETS="name=x,y,z/view/dist;..." -> sheet_<name>_off.png | sheet_<name>_on.png, the sun and the sky drawn
	IFS=';' read -ra pairs <<< "${SHEETS:-car=-60576,69770,6077/1/900;crates=-55200,67900,6080/1/1100;dumpster=-62061,70746,6263/1/700}"
	for pr in "${pairs[@]}"; do
		nm="${pr%%=*}"
		for st in off on; do
			sw=(); [ $st = on ] && sw=( "${ON[@]}" )
			r=$(CAM="${pr#*=}" shoot sheet_${nm}_$st WW_CELL_GI=1 "${sw[@]}" WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=1 \
				WW_LOOKDEV_CLOUDS=1 WW_LODGEN_RESOURCES="$DATA")
			check "sheet $nm decals $st drew" "$r"
		done
	done
fi
say "aodecal1_cell.sh: $fails failed"
exit $fails
