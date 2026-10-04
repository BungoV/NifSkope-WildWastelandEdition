#!/bin/bash
#
# THE LIT MEDIUM IN THE CELL VIEW (lane VOLFOG1, 2026-10-04; src/gl/cellvolfog.h, docs/cloud/VOLFOG1_DESIGN.md).
#
# Not judged by eye. Every shot: Cell lights on, the imagespace off, the effects hidden (WW_CELL_FX_RED=hide), the
# placed decals off (WW_CELL_DECAL_RED=none: with them on the SAME exe differs from itself, lane SSR1),
# 960 x 600. Exteriors in Lookdev (CommonwealthFoggy, 08:00, day 4, the Fog row pinned on, the sky rows off);
# the interior Vault111Cryo with its own fog. Per place:
#   dump     WW_VOLFOG=1 WW_VOLFOG_TERMS=1 (the directional light alone: the pixels stay stable; interiors 2, the placed
#            shaft lights: the game's interiors carry no directional light) + WW_VOLFOG_DUMP:
#            the injected and the integrated volumes and every number a rebuild needs; on.png = its picture
#   probe    the same with WW_VOLFOG_PROBE=1: each fragment writes the volume term it adds, raw
#   geo      the volume off, the fog's geometry probe (8: d / 16384 per fragment)
#   off      the volume off (the row's shipped state)          before   the exe from before the lane, its shaders
#   rung2    the before exe again: its own run-to-run floor for stage O
# Sanctuary also:  shad / shadref  the sun's cascades on (WW_LOOKDEV_SHADOWS=1), terms 1, with and without
#                  WW_VOLFOG_RED=noshadow                        time  all terms + a dump (its ms: the cost)
# Vault111Cryo also:  gi / giflat  the probe GI baked here, terms 4 (the GI alone), with and without red flat
# Then tests/spells/volfog1_cell_check.py (stages R I P A O S G F C; its header says what each is).
# The directional light, the phase, the slices, the integration and the apply are rebuilt independently;
# the placed lights and the GI are pinned OFF in R/P/A (WW_VOLFOG_TERMS=1, WW_CELL_GI=0) and gated by S/G/F and
# the cloud twin (tests/spells/volfog1_check.py: the closed coloured box).
#
# RED CONTROLS (each must FAIL its stage):
#   --red off       computed, not applied                         P A
#   --red wrongsrc  a fixed medium in place of the fog records    R
#   --red noshadow  the cascades ignored in the medium            S
#   --red gioff     the medium takes no GI                        G
#   --red flat      one GI cube for every froxel                  F
# Lane VOLFOG1b: Vault111Cryo also shoots time6 = time with WW_VOLFOG_RED=sixreads (the VOLFOG1 GI reads); stage K
# needs the one-lookup read cheaper than that on the same camera, same exe.
#
# USAGE  bash tests/spells/volfog1_cell.sh [--red off|wrongsrc|noshadow|gioff|flat]   RECHECK=1 judges again
# Run under the nifskope lock (withlock.sh nifskope ...).

set -u
. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-$REPO/scratchpad/volfog1_20261004/before_run/NifSkope.exe}"
SCOPE="${SCOPE:-volfog1_cell}"
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
case "$RED" in ""|off|wrongsrc|noshadow|gioff|flat) ;; *) echo "unknown red: $RED"; exit 2 ;; esac
OUT="${OUT:-$REPO/scratchpad/volfog1_20261004/gate${RED:+_red_$RED}}"
GREEN="${GREEN:-$REPO/scratchpad/volfog1_20261004/gate}"	# the green run a red copies its other shots from
PORT="${PORT:-14933}"
SIZE="${SIZE:-960x600}"
PLACES="${PLACES:-sanctuary concord Vault111Cryo}"
RECHECK="${RECHECK:-0}"
GI_PLACES="${GI_PLACES:-Vault111Cryo}"
declare -A XY=( [sanctuary]="-19,22" [concord]="-15,17" )
declare -A AT=( [sanctuary]="-75776,92160,8400" [concord]="-60200,73500,6350" [Vault111Cryo]="-4564,-294,150" )
declare -A VW=( [sanctuary]=5 [concord]=5 [Vault111Cryo]=4 )
declare -A DI=( [sanctuary]=20 [concord]=20 [Vault111Cryo]=500 )
LD=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthFoggy WW_LOOKDEV_HOUR=8 WW_LOOKDEV_DAY=4 WW_LOOKDEV_GROUND=0
	WW_LOOKDEV_PLUGINS="$ESM" WW_LOOKDEV_CLOUDTIME=0 WW_LOOKDEV_FOG=1 WW_LOOKDEV_SHADOWS=0
	WW_LOOKDEV_SKY=0 WW_LOOKDEV_SUN=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 )

mkdir -p "$OUT"
LOG="$OUT/volfog1_cell.log"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
say "volfog1_cell.sh $(date '+%F %T')${RED:+  RED CONTROL: $RED (must fail)}  out: $OUT"

