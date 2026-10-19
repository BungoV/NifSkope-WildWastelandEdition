#!/bin/bash
#
# THE AIR'S GROUND LAYER FROM .recx FILES (lane VOLAIR1, 2026-10-14; src/recx.h, docs/RECX.md, src/gl/cellvolair.cpp
# VaLayer). Not judged by eye. Sanctuary in Lookdev (CommonwealthFoggy, 18:00, day 4, the Fog row on, the sky rows
# off), the air on with the directional light alone (terms 1), a dump per shot. The .recx roots are made here, under
# $OUT/roots, and pinned with WW_RECX_ROOT (the shipped files and the game's Data play no part):
#   none   an empty root: the records' own layer (the near band's share), no ground layer
#   form   CommonwealthFoggy by "Fallout4.esm|0x001C3473", strength keyed by hour (17: 2, 19: 6 -> 4 at 18:00),
#          thickness 600; the same root also holds a wildcard entry for "Commonwealth*" (strength 9) that must lose,
#          another weather's entry and an unknown block (both ignored)
#   nosc   the form root under the red nosidecar (the files ignored)
#   off    the form root under the red noground (the far band alone)
#   wild   only a wildcard "CommonwealthFog*" (strength 5, thickness 300)
#   edid   a bare-EditorID entry (strength 2.5, thickness 400) beside a wildcard (strength 9)
#   conf   two files of one provider claiming the form at one rank (a_: strength 3; b_: strength 7): a logged conflict,
#          a_ wins
#   prov   three providers with no plugin (merge order: by name): AMod groundFog 1.5 / 700, BMod only a climate block,
#          ZMod groundFog 3.5 / 350: ZMod wins the block (the later provider), AMod's claim reported as overridden
# The files sit under Plugins/<Provider>/Weathers (GateMod unless named); the ids are 8 hex digits.
# Then tests/spells/volair1_ground_check.py (its header says what each stage is).
#
# RED CONTROLS (each must FAIL a stage):
#   --red noground   the form shot drawn with the far band alone            V T
#   --red nosidecar  the form shot with the files ignored                  R V T
#
# USAGE  bash tests/spells/volair1_ground.sh [--red NAME]   under the nifskope lock; RECHECK=1 judges again
set -u
. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-volair1_ground}"
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
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
case "$RED" in ""|noground|nosidecar) ;; *) echo "unknown red: $RED"; exit 2 ;; esac
OUT="${OUT:-$REPO/scratchpad/volair1_20261014/ground${RED:+_red_$RED}}"
GREEN="${GREEN:-$REPO/scratchpad/volair1_20261014/ground}"
PORT="${PORT:-14934}"
SIZE="${SIZE:-960x600}"
RECHECK="${RECHECK:-0}"
mkdir -p "$OUT"
LOG="$OUT/volair1_ground.log"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
say "volair1_ground.sh $(date '+%F %T')${RED:+  RED CONTROL: $RED (must fail)}  out: $OUT"

