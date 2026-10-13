#!/bin/bash
#
# VOLUMETRIC AIR IN THE CELL VIEW (lane VOLAIR1, 2026-10-13; src/gl/cellvolair.h). Replaces lane VOLFOG1's
# volfog1_cell.sh (the lit fog it gated is now the air).
#
# Not judged by eye. Every shot: Cell lights on, the imagespace off, the effects hidden (WW_CELL_FX_RED=hide), the
# placed decals off (WW_CELL_DECAL_RED=none), 960 x 600 (the cost shots 1920 x 1080). Exteriors in Lookdev
# (CommonwealthFoggy, 08:00, day 4, the Fog row pinned on: the air's density comes from the fog records, the sky
# rows off); the interior Vault111Cryo with its own fog. Per place:
#   vol      WW_CELL_AIR=1, outdoors WW_CELL_AIR_TERMS=1 (the directional light alone, no cascades: rebuilt here
#            exactly), indoors 2 (the placed lights: their light is judged present, the medium rebuilt) +
#            WW_CELL_AIR_DUMP (the injected and integrated volumes and every number a rebuild needs)
#   probe / probet   the same with WW_CELL_AIR_PROBE=1 (L, raw) / 2 (T, grey)
#   geo      the air off, the fog's geometry probe (8: d / 16384 per fragment)
#   off      the air off (the row's shipped state)        before / rung2   the exe from before the lane, twice
# Sanctuary also:  shad / shadref  the sun's cascades on, terms 1, with and without WW_CELL_AIR_RED=noshadow
#                  time            all terms, 1920 x 1080, a dump (its ms: the cost)
# Vault111Cryo also:  gi / giflat  the probe GI baked here, terms 4 (the GI alone), with and without red flat
#                     time         all terms + the GI, 1920 x 1080
# Goodneighbor (NEON_DIR set: a folder holding lane NEONGI1's bake_veil and bake_off, NEON_PROFILE its plugin
# profile): the Rexford camera (in-bounds), 22:00 CommonwealthClear, terms 4 (the GI alone), the bake WITH the
# neon emitters (veil) and WITHOUT them (noneon): the air near the signs must glow with their light.
# Then tests/spells/volair1_cell_check.py (stages M R I P O S L G F N C; its header says what each is) and the
# twins: tests/spells/volair1_check.py (the air's identities) and tests/spells/volfog1_check.py (the coloured
# bounce in a red room, the shafts; the GI read they model is unchanged).
#
# RED CONTROLS (each must FAIL its stage):
#   --red off       computed, not applied (the game's fog drawn)   P
#   --red wrongmed  twice the density, no height falloff           M
#   --red noshadow  the cascades ignored in the air                S
#   --red nolights  the placed lights put nothing in the air       L
#   --red gioff     the air takes no GI                            G N
#   --red flat      one GI cube for every froxel                   F
#
# USAGE  bash tests/spells/volair1_cell.sh [--red NAME]   RECHECK=1 judges again
# Run under the nifskope lock (withlock.sh nifskope ...).

set -u
. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-$REPO/scratchpad/volair1_20261013/before_run/NifSkope.exe}"
SCOPE="${SCOPE:-volair1_cell}"
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
case "$RED" in ""|off|wrongmed|noshadow|nolights|gioff|flat) ;; *) echo "unknown red: $RED"; exit 2 ;; esac
OUT="${OUT:-$REPO/scratchpad/volair1_20261013/gate${RED:+_red_$RED}}"
GREEN="${GREEN:-$REPO/scratchpad/volair1_20261013/gate}"	# the green run a red copies its other shots from
PORT="${PORT:-14934}"
SIZE="${SIZE:-960x600}"
BIG="${BIG:-1920x1080}"
PLACES="${PLACES:-sanctuary Vault111Cryo}"
NEON_DIR="${NEON_DIR:-}"
NEON_PROFILE="${NEON_PROFILE:-}"
[ -n "$NEON_DIR" ] && PLACES="$PLACES goodneighbor"
RECHECK="${RECHECK:-0}"
declare -A XY=( [sanctuary]="-19,22" )
declare -A AT=( [sanctuary]="-75776,92160,8400" [Vault111Cryo]="-4564,-294,150" )
declare -A VW=( [sanctuary]=5 [Vault111Cryo]=4 )
declare -A DI=( [sanctuary]=20 [Vault111Cryo]=500 )
LD=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthFoggy WW_LOOKDEV_HOUR=8 WW_LOOKDEV_DAY=4 WW_LOOKDEV_GROUND=0
	WW_LOOKDEV_PLUGINS="$ESM" WW_LOOKDEV_CLOUDTIME=0 WW_LOOKDEV_FOG=1 WW_LOOKDEV_SHADOWS=0
	WW_LOOKDEV_SKY=0 WW_LOOKDEV_SUN=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 )

mkdir -p "$OUT"
LOG="$OUT/volair1_cell.log"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
say "volair1_cell.sh $(date '+%F %T')${RED:+  RED CONTROL: $RED (must fail)}  out: $OUT"

