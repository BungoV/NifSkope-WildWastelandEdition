#!/bin/bash
#
# THE PLACED LIGHTS KEPT BETWEEN RELIGHTS (lane STATICCACHE1, 2026-10-11; src/probegi.cpp, PRTP_PLAN 2as, deck C6).
#
# WW_CELL_GI_STATICCACHE=1 (ships OFF): a relight keeps each surfel's direct light from the placed lights; the next
# relight with the same surfels, soup, lights and shadow settings (a new hour or weather) takes it from the cache
# and traces only the sun. The deck's gate (DECK_MATRIX C6 / item 7): cached + movable = the full relight, byte
# for byte. WW_CELL_GI_STATICCACHE_WARM=1 makes the cell view relight once first under another sun (turned 90
# degrees, half as bright), so the relight that is dumped is a second hour's.
#
# Per cell (Vault111Cryo: many placed lights; Commonwealth -17,23: the sun), one bake, relights of it:
#   rung   the exe from before the lane                      } every .bin byte for byte (off = identical)
#   full   the new exe, switch unset                          }
#   warm   the new exe, the warm-up relight, switch unset: still the full relight's bytes (the warm-up leaves nothing)
#   cache  switch on + the warm-up: "HIT" in the notes, every .bin the full relight's, byte for byte
#   red    WW_CELL_GI_STATICCACHE_RED=withsun (the cache keeps the warm-up's sun): must differ (a cell with sun)
# The time: the notes' "gi staticcache:" lines, the miss (every light traced) against the hit.
#
# USAGE  bash tests/spells/cell_staticcache.sh   (EXE= RUNG= absolute; OUT=; CELLS="cryo ext")

set -u
. "$(dirname "$0")/_harness.sh"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
RUNG="${RUNG:-$REPO/release/NifSkope.before_skypic1.exe}"
LAUNCH="${LAUNCH:-/e/Projects/Claude/notes/finalfix/launch.sh}"
SCOPE="${SCOPE:-cell_staticcache}"
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
wipe_scope
trap wipe_scope EXIT
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
OUT="${OUT:-$REPO/scratchpad/staticcache1/gate}"
PORT="${PORT:-14767}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
CELLS="${CELLS:-cryo ext}"
declare -A OPEN=( [cryo]="interior|Vault111Cryo" [ext]="Commonwealth|-17,23|1" )
declare -A CAM=( [cryo]="-4600,-280,0/1/1400" [ext]="-67928,95613,7250/5/1800" )
declare -A TILT=( [cryo]="" [ext]="-15" )
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"; LOG="$OUT/cell_staticcache.log"; : > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
say "cell_staticcache.sh  $(date '+%Y-%m-%d %H:%M:%S')  exe $(sha1sum "$EXE" | cut -c1-8)  rung $(sha1sum "$RUNG" | cut -c1-8)"
newer=1
for s in src/probegi.cpp src/cellview.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"
LD=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=12 WW_LOOKDEV_DAY=4 WW_LOOKDEV_CLOUDTIME=0
	WW_LOOKDEV_SKY=1 WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_MOON=0 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_FOG=0
	WW_LOOKDEV_SHADOWS=0 WW_LOOKDEV_PLUGINS="$ESM" WW_CELL_IS=0 WW_CELL_GI_PASSES=1 )
