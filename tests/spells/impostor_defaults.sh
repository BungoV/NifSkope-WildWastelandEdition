#!/bin/bash
# impostor_defaults.sh -- bungo's R5 card defaults, measured on the running code (lane CARDFIX1, 2026-09-24).
#
# bungo, 2026-09-24 21:1x, RULED: "crisp cards -- N8 grid, the crisp cut, the transition slider at the
# crisp end (defaults)". And 2026-09-23: "they're LOD objects ... it's fine if they're choppy".
#
# ROWS
#   D1 the bake driver's grid default is 8 (tools/bake_impostor_cards.sh `OCT`)
#   D2 the LOD panel's card-frames default is 8 (src/lodgenmanager.cpp `cardFrames`)
#   D3 the drawer, with NO override, is at the slider's crisp end: flat snap, ONE frame
#      (read from the preview harness's own log, which prints ImpostorDraw::resolve's answer)
#   D4 the drawer's default cut is the STRONGEST frame BY NAME ("the crisp end's own cut (R5 ...)").
#      RED ON THE RUNG: an exe from before CARDFIX1 prints "cut rule: stipple" at the default.
#   D5 NO PIXEL MOVED: the default render is byte-identical to WW_IMPOSTOR_CUT=strong and to
#      WW_IMPOSTOR_CUT=mean at the same views (one frame at weight 1: every rule cuts the same)
#   D6 FLOOR for D5: the smooth end (WW_IMPOSTOR_SLIDER=1) differs from the default at those views,
#      so "byte-identical" is not a comparison that cannot fail
#
# USAGE  IMPOSTOR_LODM=<card _oct.lodm> [IMPOSTOR_EXE=<exe>] bash tests/spells/impostor_defaults.sh
#        default card: IMPOSTORDEPTH2's N8 blast (main tree scratchpad, read only)
set -u
here=$( cd "$( dirname "$0" )" && pwd ); root=$( cd "$here/../.." && pwd )
exe="${IMPOSTOR_EXE:-$root/release/NifSkope.exe}"
port="${IMPOSTOR_PORT:-27731}"
LODM="${IMPOSTOR_LODM:-E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth2_20260923/n8_2k_bc7/cards/000531b3_oct.lodm}"
PY="${PY:-/c/Users/bungo/AppData/Local/Programs/Python/Python39/python}"
. "$here/_harness.sh"
tmp="$root/release/impostor_defaults_tmp"; rm -rf "$tmp"; mkdir -p "$tmp"
checks=0; fails=0
ok()  { checks=$((checks+1)); echo "  ok    $*"; }
bad() { checks=$((checks+1)); fails=$((fails+1)); echo "  FAIL  $*"; }
tasklist 2>/dev/null | grep -qi fallout4 && { echo "REFUSED: Fallout4.exe is up"; exit 2; }
echo "exe: $exe ($(stat -c '%y %s' "$exe" 2>/dev/null))"
[ -f "$LODM" ] || { echo "no card .lodm at $LODM"; exit 2; }

# D1 / D2: the two places the grid default lives
d1=$( sed -n 's/^OCT="\${OCT:-\([0-9]*\)}".*/\1/p' "$root/tools/bake_impostor_cards.sh" | head -1 )
[ "$d1" = 8 ] && ok "D1 the bake driver's grid default is 8" || bad "D1 the bake driver's grid default is 8 (read '${d1:-nothing}')"
d2=$( grep -oE 'cardFrames"[^0-9]*[0-9]+' "$root/src/lodgenmanager.cpp" | grep -oE '[0-9]+$' | sort -u | tr '\n' ' ' )
[ "$d2" = "8 " ] && ok "D2 the LOD panel's card-frames default is 8" || bad "D2 the LOD panel's card-frames default is 8 (read '${d2:-nothing}')"

