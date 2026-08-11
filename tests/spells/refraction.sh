#!/usr/bin/env bash
#
# Screen-space refraction gate.
#
# tools/render_regression/capture.ps1 owns "did any pixel change since the last
# baseline". This owns the question that set could never answer: is the preview
# actually DOING anything, and does each of its inputs reach the shader?
#
# That distinction is the whole reason this file exists. The 2026-08-09s pass
# shipped a refraction path whose output was, on the file it was written for,
# byte-identical to copying the framebuffer straight through — and the
# regression set still said the case was fine, because "refraction_fixed
# changed" is satisfied by a shape that merely draws the background verbatim.
#
# Every check below therefore renders two frames that differ in EXACTLY ONE
# input and says which:
#
#   strength    Refraction Strength 1.0 vs 0.0, same frame  -> must differ
#   normal map  the normal texture slot cleared             -> must stop differing
#   animation   the file's own controller, walked in five   -> must RISE at every
#               steps along its linear ramp                    step, not just move
#   vertex alpha  0 / one-sided / 1.0                       -> must gate and scale it
#   billboard   a second camera                             -> must still differ
#
# Fixtures are DERIVED at run time, through NifSkope's own CLI, from a real file
# (default: the X-01 Tesla torso VFX, whose stock controller ramps Refraction
# Strength 0 -> 1 -> 0 over 4.93 s and whose refracting shape hangs off a
# NiBillboardNode). The source is copied first and its hash re-checked at the
# end: this never writes to the file it is handed.
#
#   bash tests/spells/refraction.sh
#   WW_REFRACTION_SRC=<other.nif> bash tests/spells/refraction.sh
#
# Artifacts: release/ww_refraction/*.png (kept, they are the visual evidence).

set -u

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="$REPO/release/NifSkope.exe"
# Every window this opens goes to the second monitor. WW_RENDER_SHOT still puts
# a REAL window on screen for a couple of seconds per capture, and this script
# opens eighteen of them, so without WW_WINDOW_AT a run walks all over whatever
# is on the primary display. _harness.sh owns the coordinates for every harness
# in this directory - set them there, not here.
. "$(dirname "$0")/_harness.sh"
WORK="$REPO/release/ww_refraction"
DIFF="$REPO/tools/render_regression/imgdiff.ps1"
SRC_IN="${WW_REFRACTION_SRC:-E:/Projects/Fallout 4 Mods/mods/X01Tesla/meshes/actors/powerarmor/x01/X01_Torso_VFX.nif}"

# Peak of the stock ramp - an authored key in the file. The trough (t = 0.0) and
# the intermediate samples come from RAMP_T below, so they are declared once.
T_PEAK=2.5

checks=0; fails=0
ok()   { checks=$((checks+1)); printf '  ok   %-36s %s\n' "$1" "$2"; }
bad()  { checks=$((checks+1)); fails=$((fails+1)); printf '  FAIL %-36s %s\n' "$1" "$2"; }
check(){ if [ "$1" = "1" ]; then ok "$2" "$3"; else bad "$2" "$3"; fi }

[ -x "$EXE" ] || { echo "FAIL  no release/NifSkope.exe"; exit 1; }
[ -f "$SRC_IN" ] || { echo "FAIL  source nif not found: $SRC_IN"; exit 1; }

rm -rf "$WORK"; mkdir -p "$WORK"
SRC_HASH_BEFORE=$(sha256sum "$SRC_IN" | cut -d' ' -f1)
SRC="$WORK/src.nif"
cp "$SRC_IN" "$SRC"

cli() { "$EXE" -no-gui "$@" 2>&1; }
nget() { cli get -b "$1" -f "$2" "$3" | tail -1; }
nset() { # in out block field value
	cli set -b "$3" -f "$4" -v "$5" -o "$2" "$1" >/dev/null 2>&1
	[ -f "$2" ] || { echo "FAIL  set -b $3 -f $4 on $1"; exit 1; }
}