shoot() {   # [XE=] shoot <cell> <tag> <env...>
	local cell="$1" tag="$2"; shift 2
	local run="$OUT/$cell" c="${CAM[$cell]}" exe="${XE:-$EXE}" rest
	rest="${c#*/}"
	mkdir -p "$run"; rm -f "$run/$tag.png" "$run/$tag.notes"
	env "${LD[@]}" WW_CELL_OPEN="$ESM|${OPEN[$cell]}" WW_CELL_BAKE_REFRACT_RED=keep "$@" ${TILT[$cell]:+WW_RENDER_PITCH=${TILT[$cell]}} \
		WW_RENDER_CENTER="${c%%/*}" WW_RENDER_VIEW="${rest%%/*}" WW_RENDER_DIST="${rest#*/}" WW_RENDER_FOV=70 \
		WW_CELL_DATAROOT="$DATA" WW_CELL_PROBES="$(winpath "$run/probes.tsv")" WW_CELL_PROBES_HIDE=1 WW_CELL_LIT=1 \
		WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" bash "$LAUNCH" "$run/$tag.notes" "$exe" --port "$PORT" "$(winpath "$SPEC")" > /dev/null 2>&1
	[ -s "$run/$tag.png" ] && echo 1 || echo 0
}
samebins() {   # samebins <dirA> <dirB> -> "same n"
	local s=1 n=0 f
	for f in "$1"/*.bin; do n=$((n+1)); cmp -s "$f" "$2/$(basename "$f")" || s=0; done
	echo "$s $n"
}
for cell in $CELLS; do
	run="$OUT/$cell"
	say "== $cell (${OPEN[$cell]}): bake"
	rm -rf "$run"; mkdir -p "$run"
	check "$cell: the bake ran" "$(shoot $cell bake WW_CELL_GI=1 WW_CELL_PROBE_SOUP="$(winpath "$run/soup.psp")" \
		WW_CELL_PROBE_BAKE="$(winpath "$run/bake")")"
	grep -h "  gi: " "$run/bake.notes" | cut -c1-260 | tee -a "$LOG"
	from=( WW_CELL_GI=1 WW_CELL_GI_FROM="$(winpath "$run/bake")" )
	a="$(shoot $cell full "${from[@]}" WW_CELL_GI_DUMP="$(winpath "$run/dump_full")")"
	b="$(XE="$RUNG" shoot $cell rung "${from[@]}" WW_CELL_GI_DUMP="$(winpath "$run/dump_rung")")"
	c="$(shoot $cell warm "${from[@]}" WW_CELL_GI_STATICCACHE_WARM=1 WW_CELL_GI_DUMP="$(winpath "$run/dump_warm")")"
	d="$(shoot $cell cache "${from[@]}" WW_CELL_GI_STATICCACHE=1 WW_CELL_GI_STATICCACHE_WARM=1 WW_CELL_GI_DUMP="$(winpath "$run/dump_cache")")"
	e="$(shoot $cell red "${from[@]}" WW_CELL_GI_STATICCACHE=1 WW_CELL_GI_STATICCACHE_WARM=1 WW_CELL_GI_STATICCACHE_RED=withsun \
		WW_CELL_GI_DUMP="$(winpath "$run/dump_red")")"
	check "$cell: all five relights shot" "$([ "$a$b$c$d$e" = 11111 ] && echo 1 || echo 0)"
	r="$(samebins "$run/dump_rung" "$run/dump_full")"
	check "$cell: switch off: every .bin the rung dumps is byte for byte (${r#* } files)" "$([ "${r#* }" -gt 5 ] && echo "${r% *}" || echo 0)"
	r="$(samebins "$run/dump_full" "$run/dump_warm")"
	check "$cell: the warm-up relight alone leaves the dump the full relight's (${r#* } files)" "$([ "${r#* }" -gt 5 ] && echo "${r% *}" || echo 0)"
	grep -h "gi staticcache:" "$run/cache.notes" | sed "s/^/  /" | tee -a "$LOG"
	check "$cell: on: the warm-up missed and the dumped relight HIT the cache" \
		"$(grep -q "gi staticcache: miss" "$run/cache.notes" && grep -q "gi staticcache: HIT" "$run/cache.notes" && echo 1 || echo 0)"
	r="$(samebins "$run/dump_full" "$run/dump_cache")"
	check "$cell: on: cached placed lights + the sun traced = the full relight, every .bin byte for byte (${r#* } files)" \
		"$([ "${r#* }" -gt 5 ] && echo "${r% *}" || echo 0)"
	r="$(samebins "$run/dump_full" "$run/dump_red")"
	if [ "$cell" = ext ]; then
		check "$cell: red withsun (the old sun kept) differs from the full relight" "$([ "${r% *}" = 0 ] && echo 1 || echo 0)"
	else
		say "  $cell: red withsun $( [ "${r% *}" = 1 ] && echo "is the full relight (no sun in this interior: nothing to keep wrong)" || echo differs)"
	fi
	pd="$(python -c "import sys,numpy as np;from PIL import Image
a,b=(np.asarray(Image.open(f).convert('RGB'),int) for f in sys.argv[1:3]);print(int((np.abs(a-b).max(2)>0).sum()))" \
		"$(winpath "$run/cache.png")" "$(winpath "$run/full.png")" 2>/dev/null)"
	say "  $cell: cache.png against full.png: ${pd:-unreadable} pixels differ"
done
say "STATICCACHE1 GATE $([ $fails = 0 ] && echo PASS || echo "FAIL ($fails)")"
exit $fails
