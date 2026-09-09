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
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PORT="${PORT:-42309}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }

# a terrain chunk with the generated channels in it
CHUNK="$W/Commonwealth.4.-20.24.BTR"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain -20 24 --dim 4 -o "$CHUNK" >/dev/null 2>&1
[ -s "$CHUNK" ] || { echo "FAIL: could not generate the terrain chunk"; exit 1; }

winpath() { cygpath -w "$1" 2>/dev/null || echo "$1"; }

shot() { # shot <channel> <png>
	WW_RENDER_SHOT="$(winpath "$2")" WW_RENDER_FLAT=1 WW_RENDER_VIEW=1 WW_LOD_CHANNEL="$1" WW_RENDER_SIZE=480x480 \
		timeout 90 "$NS" --port "$PORT" "$CHUNK" >/dev/null 2>&1
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