# Locate the pieces by type, not by hard-coded block number, so a re-export that
# renumbers the file does not silently test the wrong block.
SHAPE=$(cli list "$SRC" | grep -i "BSTriShape.*Refraction" | head -1 | sed 's/^\[\([0-9]*\)\].*/\1/')
BILLBOARD=$(cli list "$SRC" | grep "NiBillboardNode" | head -1 | sed 's/^\[\([0-9]*\)\].*/\1/')
[ -n "$SHAPE" ] || { echo "FAIL  no BSTriShape named *Refraction* in $SRC_IN"; exit 1; }
PROP=$(nget "$SHAPE" "Shader Property" "$SRC")
TEXSET=$(nget "$PROP" "Texture Set" "$SRC")
VERTS=$(nget "$SHAPE" "Num Vertices" "$SRC")

# Walk the property's controller chain for Controlled Variable 0, which is
# LSCF Refraction Strength.
CTRL=$(nget "$PROP" "Controller" "$SRC"); DATA=-1
while [ "$CTRL" -ge 0 ] 2>/dev/null; do
	if [ "$(nget "$CTRL" "Controlled Variable" "$SRC")" = "0" ]; then
		INTERP=$(nget "$CTRL" "Interpolator" "$SRC")
		DATA=$(nget "$INTERP" "Data" "$SRC")
		break
	fi
	CTRL=$(nget "$CTRL" "Next Controller" "$SRC")
done
[ "$DATA" -ge 0 ] 2>/dev/null || { echo "FAIL  no Refraction Strength controller on block $PROP"; exit 1; }
NKEYS=$(nget "$DATA" "Data/Num Keys" "$SRC")

echo "source   : $SRC_IN"
echo "fixtures : shape=[$SHAPE] prop=[$PROP] texset=[$TEXSET] strengthData=[$DATA] keys=$NKEYS verts=$VERTS billboard=[$BILLBOARD]"

set_all_keys() { # in out value
	local cur="$1" k=0 tmp
	while [ "$k" -lt "$NKEYS" ]; do
		tmp="$WORK/_k$k.nif"; nset "$cur" "$tmp" "$DATA" "Data/Keys/$k/Value" "$3"; cur="$tmp"; k=$((k+1))
	done
	cp "$cur" "$2"
}
# NOTE: `set` on a colour takes #AARRGGBB while `get` PRINTS #RRGGBBAA. Writing
# the printed form back silently rotates the channels - "#ffffff00" stores as
# #ffff00ff, i.e. alpha 255, which is exactly the mistake that made three
# different vertex-alpha fixtures render identically and look like a working
# check. Every write below is read back and asserted for that reason.
vert_z() { nget "$SHAPE" "Vertex Data/$1/Vertex" "$SRC" | sed 's/.*Z \(-\?[0-9.]*\).*/\1/'; }
set_all_alpha() { # in out alphahex|TOP|BOTTOM
	local cur="$1" v=0 tmp a z
	while [ "$v" -lt "$VERTS" ]; do
		a="$3"
		case "$3" in
			TOP)    a=$(awk -v z="$(vert_z $v)" -v m="$Z_MEDIAN" 'BEGIN{print (z>=m)?"ff":"00"}') ;;
			BOTTOM) a=$(awk -v z="$(vert_z $v)" -v m="$Z_MEDIAN" 'BEGIN{print (z< m)?"ff":"00"}') ;;
		esac
		tmp="$WORK/_v$v.nif"; nset "$cur" "$tmp" "$SHAPE" "Vertex Data/$v/Vertex Colors" "#${a}ffffff"; cur="$tmp"; v=$((v+1))
	done
	cp "$cur" "$2"
}
alpha_of() { nget "$SHAPE" "Vertex Data/$2/Vertex Colors" "$1" | sed 's/^#......//'; }

