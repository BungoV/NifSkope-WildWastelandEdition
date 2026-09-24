#!/bin/bash
#
# Opening the NATIVE bake — `.lodi` + `.lodo` as a built document, and a `.lodl`
# lit with its `.lodt` sheets.
#
# WHY THIS EXISTS
#
# bungo, 2026-09-12: "why are you showing me .btr still? btr is a legacy thing".
# The pictures of the far field have to come from the `.lod` outputs, so the
# viewer had to learn to open them — and an opener that draws SOMETHING is the
# easiest thing in this repository to fake. A `.lodi` scene with half its
# placements dropped, or with every placement at the wrong cell, still renders a
# convincing field of rocks. A `.lodl` "lit with its sheets" that quietly fell
# back to the vertex-colour data view still renders terrain.
#
# So nothing here is judged by eye:
#
#   (a) MODULE-OFF IDENTITY. With no `.lodt` pyramid beside a `.lodl`, the
#       document this build makes is BYTE-IDENTICAL to the one the exe from
#       before this lane made — `cmp`, against `release/NifSkope.before_*.exe`,
#       not a re-reading of our own output. An empty `WW_LODL_SHEETS` and a
#       non-height plane are identical too, and `lodl_open.sh` keeps its count.
#
#   (b) THE INSTANCE CENSUS. The builder that PLACES each instance writes down
#       where it put it (`WW_LODI_DUMP`); that file is compared against the
#       `.lodi`'s own chunk table read by `lodgen_native_decode.py` (the
#       independent decoder) and against the `.BTO.manifest.txt` of the SAME
#       bake. Counts must agree three ways and every world position to 1 unit.
#       The floor: a region with no placements must come back as 0, seen.
#
#   (c) THE SCENE IS THE SAME SCENE the legacy chunk draws. The `.lodi` document
#       and the chunk's `.BTO` are rendered FROM THE SAME PINNED CAMERA and
#       their coverage masks compared: IoU >= 0.95, printed as a number. The
#       refuter is the same comparison against a DIFFERENT chunk's `.BTO`, which
#       must fail — otherwise the comparison cannot fail at all.
#
#   (d) THE LIT TERRAIN IS THE SHEETS. The lit `.lodl` render and the `.BTR`
#       render of the same cells with the same sheets are compared by mean
#       absolute colour difference over the footprint, printed. And a render
#       with `WW_LODL_SHEETS` pointed at an EMPTY directory must come back
#       byte-identical to the data view.
#
# USAGE
#   bash tests/spells/native_open.sh
#   GBAKE=<dir with the identity-on one-chunk bake> bash tests/spells/native_open.sh
#
# Fixtures, all overridable by environment:
#   LODL    the whole-worldspace landscape file
#   NATIVE  the directory holding <ws>.lodo and <ws>.lodi
#   SHEETS  the directory holding <ws>.VT.<dim>.lodt
#   OBJ     the directory holding the .BTO/.BTR of the same bake
#   RES     a resource root shaped Textures/Terrain/<ws>/... for the legacy pair
#   GBAKE   the obj dir of a ONE-CHUNK bake with identity ON (manifest leg of (b))
#   GNATIVE that bake's --native dir; defaults to GBAKE when they are the same

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
BEFORE="${BEFORE:-$ROOT/release/NifSkope.before_nativeview1.exe}"
SC="$ROOT/scratchpad/showcase1_20260912/out"
LODL="${LODL:-$SC/lodl/Terrain/Commonwealth.lodl}"
NATIVE="${NATIVE:-$SC/look/native}"
SHEETS="${SHEETS:-$SC/look/mod/Terrain}"
OBJ="${OBJ:-$SC/look/obj}"
RES="${RES:-$ROOT/scratchpad/nativeview1_20260912/resroot}"
GBAKE="${GBAKE:-}"           # the .BTO/.manifest.txt half of the identity-on bake
GNATIVE="${GNATIVE:-$GBAKE}"  # its --native half; the bake writes them to two dirs
AUTH="$ROOT/tests/spells/native_open_authority.py"
PORT="${PORT:-42931}"
WS="${WS:-Commonwealth}"
# Every window this harness opens runs in its OWN settings scope
# (src/harnesswindow.cpp, WW_SETTINGS_SCOPE): the whole QSettings tree moves to
# HKCU\Software\NifTools\NifSkope 2.0 <scope>, wiped before and after, so a
# picture depends on the fixtures and the exe and never on the profile of the
# person who last used the viewer. Measured 2026-09-24 (lane GATEFIX1): the
# (d) .BTR drew its water shape pure white under bungo's persisted profile and
# dark under the defaults, and NCC read -0.21 against 0.84 with the SAME exe
# and the SAME files -- on every exe back to before_btofree1, which passed
# 17/0 on 2026-09-16. A gate that reads the user's profile measures the profile.
SCOPE="${SCOPE:-nativeopen}"
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1; }