shoot() {	# shoot <exe> <place> <tag> [env...]
	local exe="$1" place="$2" tag="$3"; shift 3
	local run="$OUT/$place" open=()
	mkdir -p "$run"
	rm -f "$run/$tag.png" "$run/$tag.notes"
	if [ -n "${XY[$place]:-}" ]; then
		open=( "${LD[@]}" WW_CELL_OPEN="$ESM|Commonwealth|${XY[$place]}|1" )
	else
		open=( WW_CELL_OPEN="$ESM|interior|$place" )
	fi
	local cam=( WW_RENDER_VIEW="${VW[$place]:-1}" )
	[ -n "${AT[$place]:-}" ] && cam+=( WW_RENDER_CENTER="${AT[$place]}" WW_RENDER_DIST="${DI[$place]:-1400}" WW_RENDER_FOV=70 )
	env WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 WW_CELL_FX_RED=hide WW_CELL_DECAL_RED=none "${open[@]}" "$@" "${cam[@]}" \
		WW_CELL_DATAROOT="$DATA" WW_LODGEN_RESOURCES="$DATA" \
		WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout "${SHOT_TIMEOUT:-1500}" "$exe" --port "$PORT" \
		"$(winpath "$REPO/tests/fixtures/empty.wwcell")" > "$run/$tag.notes" 2>&1
	say "  $place/$tag $([ -s "$run/$tag.png" ] && echo shot || echo NO-PICTURE)  $(grep -o 'volfog=[^)]*)' "$run/$tag.notes" | tail -1 | cut -c1-220)"
}
dumpenv() { echo WW_VOLFOG_DUMP="$(winpath "$OUT/$1/$2.bin")"; }

if [ "$RECHECK" != 1 ]; then
	[ -x "$EXE" ] || { echo "no exe $EXE"; exit 2; }
	[ -n "$RED" ] && [ -d "$GREEN" ] && cp -rn "$GREEN"/. "$OUT"/ 2>/dev/null
	R=(); [ -n "$RED" ] && R=( WW_VOLFOG_RED="$RED" )
	for p in $PLACES; do
		say "== $p"
		ext=0; [ -n "${XY[$p]:-}" ] && ext=1
		geo=( WW_CELL_FOG_PROBE=8 ); [ $ext = 1 ] && geo=( WW_LOOKDEV_FOGPROBE=0,0,8 )
		tm=1; [ $ext = 0 ] && tm=2	# interiors: the placed shaft lights (the game's interiors carry no directional light)
		case "$RED" in ""|off|wrongsrc)
			shoot "$EXE" "$p" on WW_VOLFOG=1 WW_VOLFOG_TERMS=$tm "${R[@]}" "$(dumpenv "$p" vol)"
			shoot "$EXE" "$p" probe WW_VOLFOG=1 WW_VOLFOG_TERMS=$tm WW_VOLFOG_PROBE=1 "${R[@]}" ;;
		esac
		if [ -z "$RED" ]; then
			shoot "$EXE" "$p" geo WW_VOLFOG=0 "${geo[@]}"
			shoot "$EXE" "$p" off WW_VOLFOG=0
			shoot "$BEFORE" "$p" before
			shoot "$BEFORE" "$p" rung2
		fi
		if [ "$p" = sanctuary ]; then
			case "$RED" in ""|noshadow)
				shoot "$EXE" "$p" shad WW_VOLFOG=1 WW_VOLFOG_TERMS=1 WW_LOOKDEV_SHADOWS=1 "${R[@]}" "$(dumpenv "$p" shad)" ;;
			esac
			if [ -z "$RED" ]; then
				shoot "$EXE" "$p" shadref WW_VOLFOG=1 WW_VOLFOG_TERMS=1 WW_LOOKDEV_SHADOWS=1 WW_VOLFOG_RED=noshadow "$(dumpenv "$p" shadref)"
				shoot "$EXE" "$p" time WW_VOLFOG=1 WW_VOLFOG_TIME=1 WW_LOOKDEV_SHADOWS=1 "$(dumpenv "$p" time)"
			fi
		fi
		if [ $ext = 0 ] && [[ " $GI_PLACES " == *" $p "* ]]; then
			# the probes as tests/spells/cell_gi.sh places and bakes them (the first run bakes, the rest relight it)
			pr=( WW_CELL_PROBES="$(winpath "$OUT/$p/probes.tsv")" WW_CELL_PROBES_HIDE=1 WW_CELL_BAKE_REFRACT_RED=keep )
			from=( "${pr[@]}" WW_CELL_GI_FROM="$(winpath "$OUT/$p/bake")" )
			bake=( "${pr[@]}" WW_CELL_PROBE_SOUP="$(winpath "$OUT/$p/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$OUT/$p/bake")" )
			[ -s "$OUT/$p/bake/probes.tbk" ] || ls "$OUT/$p/bake/"*.tbk > /dev/null 2>&1 && bake=( "${from[@]}" )
			case "$RED" in ""|gioff|flat)
				shoot "$EXE" "$p" gi WW_CELL_GI=1 "${bake[@]}" WW_VOLFOG=1 WW_VOLFOG_TERMS=4 "${R[@]}" "$(dumpenv "$p" gi)" ;;
			esac
			if [ -z "$RED" ]; then
				shoot "$EXE" "$p" giflat WW_CELL_GI=1 "${from[@]}" WW_VOLFOG=1 WW_VOLFOG_TERMS=4 \
					WW_VOLFOG_RED=flat "$(dumpenv "$p" giflat)"
				shoot "$EXE" "$p" time WW_VOLFOG=1 WW_VOLFOG_TIME=1 WW_CELL_GI=1 "${from[@]}" "$(dumpenv "$p" time)"
				# lane VOLFOG1b: the same camera with the VOLFOG1 six surface reads (red sixreads): the cost's before
				shoot "$EXE" "$p" time6 WW_VOLFOG=1 WW_VOLFOG_TIME=1 WW_CELL_GI=1 "${from[@]}" WW_VOLFOG_RED=sixreads 					"$(dumpenv "$p" time6)"
			fi
		fi
	done
fi
VOLFOG1_RED="$RED" python "$REPO/tests/spells/volfog1_cell_check.py" "$OUT" $PLACES 2>&1 | tee -a "$LOG"
exit "${PIPESTATUS[0]}"