echo "deriving fixtures..."
set_all_keys "$SRC" "$WORK/strength_zero.nif" 0.0
set_all_keys "$SRC" "$WORK/strength_full.nif" 1.0
nset "$WORK/strength_full.nif" "$WORK/no_normalmap.nif" "$TEXSET" "Textures/1" ""
Z_MEDIAN=$(for v in $(seq 0 $((VERTS-1))); do vert_z $v; done | sort -g | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}')
set_all_alpha "$WORK/strength_full.nif" "$WORK/vertexalpha_0.nif"      00
set_all_alpha "$WORK/strength_full.nif" "$WORK/vertexalpha_1.nif"      ff
set_all_alpha "$WORK/strength_full.nif" "$WORK/vertexalpha_top.nif"    TOP
set_all_alpha "$WORK/strength_full.nif" "$WORK/vertexalpha_bottom.nif" BOTTOM
rm -f "$WORK"/_k*.nif "$WORK"/_v*.nif
echo "vertex split at local Z = $Z_MEDIAN"

# Result goes in $SHOT, never through command substitution: a bail-out inside
# $( ) only kills the subshell, and its message is captured as the filename.
SHOT=""
shot() { # nif name time [size] [view] [refr]
	SHOT="$WORK/$2.png"
	rm -f "$SHOT"
	export WW_RENDER_SHOT="$(cygpath -w "$SHOT")" WW_RENDER_TIME="$3"
	export WW_RENDER_SIZE="${4:-1280x800}" WW_RENDER_SEQ=autoLoop
	export WW_RENDER_REFRACTION="${6:-1}"
	if [ -n "${5:-}" ]; then export WW_RENDER_VIEW="$5"; else unset WW_RENDER_VIEW; fi
	timeout 120 "$EXE" "$1" >/dev/null 2>&1
	unset WW_RENDER_SHOT WW_RENDER_TIME WW_RENDER_SIZE WW_RENDER_SEQ WW_RENDER_REFRACTION WW_RENDER_VIEW
	[ -s "$SHOT" ] || { echo "FAIL  no framebuffer for $2"; exit 1; }
}
# field extractors over imgdiff.ps1's one-line result
D_LINE=""
d() { D_LINE=$(powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$DIFF")" -A "$(cygpath -w "$1")" -B "$(cygpath -w "$2")" | tr -d '\r' | tail -1); }
f() { echo "$D_LINE" | awk -v n="$1" '{print $n}'; }
dpx()   { f 1; }; dmean() { f 3; }; dfrac() { f 4; }
dx0()   { f 5; }; dy0()   { f 6; }; dx1()   { f 7; }; dy1() { f 8; }
dleft() { f 9; }; dright(){ f 10; }; dcx() { f 12; }; dcy() { f 13; }
ge() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }
lt() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a<b)}'; }

echo "checking the fixtures are what they claim to be"
check "$( [ "$(nget "$DATA" "Data/Keys/1/Value" "$WORK/strength_full.nif")" = "1" ] && echo 1 || echo 0 )" \
	"strength_full really carries 1.0" "key 1 = $(nget "$DATA" "Data/Keys/1/Value" "$WORK/strength_full.nif")"
check "$( [ "$(nget "$DATA" "Data/Keys/1/Value" "$WORK/strength_zero.nif")" = "0.0" ] && echo 1 || echo 0 )" \
	"strength_zero really carries 0.0" "key 1 = $(nget "$DATA" "Data/Keys/1/Value" "$WORK/strength_zero.nif")"
check "$( [ "$(alpha_of "$WORK/vertexalpha_0.nif" 0)" = "00" ] && [ "$(alpha_of "$WORK/vertexalpha_0.nif" $((VERTS-1)))" = "00" ] && echo 1 || echo 0 )" \
	"vertexalpha_0 really is alpha 0" "v0=$(alpha_of "$WORK/vertexalpha_0.nif" 0) v$((VERTS-1))=$(alpha_of "$WORK/vertexalpha_0.nif" $((VERTS-1)))"