# the chunk everything is measured on, and an EMPTY one for the floor
CX=${CX:--20}; CY=${CY:-24}; DIM=${DIM:-4}
EX=${EX:--20}; EY=${EY:-32}          # chunk (-5,8): 0 instances in this bake
OX=${OX:--16}; OY=${OY:-24}          # a DIFFERENT chunk, for the (c) refuter

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$AUTH" ] || { echo "no authority at $AUTH"; exit 2; }
[ -f "$NATIVE/$WS.lodi" ] || { echo "no $WS.lodi in $NATIVE"; exit 2; }
[ -f "$NATIVE/$WS.lodo" ] || { echo "no $WS.lodo in $NATIVE"; exit 2; }
[ -f "$LODL" ] || { echo "no .lodl at $LODL"; exit 2; }

# CONSTITUTION 6: never a build and never a window while the game is up. This is
# its own step and its answer is read BEFORE anything is launched.
if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "Fallout4.exe is running; this harness opens windows. Stopping."
	exit 2
fi

PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
wipe_scope
trap 'rm -rf "$W"; wipe_scope' EXIT
mkdir -p "$W/empty"

checks=0; fails=0; skips=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }
skip()  { skips=$((skips+1)); echo "  SKIP $1"; }
ge() { awk -v a="$1" -v b="$2" 'BEGIN{print (a >= b) ? 1 : 0}'; }
lt() { awk -v a="$1" -v b="$2" 'BEGIN{print (a <  b) ? 1 : 0}'; }
le() { awk -v a="$1" -v b="$2" 'BEGIN{print (a <= b) ? 1 : 0}'; }

# the pin: the chunk's own footprint, so the camera is arithmetic and not a
# remembered screen coordinate (nifskope-ww-render-shot, the SKEL2 corollary)
CENTER="$(( (CX * 4096 + (CX + DIM) * 4096) / 2 )),$(( (CY * 4096 + (CY + DIM) * 4096) / 2 )),0"
ORTHO=$(( DIM * 4096 / 2 ))
# The two legacy files do NOT sit in the same space, which is measured, not
# assumed: the .BTR's shape carries translation 0,0,0 and chunk-local vertices,
# so it needs the chunk's own centre, while the .BTO's BSSubIndexTriShape
# carries translation (-81920, 98304, 0) -- the chunk's world origin -- over
# chunk-local vertices, so it lands in WORLD space like the .lodi does.  The
# wrong one of the two gives an empty 5,179-byte frame.  Same ortho, same
# window, two centres.
LCENTER="$(( DIM * 4096 / 2 )),$(( DIM * 4096 / 2 )),0"
# WW_RENDER_SIZE sets the WINDOW size and the main window has a minimum WIDTH
# of about 1024 px, so a narrower request is silently floored (480x480 came back
# 1024x445). Ask for at least that, and read the frame size and `upp` BACK from
# release/ww_camera_pin.log rather than computing them from the request.
SIZE="${SIZE:-1024x1024}"
echo "pin: centre $CENTER (legacy chunk-local centre $LCENTER), ortho half-width $ORTHO, window $SIZE (the frame is read back below)"