# launch <notes> <exe> [args...]: straight, and once more through PowerShell when a straight launch leaves nothing
# (a bash-started exe has been seen to return at once with no output while the real process went on)
launch() {
	local notes="$1" exe="$2"; shift 2
	timeout "${SHOT_TIMEOUT:-1500}" "$exe" "$@" > "$notes" 2>&1
	local rc=$?
	if [ $rc = 0 ] && [ ! -s "$notes" ]; then
		local args="" a
		for a in "$@"; do args="$args,'${a//\'/\'\'}'"; done
		powershell -NoProfile -Command "\$p = Start-Process -FilePath '$(cygpath -w "$exe")' -ArgumentList ${args#,} -NoNewWindow -PassThru -RedirectStandardOutput '$(cygpath -w "$notes").out' -RedirectStandardError '$(cygpath -w "$notes").err'; \$null = \$p.Handle; if (-not \$p.WaitForExit(1500000)) { exit 124 }; exit \$p.ExitCode" < /dev/null
		rc=$?
		cat "$notes.out" "$notes.err" > "$notes" 2>/dev/null; rm -f "$notes.out" "$notes.err"
	fi
	return $rc
}

shoot() {	# shoot <exe> <place> <tag> [env...]
	local exe="$1" place="$2" tag="$3"; shift 3
	local run="$OUT/$place" open=() cam=() size="$SIZE"
	mkdir -p "$run"
	rm -f "$run/$tag.png" "$run/$tag.notes"
	case "$tag" in time*) size="$BIG" ;; esac
	if [ "$place" = goodneighbor ]; then
		local prof plugins res
		prof="$NEON_PROFILE"
		plugins="$(grep -E '^plugin [0-9]+: ' "$prof" | sed 's/^plugin [0-9]*: //' | tr -d '\r' | paste -sd, -)"
		res="$(grep -E '^resource [0-9]+: ' "$prof" | sed 's/^resource [0-9]*: //' | tr -d '\r' | paste -sd';' -)"
		printf 'fps 16\nframes 3\n' > "$run/rexford.path"
		for k in 0 1 2; do printf 'key %s 21300 -10200 730 21490 -9550 950 55\n' "$(python -c "print($k/16)")" >> "$run/rexford.path"; done
		open=( WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=22 WW_LOOKDEV_DAY=4 WW_LOOKDEV_CLOUDTIME=0
			WW_LOOKDEV_SKY=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_SUN=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_FOG=1
			WW_LOOKDEV_PLUGINS="$ESM" WW_LODGEN_RESOURCES="$res" WW_CELL_NOGRID=1 WW_CELL_BAKE_REFRACT_RED=keep
			WW_CELL_OPEN="$plugins|Goodneighbor|5,-3|1" )
		cam=( WW_RENDER_PATH="$(winpath "$run/rexford.path")" )
	elif [ -n "${XY[$place]:-}" ]; then
		open=( "${LD[@]}" WW_CELL_OPEN="$ESM|Commonwealth|${XY[$place]}|1" WW_CELL_DATAROOT="$DATA" WW_LODGEN_RESOURCES="$DATA" )
		cam=( WW_RENDER_VIEW="${VW[$place]}" WW_RENDER_CENTER="${AT[$place]}" WW_RENDER_DIST="${DI[$place]}" WW_RENDER_FOV=70 )
	else
		open=( WW_CELL_OPEN="$ESM|interior|$place" WW_CELL_DATAROOT="$DATA" WW_LODGEN_RESOURCES="$DATA" )
		cam=( WW_RENDER_VIEW="${VW[$place]}" WW_RENDER_CENTER="${AT[$place]}" WW_RENDER_DIST="${DI[$place]}" WW_RENDER_FOV=70 )
	fi
	local scope; scope="$(fresh_scope)"
	(	# launch is a function: the shot's environment exported in a subshell (env cannot run a function)
		export WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 WW_CELL_FX_RED=hide WW_CELL_DECAL_RED=none "${open[@]}" "$@" "${cam[@]}" \
			WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$size" WW_RENDER_CLEAN=1 WW_SETTINGS_SCOPE="$scope"
		launch "$run/$tag.notes" "$exe" --port "$PORT" "$(winpath "$REPO/tests/fixtures/empty.wwcell")"
	)
	if [ "$place" = goodneighbor ]; then
		[ -s "$run/${tag}_002.png" ] && mv "$run/${tag}_002.png" "$run/$tag.png"
		rm -f "$run/${tag}"_[0-9][0-9][0-9].png
	fi
	say "  $place/$tag $([ -s "$run/$tag.png" ] && echo shot || echo NO-PICTURE)  $(grep -o 'volair=[^)]*)' "$run/$tag.notes" | tail -1 | cut -c1-260)"
}
dumpenv() { echo WW_CELL_AIR_DUMP="$(winpath "$OUT/$1/$2.bin")"; }
room() {	# the heavy Goodneighbor launches wait for 10 GB free (two freezes on 10-03)
	local t=0
	while [ "$(awk '/MemFree/{print $2}' /proc/meminfo)" -lt 10240000 ] && [ $t -lt 1800 ]; do sleep 15; t=$((t+15)); done
	[ "$(awk '/MemFree/{print $2}' /proc/meminfo)" -ge 10240000 ] || { say "  under 10 GB free: no launch"; return 1; }
}