check "$( [ "$(alpha_of "$WORK/vertexalpha_1.nif" 0)" = "ff" ] && echo 1 || echo 0 )" \
	"vertexalpha_1 really is alpha 255" "v0=$(alpha_of "$WORK/vertexalpha_1.nif" 0)"
check "$( [ "$(nget "$TEXSET" "Textures/1" "$WORK/no_normalmap.nif")" = "" ] && echo 1 || echo 0 )" \
	"no_normalmap really has no slot 1" "'$(nget "$TEXSET" "Textures/1" "$WORK/no_normalmap.nif")'"

echo "rendering..."
shot "$WORK/strength_zero.nif" zero_peak   $T_PEAK   ; Z_PEAK=$SHOT
shot "$WORK/strength_full.nif" full_peak   $T_PEAK   ; F_PEAK=$SHOT
shot "$WORK/no_normalmap.nif"  no_normalmap $T_PEAK  ; N_PEAK=$SHOT
shot "$WORK/vertexalpha_0.nif"      vertexalpha_0      $T_PEAK ; A0=$SHOT
shot "$WORK/vertexalpha_1.nif"      vertexalpha_1      $T_PEAK ; A1=$SHOT
shot "$WORK/vertexalpha_top.nif"    vertexalpha_top    $T_PEAK ; ATOP=$SHOT
shot "$WORK/vertexalpha_bottom.nif" vertexalpha_bottom $T_PEAK ; ABOT=$SHOT

# THE RAMP, SAMPLED.
#
# "peak differs from trough" only shows the controller is not stuck; it cannot
# tell a value that TRACKS the curve from one that flips between two states. So
# the curve is walked, and each sample is rendered TWICE - once from the file's
# own controller and once from strength_zero at the same scene time, which pins
# the fan's rotation so the controller's output is the only variable left.
#
# Data block [$DATA] is LINEAR (KeyGroup Interpolation = 1) between (0.0, 0.0)
# and (2.49999, 1.0), so the value the uniform must be receiving at each of
# these times is known in closed form. It is printed beside the measurement,
# which is what "log the strength per frame" buys without instrumenting a
# release build.
RAMP_T="0.0 0.625 1.25 1.875 2.5"
RAMP_EXPECT="0.00 0.25 0.50 0.75 1.00"
for t in $RAMP_T; do
	tag=$(echo "$t" | tr . _)
	shot "$WORK/strength_zero.nif" "ramp_zero_$tag" "$t"
	shot "$SRC"                    "ramp_anim_$tag" "$t"
done
Z_TRO="$WORK/ramp_zero_0_0.png"; O_TRO="$WORK/ramp_anim_0_0.png"
O_PEAK="$WORK/ramp_anim_2_5.png"

echo
echo "1. the distortion exists, and the normal map is what makes it"
d "$Z_PEAK" "$F_PEAK"; S_PX=$(dpx); S_MEAN=$(dmean); S_FRAC=$(dfrac)
check "$( [ "$S_PX" -ge 2000 ] && echo 1 || echo 0 )" "strength 1.0 vs 0.0 differs" \
	"$S_PX px, mean |d|=$S_MEAN, bbox ($(dx0),$(dy0))-($(dx1),$(dy1))"
# LOCAL, not global - the 08-09s guarantee. Reading strength as a fraction of the
# whole viewport spread the difference across the frame; a local haze cannot
# leave the shape's own footprint.
check "$( lt "$S_FRAC" 0.25 && echo 1 || echo 0 )" "and the change stays local" \
	"$S_FRAC of the frame"
# The offset is the SHADING normal, so clearing the map does not zero it - the
# plume's own curvature still bends the sample, exactly as a glass surface with
# no normal map would. What must be true is that the map is the dominant term.
d "$N_PEAK" "$F_PEAK"; NM_PX=$(dpx); NM_MEAN=$(dmean)
d "$Z_PEAK" "$N_PEAK"; NG_MEAN=$(dmean)
check "$( [ "$NM_PX" -ge 2000 ] && echo 1 || echo 0 )" "the normal map drives the pattern" \
	"$NM_PX px change when slot 1 is cleared, mean |d|=$NM_MEAN"