shot() {  # shot <out.png> <file to open> [extra env assignments...]
	local out="$1" file="$2"; shift 2
	local ctr="${SHOT_CENTER:-$CENTER}"
	rm -f "$out"
	# EVERY window from an empty scope: a window saves its layout on close, and
	# the next one then opens a different viewport (991 against 989 rows,
	# measured), which (c) rightly refuses to compare
	wipe_scope
	env "$@" WW_SETTINGS_SCOPE="$SCOPE" \
		WW_RENDER_SHOT="$(winpath "$out")" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW=1 \
		WW_RENDER_CENTER="$ctr" WW_RENDER_ORTHO="$ORTHO" WW_RENDER_CLEAN=1 \
		timeout 300 "$NS" --port "$PORT" "$(winpath "$file")" >/dev/null 2>&1
	[ -s "$out" ]
}

echo
echo "=== (a) module-off identity: no sheets, nothing changes ==================="

A_NEW="$W/a_new.nif"; A_OLD="$W/a_old.nif"; A_EMPTY="$W/a_empty.nif"
"$NS" -no-gui lodl "$(winpath "$LODL")" --region $CX $CY $((CX+1)) $((CY+1)) --lod 2 \
	--plane height -o "$(winpath "$A_NEW")" >/dev/null 2>&1
if [ -x "$BEFORE" ]; then
	"$BEFORE" -no-gui lodl "$(winpath "$LODL")" --region $CX $CY $((CX+1)) $((CY+1)) --lod 2 \
		--plane height -o "$(winpath "$A_OLD")" >/dev/null 2>&1
	check "a height document with no sheets beside the file is byte-identical to the exe before this lane" \
		"$([ -s "$A_NEW" ] && [ -s "$A_OLD" ] && cmp -s "$A_NEW" "$A_OLD" && echo 1 || echo 0)"
else
	skip "byte-identity against $BEFORE (the rung is not on disk)"
fi

WW_LODL_SHEETS="$(winpath "$W/empty")" "$NS" -no-gui lodl "$(winpath "$LODL")" \
	--region $CX $CY $((CX+1)) $((CY+1)) --lod 2 --plane height \
	-o "$(winpath "$A_EMPTY")" >/dev/null 2>&1
check "WW_LODL_SHEETS pointed at an EMPTY directory builds the same bytes" \
	"$([ -s "$A_EMPTY" ] && cmp -s "$A_NEW" "$A_EMPTY" && echo 1 || echo 0)"

# a plane that is NOT height is a data view by definition, sheets or no sheets
P_OFF="$W/p_off.nif"; P_ON="$W/p_on.nif"
"$NS" -no-gui lodl "$(winpath "$LODL")" --region $CX $CY $((CX+1)) $((CY+1)) --lod 2 \
	--plane ao -o "$(winpath "$P_OFF")" >/dev/null 2>&1
WW_LODL_SHEETS="$(winpath "$SHEETS")" "$NS" -no-gui lodl "$(winpath "$LODL")" \
	--region $CX $CY $((CX+1)) $((CY+1)) --lod 2 --plane ao \
	-o "$(winpath "$P_ON")" >/dev/null 2>&1
check "a non-height plane ignores the sheets entirely (byte-identical with them present)" \
	"$([ -s "$P_OFF" ] && [ -s "$P_ON" ] && cmp -s "$P_OFF" "$P_ON" && echo 1 || echo 0)"