views="0:0,23:0,45:5,77:10,135:15,200:20,290:3,333:12"
grab() {   # $1 out dir, rest env
	d="$1"; shift; rm -rf "$d"; mkdir -p "$d"
	env "$@" WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$LODM" \
		WW_IMPOSTOR_LOG="$( cygpath -m "$d.log" )" WW_IMPOSTOR_SHOT="$( cygpath -m "$d" )/v" \
		WW_IMPOSTOR_ORBIT_VIEWS="$views" WW_RENDER_CLEAN=1 WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=768x827 \
		timeout 300 "$exe" --port "$port" > /dev/null 2>&1
	echo "  grab $(basename "$d"): $(ls "$d"/*.png 2>/dev/null | wc -l) pictures"
}
grab "$tmp/default"
grab "$tmp/strong" WW_IMPOSTOR_CUT=strong
grab "$tmp/mean"   WW_IMPOSTOR_CUT=mean
grab "$tmp/smooth" WW_IMPOSTOR_SLIDER=1
# RUNG=<older exe>: its default render, the same views -- "no pixel moved" against the exe BEFORE the change
[ -n "${RUNG:-}" ] && { exe_keep="$exe"; exe="$RUNG"; grab "$tmp/rung"; exe="$exe_keep"; }

L="$tmp/default.log"
grep -q "^slider: 0.00 -- the CRISP end, flat snap (the default)" "$L" \
	&& grep -q "^frames: ONE, the nearest, flat on the card plane" "$L" \
	&& ok "D3 the default draw is the slider's crisp end, one frame, flat" \
	|| bad "D3 the default draw is the slider's crisp end, one frame, flat (log: $(grep -E '^(slider|frames):' "$L" | tr '\n' '|'))"
grep -q "^cut rule: strong -- the STRONGEST frame alone: the crisp end's own cut" "$L" \
	&& ok "D4 the default cut is the strongest frame, by name" \
	|| bad "D4 the default cut is the strongest frame, by name (log: $(grep '^cut rule' "$L" | cut -c1-60))"

r=$( "$PY" - "$( cygpath -m "$tmp" )" <<'PYEOF'
import sys, os, hashlib
t = sys.argv[1]
def pics(d):
    d = os.path.join(t, d)
    return {f: hashlib.sha1(open(os.path.join(d, f), 'rb').read()).hexdigest()
            for f in sorted(os.listdir(d)) if f.endswith('.png')} if os.path.isdir(d) else {}
D, S, M, W, R = pics('default'), pics('strong'), pics('mean'), pics('smooth'), pics('rung')
print(len(D), sum(1 for f in D if S.get(f) == D[f]), sum(1 for f in D if M.get(f) == D[f]),
      sum(1 for f in D if f in W and W[f] != D[f]), len(R), sum(1 for f in D if R.get(f) == D[f]))
PYEOF
)
set -- $r; n=${1:-0}; ss=${2:-0}; sm=${3:-0}; dw=${4:-0}; nr=${5:-0}; sr=${6:-0}
[ "$n" -ge 8 ] && [ "$ss" = "$n" ] && [ "$sm" = "$n" ] \
	&& ok "D5 no pixel moved: the default equals the strongest-frame and mean cuts at all $n views" \
	|| bad "D5 no pixel moved: default vs strong $ss/$n, vs mean $sm/$n identical"
[ "$dw" -ge 1 ] \
	&& ok "D6 floor: the smooth end differs from the default at $dw of $n views" \
	|| bad "D6 floor: the smooth end differs from the default at $dw of $n views -- the identity above proves nothing"
if [ -n "${RUNG:-}" ]; then
	[ "$nr" -ge 8 ] && [ "$sr" = "$n" ] 		&& ok "D7 the default picture is byte-identical to the rung's default at all $n views" 		|| bad "D7 the default picture vs the rung's default: $sr of $n identical (rung pictures $nr)"
fi
echo "$checks checks, $fails failures"
[ "$fails" = 0 ] && echo PASS || echo FAIL