check "$( ge "$S_MEAN" "$NG_MEAN" && echo 1 || echo 0 )" "and it adds to bare geometry" \
	"mapped mean $S_MEAN >= flat-normal mean $NG_MEAN"

echo
echo "2. every strength route"
# Trough and peak are the SAME file at two times, so the fan's rotation is
# identical in each pair and the only variable left is the controller's output.
d "$Z_TRO" "$O_TRO"
check "$( [ "$(dpx)" -eq 0 ] && echo 1 || echo 0 )" "animated: trough == strength 0" "$(dpx) px"
d "$Z_PEAK" "$O_PEAK"; P_PX=$(dpx); P_MEAN=$(dmean)
check "$( [ "$P_PX" -ge 2000 ] && echo 1 || echo 0 )" "animated: peak distorts" \
	"$P_PX px, mean |d|=$P_MEAN"
check "$( ge "$S_MEAN" "$P_MEAN" && echo 1 || echo 0 )" "forced 1.0 >= the file's own peak" \
	"forced mean $S_MEAN >= animated mean $P_MEAN"

# The ramp has to GROW, not merely be non-zero somewhere. Total |delta| (changed
# pixels x mean |delta|) is the measure, because a stronger displacement widens
# the disturbed area AND deepens it, and the sum is the only one of the three
# that moves with both.
printf '     %-7s %-9s %-9s %-9s %s\n' t strength px 'mean|d|' 'sum|d|'
RAMP_PREV=-1; RAMP_RISES=1; RAMP_N=0
set -- $RAMP_EXPECT
for t in $RAMP_T; do
	tag=$(echo "$t" | tr . _)
	d "$WORK/ramp_zero_$tag.png" "$WORK/ramp_anim_$tag.png"
	R_PX=$(dpx); R_MEAN=$(dmean)
	R_SUM=$(awk -v p="$R_PX" -v m="$R_MEAN" 'BEGIN{printf "%.0f", p*m}')
	printf '     %-7s %-9s %-9s %-9s %s\n' "$t" "$1" "$R_PX" "$R_MEAN" "$R_SUM"
	[ "$R_SUM" -gt "$RAMP_PREV" ] || RAMP_RISES=0
	RAMP_PREV=$R_SUM; RAMP_N=$((RAMP_N+1)); shift
done
check "$( [ "$RAMP_RISES" = "1" ] && [ "$RAMP_N" -eq 5 ] && echo 1 || echo 0 )" \
	"the ramp rises at every step" "$RAMP_N samples, strictly increasing sum |d|"

echo
echo "3. vertex alpha modulates it"
d "$Z_PEAK" "$A0"
check "$( [ "$(dpx)" -eq 0 ] && echo 1 || echo 0 )" "vertex alpha 0 distorts nothing" "$(dpx) px"
d "$Z_PEAK" "$A1"; V1_PX=$(dpx); V1_MEAN=$(dmean)
check "$( [ "$V1_PX" -ge 2000 ] && ge "$V1_MEAN" "$S_MEAN" && echo 1 || echo 0 )" \
	"vertex alpha 1 distorts fully" "$V1_PX px, mean $V1_MEAN >= authored-gradient mean $S_MEAN"