# the roots
W=Plugins/GateMod/Weathers
mkroot() { rm -rf "$OUT/roots/$1"; mkdir -p "$OUT/roots/$1/$W"; }
mkroot empty
mkroot form
cat > "$OUT/roots/form/$W/CommonwealthFoggy.recx" <<'EOF'
{ "entries": [
	{ "name": "CommonwealthFoggy", "weather": "Fallout4.esm|0x001C3473",
	  "groundFog": { "thickness": 600, "strength": 1, "hours": { "17": { "strength": 2 }, "19": { "strength": 6 } } },
	  "climate": { "anything": 1 } },
	{ "name": "CommonwealthClear", "weather": "Fallout4.esm|0x0002B52A", "groundFog": { "thickness": 50, "strength": 20 } }
] }
EOF
cat > "$OUT/roots/form/$W/CommonwealthFamily.recx" <<'EOF'
{ "entries": [ { "name": "family", "editorId": "Commonwealth*", "groundFog": { "thickness": 100, "strength": 9 } } ] }
EOF
mkroot wild
cat > "$OUT/roots/wild/$W/CommonwealthFogFamily.recx" <<'EOF'
{ "entries": [ { "name": "fog family", "editorId": "CommonwealthFog*", "groundFog": { "thickness": 300, "strength": 5 } } ] }
EOF
mkroot edid
cat > "$OUT/roots/edid/$W/CommonwealthFoggy.recx" <<'EOF'
{ "entries": [ { "name": "CommonwealthFoggy", "weather": "CommonwealthFoggy", "groundFog": { "thickness": 400, "strength": 2.5 } } ] }
EOF
cat > "$OUT/roots/edid/$W/CommonwealthFamily.recx" <<'EOF'
{ "entries": [ { "name": "family", "editorId": "Commonwealth*", "groundFog": { "thickness": 100, "strength": 9 } } ] }
EOF
mkroot prov
mkdir -p "$OUT/roots/prov/Plugins/AMod/Weathers" "$OUT/roots/prov/Plugins/BMod/Weathers" "$OUT/roots/prov/Plugins/ZMod/Weathers"
cat > "$OUT/roots/prov/Plugins/AMod/Weathers/CommonwealthFoggy.recx" <<'EOF'
{ "entries": [ { "name": "CommonwealthFoggy", "weather": "Fallout4.esm|0x001C3473", "groundFog": { "thickness": 700, "strength": 1.5 } } ] }
EOF
cat > "$OUT/roots/prov/Plugins/BMod/Weathers/CommonwealthFoggy.recx" <<'EOF'
{ "entries": [ { "name": "CommonwealthFoggy", "weather": "Fallout4.esm|0x001C3473", "climate": { "anything": 2 } } ] }
EOF
cat > "$OUT/roots/prov/Plugins/ZMod/Weathers/CommonwealthFoggy.recx" <<'EOF'
{ "entries": [ { "name": "CommonwealthFoggy", "weather": "Fallout4.esm|0x001C3473", "groundFog": { "thickness": 350, "strength": 3.5 } } ] }
EOF
mkroot conf
cat > "$OUT/roots/conf/$W/a_CommonwealthFoggy.recx" <<'EOF'
{ "entries": [ { "name": "CommonwealthFoggy", "weather": "Fallout4.esm|0x001C3473", "groundFog": { "thickness": 500, "strength": 3 } } ] }
EOF
cat > "$OUT/roots/conf/$W/b_CommonwealthFoggy.recx" <<'EOF'
{ "entries": [ { "name": "CommonwealthFoggy", "weather": "Fallout4.esm|0x001C3473", "groundFog": { "thickness": 500, "strength": 7 } } ] }
EOF

LD=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthFoggy WW_LOOKDEV_HOUR=18 WW_LOOKDEV_DAY=4 WW_LOOKDEV_GROUND=0
	WW_LOOKDEV_PLUGINS="$ESM" WW_LOOKDEV_CLOUDTIME=0 WW_LOOKDEV_FOG=1 WW_LOOKDEV_SHADOWS=0
	WW_LOOKDEV_SKY=0 WW_LOOKDEV_SUN=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 )

shoot() {	# shoot <tag> <root> [env...]
	local tag="$1" root="$2"; shift 2
	rm -f "$OUT/$tag.png" "$OUT/$tag.notes" "$OUT/$tag.bin" "$OUT/$tag.bin.txt"
	local scope; scope="$(fresh_scope)"
	env WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 WW_CELL_FX_RED=hide WW_CELL_DECAL_RED=none "${LD[@]}" \
		WW_CELL_OPEN="$ESM|Commonwealth|-19,22|1" WW_CELL_DATAROOT="$DATA" WW_LODGEN_RESOURCES="$DATA" \
		WW_CELL_AIR=1 WW_CELL_AIR_TERMS=1 WW_CELL_AIR_OPT=0 WW_RECX_ROOT="$(winpath "$OUT/roots/$root")" "$@" \
		WW_CELL_AIR_DUMP="$(winpath "$OUT/$tag.bin")" \
		WW_RENDER_VIEW=5 WW_RENDER_CENTER=-75776,92160,8400 WW_RENDER_DIST=20 WW_RENDER_FOV=70 \
		WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 WW_SETTINGS_SCOPE="$scope" \
		timeout 1500 "$EXE" --port "$PORT" "$(winpath "$REPO/tests/fixtures/empty.wwcell")" > "$OUT/$tag.notes" 2>&1
	say "  $tag $([ -s "$OUT/$tag.png" ] && echo shot || echo NO-PICTURE)  $(grep -o 'cell air recx: .*' "$OUT/$tag.notes" | tail -1)"
}
if [ "$RECHECK" != 1 ]; then
	[ -x "$EXE" ] || { echo "no exe $EXE"; exit 2; }
	if [ -z "$RED" ]; then
		shoot none empty
		shoot form form
		shoot nosc form WW_CELL_AIR_RED=nosidecar
		shoot off form WW_CELL_AIR_RED=noground
		shoot wild wild
		shoot edid edid
		shoot conf conf
		shoot prov prov
	else
		for t in none nosc off wild edid conf prov; do
			for x in png notes bin bin.txt; do cp "$GREEN/$t.$x" "$OUT/$t.$x" 2>/dev/null; done
		done
		shoot form form WW_CELL_AIR_RED="$RED"
	fi
fi
VOLAIR1_RED="$RED" python "$REPO/tests/spells/volair1_ground_check.py" "$OUT" 2>&1 | tee -a "$LOG"
exit "${PIPESTATUS[0]}"