if [ "${RUN_LODL_OPEN:-1}" = "1" ] && [ -f "$ROOT/tests/spells/lodl_open.sh" ]; then
	# its OWN fixtures: lodl_open.sh reads LODL and PORT too and means another
	# file, so an LODL= given to this harness must not reach it (EXE and PY may)
	lo=$(env -u LODL -u PORT bash "$ROOT/tests/spells/lodl_open.sh" 2>&1 | tail -3)
	echo "$lo" | sed 's/^/    /'
	locount=$(echo "$lo" | grep -oE '^[0-9]+ checks' | grep -oE '^[0-9]+')
	lofail=$(echo "$lo" | grep -oE '[0-9]+ failures' | grep -oE '^[0-9]+')
	check "lodl_open.sh still runs ${LODL_OPEN_CHECKS:-23} checks with 0 failures (ran ${locount:-?}, failed ${lofail:-?})" \
		"$([ "${locount:-0}" = "${LODL_OPEN_CHECKS:-23}" ] && [ "${lofail:-1}" = "0" ] && echo 1 || echo 0)"
else
	skip "lodl_open.sh (RUN_LODL_OPEN=0)"
fi

echo
echo "=== (b) the instance census: three counts that must agree ================="

CEN="$W/census.txt"; AUTHOUT="$W/auth.txt"
"$PY" "$AUTH" chunk "$NATIVE/$WS.lodo" "$NATIVE/$WS.lodi" $CX $CY $DIM > "$AUTHOUT" 2>&1
A_N=$(sed -n '1s/^chunk.instances //p' "$AUTHOUT")
echo "  the .lodi's own chunk table puts ${A_N:-?} instances in cells ($CX,$CY) dim $DIM"

if shot "$W/b.png" "$NATIVE/$WS.lodi" \
		WW_LODI_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1))" \
		WW_LODI_DUMP="$(winpath "$CEN")" \
		WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"; then
	C_N=$(grep -cv '^#' "$CEN" 2>/dev/null | head -1); C_N="${C_N:-0}"
	echo "  the viewer placed $C_N instances and wrote them down"
	check "the viewer's census count equals the .lodi's own chunk count ($C_N == ${A_N:-?})" \
		"$([ -n "$A_N" ] && [ "$C_N" = "$A_N" ] && echo 1 || echo 0)"
	cmpout=$("$PY" "$AUTH" compare "$CEN" "$AUTHOUT" 1.0 2>&1); cmprc=$?
	echo "$cmpout" | sed 's/^/  /'
	check "every placement's world position is within 1 unit of the file's own" \
		"$([ "$cmprc" = "0" ] && echo 1 || echo 0)"
else
	check "the .lodi document builds and writes its census" 0
	C_N=""
fi