# SPATIAL proof, and deliberately DIRECTION-FREE.
#
# Alpha on only the vertices above / below the mesh's median local Z must move
# WHERE the disturbance happens. Which way that is on SCREEN is not knowable
# from here: the shape hangs off a NiBillboardNode and its parent rotates, so
# the mapping from local +Z to screen -Y is whatever the billboard resolved to
# on the captured frame. An earlier revision asserted "top is higher up the
# image" and failed on a correct renderer for that reason alone (measured
# centroids: top y=287.6, bottom y=280.8 - separated, but not in the assumed
# order, while x was 637.6 vs 675.7).
#
# What IS knowable without a camera model: a shader that ignored C.a would draw
# all three alpha fixtures IDENTICALLY. Then top-vs-bottom would differ by zero
# pixels and the two disturbance centroids would coincide. Both are asserted, so
# this fails on the broken code rather than merely agreeing with the fixed code.
d "$Z_PEAK" "$A1";   CX_ALL=$(dcx); CY_ALL=$(dcy)
d "$Z_PEAK" "$ATOP"; TOP_PX=$(dpx); CX_TOP=$(dcx); CY_TOP=$(dcy)
d "$Z_PEAK" "$ABOT"; BOT_PX=$(dpx); CX_BOT=$(dcx); CY_BOT=$(dcy)
SPREAD=$(awk -v ax="$CX_TOP" -v ay="$CY_TOP" -v bx="$CX_BOT" -v by="$CY_BOT" \
	'BEGIN{printf "%.1f", sqrt((ax-bx)*(ax-bx)+(ay-by)*(ay-by))}')
d "$ATOP" "$ABOT"; TB_PX=$(dpx)
check "$( [ "$TOP_PX" -ge 500 ] && [ "$BOT_PX" -ge 500 ] && [ "$TB_PX" -ge 2000 ] && ge "$SPREAD" 20 && echo 1 || echo 0 )" \
	"alpha on half the verts moves it" \
	"top/bottom differ by $TB_PX px; centroids ($CX_TOP,$CY_TOP) vs ($CX_BOT,$CY_BOT) = $SPREAD px apart; all at ($CX_ALL,$CY_ALL); regions $TOP_PX / $BOT_PX px"

echo
echo "4. the billboard parent, other cameras, other resolutions"
check "$( [ -n "$BILLBOARD" ] && echo 1 || echo 0 )" "shape sits under a NiBillboardNode" "block [$BILLBOARD]"
shot "$WORK/strength_zero.nif" zero_left $T_PEAK 1280x800 2 ; ZL=$SHOT
shot "$WORK/strength_full.nif" full_left $T_PEAK 1280x800 2 ; FL=$SHOT
d "$ZL" "$FL"
check "$( [ "$(dpx)" -ge 500 ] && echo 1 || echo 0 )" "still distorts from another camera" "$(dpx) px"
# Resolution independence: the cap is a fraction of the viewport, so halving the
# render must not halve how strongly the image is disturbed. A fixed pixel cap
# fails this - the same 8 px is twice the picture at half the size.
shot "$WORK/strength_zero.nif" zero_half $T_PEAK 640x400 ; ZH=$SHOT
shot "$WORK/strength_full.nif" full_half $T_PEAK 640x400 ; FH=$SHOT
d "$ZH" "$FH"; HALF_MEAN=$(dmean)
RATIO=$(awk -v a="$HALF_MEAN" -v b="$S_MEAN" 'BEGIN{print (b>0)?a/b:0}')
check "$( ge "$RATIO" 0.6 && lt "$RATIO" 1.6 && echo 1 || echo 0 )" "half resolution, same strength" \
	"mean |d| $HALF_MEAN at 640x400 vs $S_MEAN at 1280x800 (ratio $RATIO)"

echo
echo "5. the Viewport Effects toggle still switches it off"
shot "$WORK/strength_full.nif" refraction_off $T_PEAK 1280x800 "" 0 ; OFF=$SHOT
d "$F_PEAK" "$OFF"
check "$( [ "$(dpx)" -ge 2000 ] && echo 1 || echo 0 )" "refraction off changes the shape" "$(dpx) px"

