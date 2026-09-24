#!/bin/bash
# ---------------------------------------------------------------------------
# impostor_trunk.sh -- the card's TRUNK gate, the slider's two ends, and the
# BC7 depth sheet.
#
# Lane IMPOSTORDEPTH1, 2026-09-23. bungo on the 8x8 maple card: the trunk
# doubles, thins, vanishes and jumps as the camera goes round. The bar below
# was written in scratchpad/impostordepth1_20260923/progress.md BEFORE any card
# number was read, and it is never lowered.
#
# Lane IMPOSTORDEPTH2, 2026-09-23, on bungo's rulings:
#   * the card's `_n` depth sheet is BC7 (DX10, DXGI 98);
#   * the coverage is ALWAYS decoded per texel, then filtered -- the
#     WW_IMPOSTOR_COVFILTER switch is retired and does nothing;
#   * the slider's CRISP end (0, the default) is FLAT SNAP (ruled 2026-09-23
#     13:1x): the nearest frame alone, no blend, NOT moved by its depth --
#     byte-identical to WW_IMPOSTOR_BLEND=0. It may JUMP at a frame change
#     (ruled acceptable), so its T3 and its pop are REPORTED, not gated; its
#     trunk width (T1) and trunk count (T2) are gated; its tear share is
#     gated and is a NAMED KNOWN RED (bungo saw it fail and ruled the flat
#     snap anyway, 2026-09-23 13:1x) -- it counts as a failure, like
#     native_lighting.sh's two legacy_btr reds, and the bar is not lowered.
#     The depth-MOVED snap (WW_IMPOSTOR_SNAP=1) is a harness override only;
#   * the slider's SMOOTH end (1) is stipple + depth search 16: the whole
#     trunk bar and the whole tear bar.
#   WW_IMPOSTOR_SEARCH=N and WW_IMPOSTOR_SLIDER=x stay harness overrides.
#
# THE TRUNK BAR, per elevation, over a 1-degree orbit (az 0..359):
#   band A = the rows 8..20% of the MESH's height above its lowest row;
#   band B = 8..35% (reaches the fork).
#   T1  card/mesh pixel ratio in band A inside 0.85..1.15 at EVERY azimuth;
#   T2  trunk runs per row (gaps <= 2 px filled, runs >= 3 px), median over
#       band B: card <= mesh at EVERY azimuth (no doubled trunk);
#   T3  band-A centre x per 1-degree step: the card's worst <= 2 x the mesh's
#       worst (no jumping trunk); a view with no card pixel in band A fails.
# THE TEAR BAR beside it: worst chunk share (|open(mesh & ~card, 7x7)| /
#   |mesh|) <= 2 x the reference drawer's worst at the same elevation, and
#   worst per-step pop <= 1.5 x its worst pop. The references are PINNED to
#   the drawer IMPOSTORDEPTH1 measured them on (exe e294ae80, DXT5 sheets,
#   stipple, no search, the hardware coverage filter), so the bar cannot move
#   with the drawer it judges:
#     el 0: pop 0.11702900, tear 0.00961476;  el 20: pop 0.13232642, tear 0.01388609.
#
# THE SHEET BAR (tests/spells/impostor_sheetbar.py): the set's `_n` is DX10
#   BC7 and its decoded height, against the height lodgen was given to encode
#   (the bake PNG after lodgenRepairOctHeight), reads mean < 1.0 level,
#   p95 <= 3, and >= 90% of the heights survive. Red control: the DXT5 sheet
#   of the same bake (DXT5_CARDS) fails it.
#
# Instrument: tests/spells/impostor_trunkbar.py. Card = WW_IMPOSTOR_CHANNEL=2
# (the coverage the cut read), mesh = WW_IMPOSTOR_MESH_CHANNEL=8.
#
# FLOORS, each in the same run:
#   * the mesh scored against itself PASSES the bar (the arithmetic can pass);
#   * the old drawer (stipple WITHOUT the search: WW_IMPOSTOR_SLIDER=1
#     WW_IMPOSTOR_SEARCH=0) FAILS the trunk bar at el 0 and el 20;
#   * the knob rows: the default draws the crisp end and says so; SLIDER=0
#     and BLEND=0 are byte-identical to it (the flat snap); SNAP=1 (the
#     depth-moved snap) is not; SLIDER=1 is not, and is byte-identical to
#     SLIDER=1 SEARCH=16; SEARCH=0 at the smooth end changes it; SLIDER=0.5
#     is a third picture; COVFILTER=0 and =1 are byte-identical to the default.
#
# MEASURED 2026-09-23 (lane IMPOSTORDEPTH2), BC7 sheets of the 8x8 maple:
#   exe 6b8ed793 (sha1; the flat snap): 38 checks, 3 failures. Sheet, knob and floor
#   rows all ok.
#     crisp end (flat snap) el 0: T1 0.882..1.011, 0 of 360 outside; T2 ok;
#       tear 4.68% = KNOWN RED; jump 15.25 px (reported);
#     crisp end (flat snap) el 20: T1 0.874..1.091, 0 outside; T2 ok;
#       tear 10.14% = KNOWN RED; jump 64.36 px (reported);
#     smooth end el 0: TRUNK + TEAR PASS (T1 0.901..1.135, T3 1.46x, tear 0.54%);
#     smooth end el 20: T1 ok, T3 1.12x, tear ok, T2 FAILS on 2 views (az 161,
#       2 runs vs 1) -- OPEN; the miss DEPTH1 saw on uncompressed sheets too.
#   exe f7403e44 (the depth-MOVED snap at 0): 38 checks, 5 failures -- crisp
#   el 0 T1 14 of 360 outside, jump 3.38 px; el 20 T1 203 outside, T2 6,
#   tear 6.19%, jump 20.35 px. Why it thins (scratchpad/impostordepth2_20260923/
#   diag2.out): moving ONE frame by its height uncovers what that frame never
#   photographed, and no second frame fills it. The sheet is not the cause: an
#   uncompressed sheet thins it more (T1 309 outside at el 20).
#   The rung (DEPTH1's e294ae80): 38 checks, 20 failures, every trunk row red.
#
# Usage:   bash tests/spells/impostor_trunk.sh
#          IMPOSTOR_LODM=<dir>/cards/<id>_oct.lodm MESH=<nif> bash tests/spells/impostor_trunk.sh
#          EXE=release/NifSkope.before_impostordepth2.exe bash tests/spells/impostor_trunk.sh
#   (the rung has no slider: its knob rows fail and its trunk rows fail)
# ---------------------------------------------------------------------------
set -u
. "$(dirname "$0")/_harness.sh"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
case "$NS" in /*) ;; *) NS="$ROOT/$NS" ;; esac
LODM="${IMPOSTOR_LODM:-$ROOT/scratchpad/impostordepth2_20260923/n8_2k_bc7/cards/000531b3_oct.lodm}"
DXT5_CARDS="${DXT5_CARDS:-$ROOT/scratchpad/impostor16_20260923/n8/bakes/n8_2k/cards}"
MESH="${MESH:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleInstitute06Green.nif}"
W="$ROOT/release/impostor_trunk_tmp"
LOG="$ROOT/release/ww_impostor_trunk.log"
rm -rf "$W"; mkdir -p "$W"; : > "$LOG"
checks=0; fails=0
say() { echo "$*" | tee -a "$LOG"; }
ok()  { checks=$((checks+1)); say "  ok    $*"; }
bad() { checks=$((checks+1)); fails=$((fails+1)); say "  FAIL  $*"; }

say "impostor_trunk gate $(date '+%Y-%m-%d %H:%M:%S')"
say "exe $NS  $(ls -l --time-style=+%Y-%m-%d_%H:%M:%S "$NS" | awk '{print $5, $6}')  sha1 $(sha1sum "$NS" | cut -c1-8)"
say "set $LODM"

PY=""
for c in /c/Users/bungo/AppData/Local/Programs/Python/Python39/python python python3; do
	if command -v "$c" >/dev/null 2>&1 && "$c" -c "import numpy, PIL, scipy" >/dev/null 2>&1; then PY="$c"; break; fi
done
[ -n "$PY" ] || { say "FAIL: no python with numpy, PIL and scipy"; exit 1; }
tasklist | grep -qi "Fallout4" && { say "REFUSED: Fallout4.exe is up"; exit 1; }
n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
[ "$n" = "0" ] || { say "REFUSED: $n harness NifSkope(s) with --port already running (one at a time)"; exit 1; }
[ -f "$LODM" ] || { say "REFUSED: no card set at $LODM (set IMPOSTOR_LODM)"; exit 1; }
[ -f "$MESH" ] || { say "REFUSED: no mesh at $MESH (set MESH)"; exit 1; }

say "SHEET: the depth sheet"
SB="$("$PY" "$ROOT/tests/spells/impostor_sheetbar.py" "$(dirname "$LODM")" 2>&1 | tail -1)"; say "    $SB"
echo "$SB" | grep -q "^SHEET PASS" && ok "the set's _n is BC7 and holds the height (mean < 1 level, p95 <= 3, >= 90% of heights survive)" \
	|| bad "the set's _n fails the sheet bar"
if [ -d "$DXT5_CARDS" ]; then
	SR="$("$PY" "$ROOT/tests/spells/impostor_sheetbar.py" "$DXT5_CARDS" 2>&1 | tail -1)"; say "    red control: $SR"
	echo "$SR" | grep -q "^SHEET FAIL" && ok "FLOOR: the DXT5 sheet of the same bake FAILS the sheet bar, so the bar can fail" \
		|| bad "FLOOR: the DXT5 sheet passes the sheet bar -- the bar cannot fail"
else
	bad "FLOOR: no DXT5 red control at $DXT5_CARDS (set DXT5_CARDS)"
fi

PORT=46110
orbit() { # orbit <name> <views> [ENV...]  -> $W/<name>/v_az..._{card,mesh}.png + $W/<name>.log
	local d="$W/$1" v="$2" t0; shift 2
	mkdir -p "$d"; t0=$(date +%s)
	for try in 1 2 3; do
		PORT=$((PORT+1))
		env "$@" WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$(winpath "$LODM")" \
			WW_IMPOSTOR_LOG="$(winpath "$d.log")" WW_IMPOSTOR_SHOT="$(winpath "$d/v")" \
			WW_IMPOSTOR_ORBIT_VIEWS="$v" WW_RENDER_CLEAN=1 WW_RENDER_SIZE=1000x1000 \
			WW_IMPOSTOR_CHANNEL=2 WW_IMPOSTOR_MESH_CHANNEL=8 \
			timeout 3000 "$NS" "$MESH" --port "$PORT" >/dev/null 2>&1
		grep -q "orbit counted 0 of" "$d.log" 2>/dev/null || break
	done
	say "  orbit $(basename "$d"): $(ls "$d"/*_card.png 2>/dev/null | wc -l) cards, $(( $(date +%s) - t0 ))s"
}
diffpx() { "$PY" -c "
import sys, glob, os, numpy as np
from PIL import Image
a, b = sys.argv[1], sys.argv[2]; t = 0; n = 0
for f in sorted(glob.glob(os.path.join(a, '*_card.png'))):
    g = os.path.join(b, os.path.basename(f))
    if not os.path.exists(g): print(-1); sys.exit()
    n += 1
    t += int((np.asarray(Image.open(f).convert('RGB')) != np.asarray(Image.open(g).convert('RGB'))).any(-1).sum())
print(t if n else -1)" "$1" "$2"; }
same() { # same <a> <b> <what>
	local D; D="$(diffpx "$W/$1" "$W/$2")"
	[ "$D" = "0" ] && ok "$3 (byte-identical on 6 views)" || bad "$3 -- differs by ${D:-?} px (-1 = views missing)"
}
differs() { # differs <a> <b> <min px> <what>
	local D; D="$(diffpx "$W/$1" "$W/$2")"
	[ "${D:--1}" -gt "$3" ] 2>/dev/null && ok "$4 ($D px)" || bad "$4 -- only ${D:-none} px"
}
logsays() { # logsays <run> <text> <what>
	grep -qF -- "$2" "$W/$1.log" 2>/dev/null && ok "$3" || bad "$3 -- '$2' not in $1.log"
}

say "KNOBS: 6 views OFF the frame grid"
# Off-grid on purpose: on a frame's own direction the three weights are 1/0/0 and
# sharpening them (slider between the ends) cannot move a pixel -- measured 1 px
# on the old on-grid set (0/45/90/180/270/315 at el 0), which proved nothing.
SIX="20:10,65:14,110:10,160:6,250:12,330:8"
orbit k_default "$SIX"
orbit k_snap "$SIX" WW_IMPOSTOR_SNAP=1
orbit k_slider0 "$SIX" WW_IMPOSTOR_SLIDER=0
orbit k_smooth "$SIX" WW_IMPOSTOR_SLIDER=1
orbit k_smooth16 "$SIX" WW_IMPOSTOR_SLIDER=1 WW_IMPOSTOR_SEARCH=16
orbit k_smooth0 "$SIX" WW_IMPOSTOR_SLIDER=1 WW_IMPOSTOR_SEARCH=0
orbit k_mid "$SIX" WW_IMPOSTOR_SLIDER=0.5
orbit k_covf0 "$SIX" WW_IMPOSTOR_COVFILTER=0
orbit k_covf1 "$SIX" WW_IMPOSTOR_COVFILTER=1
orbit k_blend0 "$SIX" WW_IMPOSTOR_BLEND=0
logsays k_default "slider: 0.00 -- the CRISP end, flat snap (the default)" "the default draw is the slider's crisp end and says so"
logsays k_default "frames: ONE, the nearest, flat on the card plane (the slider's crisp end)" "the default draws ONE frame, the nearest, flat"
logsays k_default "depth search: none -- the frame is drawn flat, not moved by its depth" "the crisp end is not moved by its depth"
logsays k_default "coverage filter: DECODED per texel, then bilinear -- always" "the log says the coverage filter is always decoded"
logsays k_smooth "slider: 1.00 -- the SMOOTH end, stipple + search (WW_IMPOSTOR_SLIDER)" "WW_IMPOSTOR_SLIDER=1 is named as the smooth end"
logsays k_smooth "frames: THREE, height-blended" "the smooth end blends three frames"
logsays k_smooth "depth search: 16 steps + 1 refinement (the slider's)" "the smooth end searches 16 steps of its own accord"
logsays k_smooth "cut rule: stipple" "the smooth end cuts with the stipple"
logsays k_mid "between the ends, weights sharpened to the power 2.000" "WW_IMPOSTOR_SLIDER=0.5 sharpens the weights (power 1/0.5)"
logsays k_smooth0 "depth search: off -- the one-step parallax (WW_IMPOSTOR_SEARCH)" "WW_IMPOSTOR_SEARCH=0 is named as the override"
logsays k_snap "frames: ONE, the nearest, moved by its depth (WW_IMPOSTOR_SNAP=1, a harness override)" "WW_IMPOSTOR_SNAP=1 is named in the log as the depth-moved harness override"
logsays k_blend0 "frames: ONE, flat on the card plane (WW_IMPOSTOR_BLEND=0)" "WW_IMPOSTOR_BLEND=0 is named in the log"
logsays k_covf1 "(WW_IMPOSTOR_COVFILTER retired)" "WW_IMPOSTOR_COVFILTER is reported as retired"
same k_default k_blend0 "the crisp end IS the flat snap (== WW_IMPOSTOR_BLEND=0)"
same k_default k_slider0 "WW_IMPOSTOR_SLIDER=0 IS the default"
same k_smooth k_smooth16 "the smooth end IS search 16 (SLIDER=1 == SLIDER=1 SEARCH=16)"
same k_default k_covf0 "WW_IMPOSTOR_COVFILTER=0 cannot turn the decoded filter off (retired)"
same k_default k_covf1 "WW_IMPOSTOR_COVFILTER=1 changes nothing (retired)"
differs k_default k_smooth 1000 "FLOOR: the smooth end is not the crisp end"
differs k_smooth k_smooth0 1000 "FLOOR: the search reaches the shader (the smooth end with SEARCH=0 differs)"
differs k_mid k_smooth 100 "FLOOR: slider 0.5 is not the smooth end"
differs k_mid k_default 1000 "FLOOR: slider 0.5 is not the crisp end"
differs k_default k_snap 1000 "FLOOR: the depth-moved snap (WW_IMPOSTOR_SNAP=1) is not the crisp end"

say "TRUNK: 1-degree orbits"
AZ0=$("$PY" -c "print(','.join('%d:0' % a for a in range(360)))")
AZ20=$("$PY" -c "print(','.join('%d:20' % a for a in range(360)))")
orbit crisp_el0 "$AZ0"
orbit crisp_el20 "$AZ20"
orbit smooth_el0 "$AZ0" WW_IMPOSTOR_SLIDER=1
orbit smooth_el20 "$AZ20" WW_IMPOSTOR_SLIDER=1
orbit old_el0 "$AZ0" WW_IMPOSTOR_SLIDER=1 WW_IMPOSTOR_SEARCH=0
orbit old_el20 "$AZ20" WW_IMPOSTOR_SLIDER=1 WW_IMPOSTOR_SEARCH=0
bar() { "$PY" "$ROOT/tests/spells/impostor_trunkbar.py" "$@" 2>&1; }
refpop() { [ "$1" = "0" ] && echo 0.11702900429474859 || echo 0.13232642082319157; }
reftear() { [ "$1" = "0" ] && echo 0.009614756035622873 || echo 0.013886092553613373; }
FL="$(MESHASCARD=1 bar mesh=$W/crisp_el0@0 | tail -1)"
echo "$FL" | grep -q "TRUNK PASS" && ok "FLOOR: the mesh scored against itself passes the trunk bar" \
	|| bad "FLOOR: the mesh scored against itself does not pass the trunk bar ($FL) -- the bar cannot pass"
for el in 0 20; do
	B="$(REF_POP=$(refpop $el) REF_TEAR=$(reftear $el) bar old=$W/old_el$el@$el)"; say "$(echo "$B" | sed 's/^/    /')"
	echo "$B" | tail -1 | grep -q "TRUNK FAIL" \
		&& ok "FLOOR: the old drawer (stipple, no search) FAILS the trunk bar at el $el, so the bar can fail" \
		|| bad "FLOOR: the old drawer PASSES the trunk bar at el $el -- the bar cannot fail, or the drawer changed"
done
for el in 0 20; do
	# the crisp end (flat snap): T1, T2 and the tear share gated; T3 and the pop reported (bungo: snap may jump).
	# The tear row is a NAMED KNOWN RED by bungo's ruling 2026-09-23 13:1x: still counted, bar not lowered.
	B="$(REF_POP=$(refpop $el) REF_TEAR=$(reftear $el) bar crisp=$W/crisp_el$el@$el)"; say "$(echo "$B" | sed 's/^/    /')"
	echo "$B" | grep "T1 ratio" | grep -q -- "-> ok" && ok "crisp end (flat snap) el $el: T1 trunk width inside 0.85..1.15 at every azimuth" \
		|| bad "crisp end (flat snap) el $el: T1 trunk width fails"
	echo "$B" | grep "T2 runs" | grep -q -- "-> ok" && ok "crisp end (flat snap) el $el: T2 never more trunks than the mesh" \
		|| bad "crisp end (flat snap) el $el: T2 doubled trunk"
	T="$(echo "$B" | grep -oE "tear [0-9.]+% <= [0-9.]+%" | tail -1)"
	"$PY" -c "import sys; a, b = sys.argv[1].replace('%', '').replace('tear ', '').split(' <= '); sys.exit(0 if float(a) <= float(b) else 1)" "$T" 2>/dev/null \
		&& ok "crisp end (flat snap) el $el: the tear share is within the bar ($T)" \
		|| bad "KNOWN RED (bungo's ruling 2026-09-23 13:1x: flat snap despite its tear) crisp end (flat snap) el $el: the tear share is over the bar (${T:-no number})"
	say "    REPORTED, not gated: $(echo "$B" | grep -oE "T3 centre step: card worst [0-9.]+ px \(az [0-9]+\)") | $(echo "$B" | grep -oE "mesh worst [0-9.]+ mean") | $(echo "$B" | grep -oE "ratio [0-9.]+ \(bar 2\)") | $(echo "$B" | grep -oE "pop worst [0-9.]+")"
	B="$(REF_POP=$(refpop $el) REF_TEAR=$(reftear $el) bar smooth=$W/smooth_el$el@$el)"; say "$(echo "$B" | sed 's/^/    /')"
	L="$(echo "$B" | tail -1)"
	echo "$L" | grep -q "TRUNK PASS" && ok "smooth end el $el passes the trunk bar" || bad "smooth end el $el fails the trunk bar (see the T1/T2/T3 lines above)"
	echo "$L" | grep -q "TEAR BAR PASS" && ok "smooth end el $el passes the tear bar" || bad "smooth end el $el fails the tear bar"
done

say "$checks checks, $fails failures"
[ "$fails" = "0" ] && { say "PASS"; exit 0; } || { say "FAIL"; exit 1; }