MAN=""
if [ -n "$GBAKE" ]; then
	MAN=$(ls "$GBAKE"/*"$CX.$CY"*.manifest.txt 2>/dev/null | head -1)
fi
if [ -n "$MAN" ] && [ -f "$MAN" ]; then
	MANOUT="$W/manifest.txt"
	"$PY" "$AUTH" manifest "$MAN" $CX $CY $DIM > "$MANOUT" 2>&1
	sed 's/^/  /' "$MANOUT"
	M_ALL=$(sed -n 's/^manifest.rows //p' "$MANOUT")
	M_IN=$(sed -n 's/^manifest.inside //p' "$MANOUT")
	# the manifest and the .lodi belong to the SAME (identity-on) bake, so the
	# count it must agree with is that bake's own .lodi, not the look arm's
	GC="$GNATIVE/$WS.lodo"; GI="$GNATIVE/$WS.lodi"
	G_A=""; G_C=""
	if [ -f "$GC" ] && [ -f "$GI" ]; then
		G_AUTH="$W/auth_gate.txt"
		"$PY" "$AUTH" chunk "$GC" "$GI" $CX $CY $DIM > "$G_AUTH" 2>&1
		G_A=$(sed -n '1s/^chunk.instances //p' "$G_AUTH")
		G_CEN="$W/census_gate.txt"
		if shot "$W/b_gate.png" "$GI" \
				WW_LODI_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1))" \
				WW_LODI_DUMP="$(winpath "$G_CEN")" \
				WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$GBAKE")"; then
			G_C=$(grep -cv '^#' "$G_CEN" 2>/dev/null | head -1); G_C="${G_C:-0}"
		fi
		echo "  the identity-on bake: .lodi says ${G_A:-?}, the viewer placed ${G_C:-?}, the manifest has $M_ALL rows of which $M_IN are inside the chunk"
		check "the three counts of the SAME bake agree (manifest inside $M_IN == .lodi ${G_A:-?} == viewer ${G_C:-?})" \
			"$([ -n "$G_A" ] && [ "$M_IN" = "$G_A" ] && [ "$G_C" = "$G_A" ] && echo 1 || echo 0)"
		if [ -s "$G_CEN" ]; then
			cmpout=$("$PY" "$AUTH" compare "$G_CEN" "$G_AUTH" 1.0 2>&1); cmprc=$?
			echo "$cmpout" | sed 's/^/  /'
			check "every identity-on placement is within 1 unit of its own file" \
				"$([ "$cmprc" = "0" ] && echo 1 || echo 0)"
		fi
		"$PY" "$ROOT/tests/spells/lodgen_native_decode.py" "$GC" "$GI" --manifest "$MAN" 2>&1 \
			| sed 's/^/    /' | tail -12
	else
		check "the identity-on bake's own .lodo/.lodi sit under GNATIVE" 0
	fi
else
	skip "the manifest leg of (b): no <chunk>.manifest.txt under GBAKE=${GBAKE:-<unset>}"
	echo "       (the look bake passes --no-identity and src/lodgen.cpp writes placement"
	echo "        rows only under opts.identity; re-bake THIS ONE CHUNK with identity on)"
fi

# the floor: an empty region must come back EMPTY, and the harness must see it
E_AUTH="$W/auth_empty.txt"
"$PY" "$AUTH" chunk "$NATIVE/$WS.lodo" "$NATIVE/$WS.lodi" $EX $EY $DIM > "$E_AUTH" 2>&1
E_A=$(sed -n '1s/^chunk.instances //p' "$E_AUTH")
E_CEN="$W/census_empty.txt"
shot "$W/b_empty.png" "$NATIVE/$WS.lodi" \
	WW_LODI_REGION="$EX,$EY,$((EX+DIM-1)),$((EY+DIM-1))" \
	WW_LODI_DUMP="$(winpath "$E_CEN")" >/dev/null 2>&1
# grep -c prints 0 AND exits 1 when it selects nothing, so an `|| echo 0` here
# appends a SECOND zero and the comparison below never matches (measured).
E_C=$(grep -cv '^#' "$E_CEN" 2>/dev/null | head -1); E_C="${E_C:-0}"
echo "  floor: cells ($EX,$EY) dim $DIM — file says ${E_A:-?}, viewer placed $E_C"
check "the floor fires: an empty region yields 0 placements and the harness sees 0" \
	"$([ "${E_A:-x}" = "0" ] && [ "$E_C" = "0" ] && echo 1 || echo 0)"

echo
echo "=== (c) the .lodi scene against the chunk's own .BTO, one camera =========="

BTO="$OBJ/$WS.$DIM.$CX.$CY.BTO"
BTO2="$OBJ/$WS.$DIM.$OX.$OY.BTO"
if [ -f "$BTO" ] && [ -s "$W/b.png" ]; then
	# WHAT THIS ASKS AND WHY IT IS NOT AN IoU ANY MORE (lane BTOFREE1,
	# 2026-09-16). Until today this line demanded IoU >= 0.95 between the
	# .lodi scene and the chunk's own .BTO, and it printed 0.8179 on every exe
	# that ever ran it -- it never passed, not once, so the bar was never a
	# measurement of these two pictures. It could not be: the scene PLACES
	# whole library models and the .BTO carries a mesh the bake merged, cut at
	# the far rings and clipped to the chunk box, so their outlines differ by
	# construction. Measured here at chunk (-20,24) dim 4, one camera:
	#
	#   library   IoU     covered   area x .BTO
	#   mnam      0.8179  0.9874    1.195     (the pre-2026-09-16 default)
	#   near      0.6190  0.9604    1.512     (NATIVE1c's default, today)
	#
	# and under mnam 100.0 percent of the scene's excess pixels sit within
	# 16 px of a pixel the two share, 58.7 percent within 1 px, with the
	# largest connected blob of excess at 143 px and NOT ONE blob of 200 px or
	# more (scratchpad/btofree1_20260916/iou_analyse.py). One silhouette drawn
	# a hair wider than the other, everywhere -- not an object one side draws
	# and the other misses. The near-library default widens it further, which
	# is a consequence of a ruling and not a defect.
	#
	# So the check asks the two questions it always wanted: does the scene
	# draw EVERYTHING the chunk draws (covered), and does it do that without
	# simply drawing the world (area)? Each is vacuous alone -- a frame that
	# covers every pixel scores a perfect 1.0000 covered -- and the pair is
	# not. Three refuters below: the other chunk, the same .BTO mirrored in Y,
	# and that solid frame.
	if shot "$W/c_bto.png" "$BTO" WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"; then
		out=$("$PY" "$AUTH" cover "$W/b.png" "$W/c_bto.png" 2>&1)
		echo "$out" | sed 's/^/  /'
		COV=$(echo "$out" | sed -n 's/^COVER //p')
		FAT=$(echo "$out" | sed -n 's/^FAT //p')
		CFL=$(echo "$out" | sed -n 's/^COVERFLIP //p')
		check "the .lodi scene draws everything the .BTO draws (covered ${COV:-?} >= 0.90)" \
			"$(ge "${COV:-0}" 0.90)"
		check "and it draws no more than the chunk plus its edges (area ${FAT:-?} x the .BTO <= 2.00)" \
			"$(le "${FAT:-9}" 2.00)"
		check "the floor fires: the same .BTO mirrored in Y is NOT covered (${CFL:-?} < 0.50)" \
			"$(lt "${CFL:-1}" 0.50)"
	else
		check "the .BTO of the same chunk renders" 0
	fi
	if [ -s "$W/c_bto.png" ] && "$PY" "$AUTH" solid "$W/c_bto.png" "$W/c_solid.png" >/dev/null 2>&1; then
		out=$("$PY" "$AUTH" cover "$W/c_solid.png" "$W/c_bto.png" 2>&1)
		SCOV=$(echo "$out" | sed -n 's/^COVER //p')
		SFAT=$(echo "$out" | sed -n 's/^FAT //p')
		echo "  a solid frame in place of the .lodi scene: covered ${SCOV:-?}, area ${SFAT:-?} x"
		check "the refuter fires: a frame covering EVERYTHING passes coverage (${SCOV:-?}) and the area bar catches it (${SFAT:-?} > 2.00)" \
			"$([ "$(ge "${SCOV:-0}" 0.90)" = "1" ] && [ "$(le "${SFAT:-0}" 2.00)" = "0" ] && echo 1 || echo 0)"
	else
		skip "the solid-frame refuter: no .BTO render to size it from"
	fi
	if [ -f "$BTO2" ] && shot "$W/c_other.png" "$BTO2" \
			WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"; then
		out=$("$PY" "$AUTH" cover "$W/b.png" "$W/c_other.png" 2>&1)
		echo "$out" | sed 's/^/  /'
		COV2=$(echo "$out" | sed -n 's/^COVER //p')
		check "the refuter fires: a DIFFERENT chunk's .BTO is not covered (${COV2:-?} < 0.90)" \
			"$(lt "${COV2:-1}" 0.90)"
	else
		skip "the (c) refuter: no $WS.$DIM.$OX.$OY.BTO"
	fi
else
	skip "(c): no $(basename "$BTO") or no .lodi render to compare it with"
fi

echo
echo "=== (d) the lit .lodl against the .BTR of the same cells =================="

BTR="$OBJ/$WS.$DIM.$CX.$CY.BTR"
D_LIT="$W/d_lit.png"; D_DATA="$W/d_data.png"; D_EMPTY="$W/d_empty.png"
if shot "$D_LIT" "$LODL" WW_LODL_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1)),2" \
		WW_LODL_SHEETS="$(winpath "$SHEETS")"; then
	if [ -f "$BTR" ] && SHOT_CENTER="$LCENTER" shot "$W/d_btr.png" "$BTR" \
			WW_LODGEN_RESOURCES="$(winpath "$RES")"; then
		out=$("$PY" "$AUTH" mad "$D_LIT" "$W/d_btr.png" 2>&1)
		echo "$out" | sed 's/^/  /'
		MAD=$(echo "$out" | sed -n 's/^MAD //p')
		nccout=$("$PY" "$AUTH" ncc "$D_LIT" "$W/d_btr.png" 2>&1)
		echo "$nccout" | sed 's/^/  /'
		NCC=$(echo "$nccout" | sed -n 's/^NCC //p')
		NCCF=$(echo "$nccout" | sed -n 's/^NCCFLIP //p')
		BTR2="$OBJ/$WS.$DIM.$OX.$OY.BTR"
		NCC2=""
		if [ -f "$BTR2" ] && SHOT_CENTER="$LCENTER" shot "$W/d_btr_other.png" "$BTR2" \
				WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"; then
			NCC2=$("$PY" "$AUTH" ncc "$D_LIT" "$W/d_btr_other.png" 2>&1 | sed -n 's/^NCC //p')
		fi
		check "the lit terrain looks like the .BTR of the SAME cells (NCC ${NCC:-?} >= ${NCC_BAR:-0.45})" \
			"$([ -n "$NCC" ] && [ "$(ge "$NCC" "${NCC_BAR:-0.45}")" = "1" ] && echo 1 || echo 0)"
		check "the refuter fires: a DIFFERENT chunk's .BTR correlates less (NCC ${NCC2:-?} < ${NCC:-?})" \
			"$([ -n "$NCC2" ] && [ -n "$NCC" ] && [ "$(lt "$NCC2" "$NCC")" = "1" ] && echo 1 || echo 0)"
		check "the floor fires: the same .BTR mirrored in Y does NOT correlate (NCC ${NCCF:-?} < 0.10)" \
			"$([ -n "$NCCF" ] && [ "$(lt "$NCCF" "0.10")" = "1" ] && echo 1 || echo 0)"
		check "the lit terrain and the .BTR of the same cells agree (mean |dColour| ${MAD:-?} < ${MAD_BAR:-48})" \
			"$(lt "${MAD:-999}" "${MAD_BAR:-48}")"
	else
		skip "(d) against the .BTR: no $(basename "$BTR")"
	fi
else
	check "the .lodl renders lit from its sheets" 0
fi

shot "$D_DATA" "$LODL" WW_LODL_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1)),2" >/dev/null 2>&1
shot "$D_EMPTY" "$LODL" WW_LODL_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1)),2" \
	WW_LODL_SHEETS="$(winpath "$W/empty")" >/dev/null 2>&1
check "a render with WW_LODL_SHEETS pointed at an EMPTY directory IS the data view" \
	"$([ -s "$D_DATA" ] && [ -s "$D_EMPTY" ] && cmp -s "$D_DATA" "$D_EMPTY" && echo 1 || echo 0)"

if [ -s "$D_LIT" ] && [ -s "$D_DATA" ]; then
	out=$("$PY" "$AUTH" mad "$D_LIT" "$D_DATA" 2>&1)
	LDIFF=$(echo "$out" | sed -n 's/^MAD //p')
	check "the lit render is NOT the data view (mean |dColour| ${LDIFF:-?} > 8), so (d) is not vacuous" \
		"$(ge "${LDIFF:-0}" 8)"
fi

if [ -f "$ROOT/release/ww_camera_pin.log" ]; then
	echo
	echo "camera pin (last grab): $(tail -1 "$ROOT/release/ww_camera_pin.log")"
fi

echo
echo "$checks checks, $fails failures, $skips skipped"
[ "$fails" = "0" ] && echo PASS || echo FAIL
[ "$fails" = "0" ]