echo
echo "6. the FIRST frame the user is shown is already the settled frame"
#
# Everything above renders through WW_RENDER_SHOT, which before it grabs waits
# 2.5 s, resizes the window, hides every dock, runs a pick render and pumps two
# more updates. Each of those repairs startup state, so a defect that lives only
# until the first interaction cannot be seen by any of them. That is not
# hypothetical: for the whole life of the screen-space refraction preview,
# opening a refracting NIF drew a visibly warped background until you clicked in
# the viewport, and all 21 checks above were green throughout.
#
#     resizeGL 1485x925 valid=0        <- the only resize for the final size, and
#                                         the context is not valid yet, so
#                                         GLView::resizeGL returns before
#                                         setViewport()
#     paint 4  uniVP=1434x730  liveVP=1485x925
#     paint 10 uniVP=1485x925  liveVP=1485x925   <- after indexAt(), i.e. a click
#
# fo4_default.frag divides gl_FragCoord by that uniform to find its background
# sample, so a stale one fetches the wrong texels and the shape shows a rescaled
# copy of the scene behind it. Frame 1 differed from the settled frame by 41507
# px (max channel delta 108); it is 8 px (max 1) now.
#
# WW_FIRSTFRAME_TEST changes NOTHING that could heal it - no resize, no docks, no
# camera, no scene time. It renders the document's own first frames, pumps two
# repaints, runs one indexAt() (the click), pumps two more, and GLView writes
# every one of them through the same readback. So frame 1 and the last frame are
# comparable by construction and the only variable left is first-frame state.
FF="$WORK/firstframe"
ff_run() { # outdir refractionOn
	rm -rf "$1"; mkdir -p "$1"
	WW_FRAME_SHOTS="$(winpath "$1/f")" WW_FRAME_SHOTS_MAX=16 WW_FIRSTFRAME_TEST=1 \
		WW_RENDER_REFRACTION="$2" timeout 120 "$EXE" "$SRC" >/dev/null 2>&1
}
ff_run "$FF/on"  1
ff_run "$FF/off" 0
FF_N=$(ls "$FF/on" 2>/dev/null | wc -l)
FF_FIRST="$FF/on/f001.png"
FF_LAST=$(ls "$FF/on"/*.png 2>/dev/null | tail -1)
check "$( [ "$FF_N" -ge 4 ] && [ -s "$FF_FIRST" ] && echo 1 || echo 0 )" \
	"the run captured its own frames" "$FF_N frames, last $(basename "${FF_LAST:-none}")"
# ANTI-VACUITY. If the refraction path did not run in frame 1 - shape missing,
# effect toggled off, file not loaded - frame 1 would trivially equal the settled
# frame and this section would pass while measuring nothing. So the same first
# frame is rendered with the effect off and must differ.
d "$FF_FIRST" "$FF/off/f001.png"; FF_EFFECT=$(dpx)
check "$( [ "$FF_EFFECT" -ge 2000 ] && echo 1 || echo 0 )" \
	"the effect is in frame 1 at all" "$FF_EFFECT px vs the same frame with refraction off"
# THE INVARIANT. Tolerance is small and deliberately two-sided: a handful of
# 1-level pixels is the run-to-run noise of the particle/lightning geometry
# between repaints (measured 6-8 px, max channel delta 1), while the defect this
# guards was five orders of magnitude larger in count and 108 levels deep.
d "$FF_FIRST" "$FF_LAST"; FF_PX=$(dpx); FF_MAX=$(f 2); FF_MEAN=$(dmean)
check "$( [ "$FF_PX" -le 200 ] && [ "$FF_MAX" -le 4 ] && echo 1 || echo 0 )" \
	"frame 1 already equals the settled frame" \
	"$FF_PX px, max channel delta $FF_MAX, mean |d| $FF_MEAN (gate: <= 200 px and <= 4)"

echo
SRC_HASH_AFTER=$(sha256sum "$SRC_IN" | cut -d' ' -f1)
check "$( [ "$SRC_HASH_BEFORE" = "$SRC_HASH_AFTER" ] && echo 1 || echo 0 )" "source nif untouched" "${SRC_HASH_AFTER:0:16}"

echo
echo "refraction.sh: $checks checks, $fails failures"
[ "$fails" -eq 0 ]