if [ "$RECHECK" != 1 ]; then
	[ -x "$EXE" ] || { echo "no exe $EXE"; exit 2; }
	[ -n "$RED" ] && [ -d "$GREEN" ] && cp -rn "$GREEN"/. "$OUT"/ 2>/dev/null
	R=(); [ -n "$RED" ] && R=( WW_CELL_AIR_RED="$RED" )
	for p in $PLACES; do
		say "== $p"
		if [ "$p" = goodneighbor ]; then
			case "$RED" in ""|gioff)
				for b in veil off; do
					room || continue
					bd="$NEON_DIR/bake_$b"
					shoot "$EXE" "$p" "neon_$b" WW_CELL_GI=1 WW_CELL_GI_FROM="$(winpath "$bd/bake")" \
						WW_CELL_PROBES="$(winpath "$bd/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
						WW_CELL_AIR=1 WW_CELL_AIR_TERMS=4 "${R[@]}" "$(dumpenv "$p" "neon_$b")"
				done ;;
			esac
			continue
		fi
		ext=0; [ -n "${XY[$p]:-}" ] && ext=1
		geo=( WW_CELL_FOG_PROBE=8 ); [ $ext = 1 ] && geo=( WW_LOOKDEV_FOGPROBE=0,0,8 )
		tm=1; [ $ext = 0 ] && tm=2
		case "$RED" in ""|off|wrongmed|nolights)
			shoot "$EXE" "$p" vol WW_CELL_AIR=1 WW_CELL_AIR_TERMS=$tm "${R[@]}" "$(dumpenv "$p" vol)" ;;
		esac
		case "$RED" in ""|off)
			shoot "$EXE" "$p" probe WW_CELL_AIR=1 WW_CELL_AIR_TERMS=$tm WW_CELL_AIR_PROBE=1 "${R[@]}"
			shoot "$EXE" "$p" probet WW_CELL_AIR=1 WW_CELL_AIR_TERMS=$tm WW_CELL_AIR_PROBE=2 "${R[@]}" ;;
		esac
		if [ -z "$RED" ]; then
			shoot "$EXE" "$p" geo WW_CELL_AIR=0 "${geo[@]}"
			shoot "$EXE" "$p" off WW_CELL_AIR=0
			shoot "$BEFORE" "$p" before
			shoot "$BEFORE" "$p" rung2
		fi
		if [ "$p" = sanctuary ]; then
			case "$RED" in ""|noshadow)
				shoot "$EXE" "$p" shad WW_CELL_AIR=1 WW_CELL_AIR_TERMS=1 WW_LOOKDEV_SHADOWS=1 "${R[@]}" "$(dumpenv "$p" shad)" ;;
			esac
			if [ -z "$RED" ]; then
				shoot "$EXE" "$p" shadref WW_CELL_AIR=1 WW_CELL_AIR_TERMS=1 WW_LOOKDEV_SHADOWS=1 WW_CELL_AIR_RED=noshadow "$(dumpenv "$p" shadref)"
				shoot "$EXE" "$p" time WW_CELL_AIR=1 WW_CELL_AIR_TIME=1 WW_LOOKDEV_SHADOWS=1 "$(dumpenv "$p" time)"
			fi
		fi
		if [ $ext = 0 ]; then
			# the probes as tests/spells/cell_gi.sh places and bakes them (the first run bakes, the rest relight it)
			pr=( WW_CELL_PROBES="$(winpath "$OUT/$p/probes.tsv")" WW_CELL_PROBES_HIDE=1 WW_CELL_BAKE_REFRACT_RED=keep )
			from=( "${pr[@]}" WW_CELL_GI_FROM="$(winpath "$OUT/$p/bake")" )
			bake=( "${pr[@]}" WW_CELL_PROBE_SOUP="$(winpath "$OUT/$p/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$OUT/$p/bake")" )
			ls "$OUT/$p/bake/"*.tbk > /dev/null 2>&1 && bake=( "${from[@]}" )
			case "$RED" in ""|gioff|flat)
				shoot "$EXE" "$p" gi WW_CELL_GI=1 "${bake[@]}" WW_CELL_AIR=1 WW_CELL_AIR_TERMS=4 "${R[@]}" "$(dumpenv "$p" gi)" ;;
			esac
			if [ -z "$RED" ]; then
				shoot "$EXE" "$p" giflat WW_CELL_GI=1 "${from[@]}" WW_CELL_AIR=1 WW_CELL_AIR_TERMS=4 \
					WW_CELL_AIR_RED=flat "$(dumpenv "$p" giflat)"
				shoot "$EXE" "$p" time WW_CELL_AIR=1 WW_CELL_AIR_TIME=1 WW_CELL_GI=1 "${from[@]}" "$(dumpenv "$p" time)"
			fi
		fi
	done
fi
VOLAIR1_RED="$RED" python "$REPO/tests/spells/volair1_cell_check.py" "$OUT" $PLACES 2>&1 | tee -a "$LOG"
exit "${PIPESTATUS[0]}"
