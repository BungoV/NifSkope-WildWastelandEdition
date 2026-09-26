#!/bin/bash
#
# The LOD channel preview on TERRAIN.
#
# The World LOD Generator's "Preview channel" box draws one generated vertex
# channel flat. It worked on object chunks and did nothing on terrain: every
# terrain channel rendered byte-identical. The mechanism (found by reading the
# renderer, then fixed): a shape caches the program it last drew with and hands
# it back as a hint; terrain resolves a PBRM material on its first ordinary
# paint, so its hint was pbrm_default.prog from then on - a program with no
# lodChannelView uniform - and switching the preview on only gated the choice
# of a NEW program, never the cached one.
#
# The gate is the property that failed: two channels of the same terrain chunk
# must render DIFFERENTLY, and either must differ from normal shading. Rendered
# headless through the WW_RENDER_SHOT hook with WW_RENDER_FLAT, which is the
# same path the Preview box drives (WW_LOD_CHANNEL sets the same global).
#
# What this found before it passed: the cached-program theory was real but not
# the cause. fo4_default.prog's conditions exclude Shader Type 18 (LOD
# landscape), and only fo4_default.frag carries the preview branch, so terrain
# never reached a program with the uniform at all - six renders, two modes,
# three channels, byte-identical in a fresh process. The fix routes FO4 shapes
# to fo4_default.prog by name while the preview is on.
#
# Top-down (WW_RENDER_VIEW=1): the default front view showed the chunk edge-on,
# a sliver a few hundred pixels wide, which is not a render to judge by.
#
# USAGE
#   bash tests/spells/lod_channel_preview.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
# ---- settings scope (lane FIX1 fix 4, 2026-09-26) ----------------------------
# Every NifSkope WINDOW this spell opens runs in its OWN QSettings scope -- never
# bungo's profile, and never a scope the caller's environment names. A gate that
# inherits the user's settings measures the profile, not the code (lane GATEFIX2,
# native_lighting.sh: his "Vertex Color" unticked in the Lighting shading mode
# turned the .BTR water white). WW_SETTINGS_SCOPE=<scope> moves the whole tree to
# HKCU\Software\NifTools\NifSkope 2.0 <scope> (src/harnesswindow.cpp).
# fresh_scope wipes it before EACH window (a window saves its layout on close, so
# the next would open at another size) and seeds Settings/Version=1: an EMPTY
# scope is a first install, whose settings dialog saves every pane's widget value
# (Background 46,46,46, src/ui/settingspane.cpp). SEED_REG=<file.reg> (keys
# already under the scope) is imported after the seed -- a red control's way to
# render under a chosen profile. The scope is wiped at exit. The -no-gui CLI
# calls are not windows and are left as they were.
SCOPE="${SCOPE:-lod_channel_preview}"
case "$SCOPE" in
	''|*[!A-Za-z0-9_-]*) echo "REFUSED: SCOPE='$SCOPE' is not a usable settings scope name"; exit 2 ;;
esac
[ ${#SCOPE} -le 40 ] || { echo "REFUSED: SCOPE='$SCOPE' is longer than 40"; exit 2; }
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {  # wipe + seed, then print the name: WW_SETTINGS_SCOPE="$(fresh_scope)"
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	# not a Game Manager first install either: version 0 shows an opaque progress dialog on the
	# PRIMARY monitor (src/gamemanager.cpp prog_dialog) before any WW window placement exists
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	# ...and the game manager state an empty scope never gets (its Game Folders come out empty):
	# Fallout 4's path and folders read from THIS MACHINE, never from the user's profile
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	if [ -n "${SEED_REG:-}" ]; then reg import "$(winpath "$SEED_REG")" > /dev/null 2>&1 || true; fi
	printf '%s' "$SCOPE"
}
wipe_scope
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PORT="${PORT:-42309}"
W="$(mktemp -d)"
trap 'rm -rf "$W"; wipe_scope' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }

# a terrain chunk with the generated channels in it
CHUNK="$W/Commonwealth.4.-20.24.BTR"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain -20 24 --dim 4 -o "$CHUNK" >/dev/null 2>&1
[ -s "$CHUNK" ] || { echo "FAIL: could not generate the terrain chunk"; exit 1; }

winpath() { cygpath -w "$1" 2>/dev/null || echo "$1"; }

shot() { # shot <channel> <png>
	WW_RENDER_SHOT="$(winpath "$2")" WW_RENDER_FLAT=1 WW_RENDER_VIEW=1 WW_LOD_CHANNEL="$1" WW_RENDER_SIZE=480x480 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 90 "$NS" --port "$PORT" "$CHUNK" >/dev/null 2>&1
	[ -s "$2" ]
}

fails=0
for c in 0 3 6; do
	if shot "$c" "$W/c$c.png"; then
		echo "  ok   channel $c rendered ($(wc -c < "$W/c$c.png") bytes)"
	else
		echo "  FAIL channel $c did not render"
		fails=$((fails + 1))
	fi
done
[ $fails -eq 0 ] || { echo "RESULT FAIL"; exit 1; }

# The property that was broken: distinct channels must look distinct on terrain.
if cmp -s "$W/c3.png" "$W/c6.png"; then
	echo "  FAIL AO (3) and wetness (6) render byte-identical on terrain"
	fails=$((fails + 1))
else
	echo "  ok   AO (3) and wetness (6) render differently on terrain"
fi
if cmp -s "$W/c0.png" "$W/c3.png"; then
	echo "  FAIL normal shading and the AO channel render byte-identical"
	fails=$((fails + 1))
else
	echo "  ok   the AO channel differs from normal shading"
fi
# A third pair, added with the ground-cover work (2026-09-06 TERRAIN1): the
# more channels there are, the more this gate matters, and two of them
# collapsing into one another is exactly what it was written to catch.
if shot 7 "$W/c7.png"; then
	if cmp -s "$W/c6.png" "$W/c7.png"; then
		echo "  FAIL wetness (6) and channel 7 render byte-identical on terrain"
		fails=$((fails + 1))
	else
		echo "  ok   channel 7 differs from wetness (6)"
	fi
else
	echo "  FAIL channel 7 did not render"
	fails=$((fails + 1))
fi
#
# WHAT THIS SCRIPT CANNOT SHOW, and why (docs/LODGEN_TERRAIN_VT.md §2):
# GROUND COVER is not one of the seven generated VERTEX channels WW_LOD_CHANNEL
# selects. It is a per-TEXEL field in the alpha of <ws>.<dim>.<x>.<y>_data.DDS,
# at 512x512, and the .btr's vertices carry no copy of it -- deliberately, since
# a per-vertex cover would be ~1,180 samples 480 units apart, which is the very
# problem the data sheet exists to fix. So there is nothing here for the preview
# box to draw, and the cover plane is gated instead by
# tests/spells/lodgen_ground_cover.sh, which measures the decoded alpha against
# an independent model of the paint. Showing cover in the viewport would need a
# terrain shader that samples the data sheet -- renderer work, not bake work.

[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
