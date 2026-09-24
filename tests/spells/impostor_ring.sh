#!/bin/sh
# IMPOSTORRING1 (lane CARDFIX1 step 5, 2026-09-24) -- THE HORIZON RING's gate.
#
# bungo 2026-09-23 04:4x, RULED: "for fo4cs use the convention was 22.5 degrees per take". Tree cards are
# 16 azimuths at elevation 0, uniform, in ONE row (the aggregate's ring layout, docs/LODGEN_LODM_FORMAT.md
# 3a) instead of the N x N hemi-octahedral grid. The .lodm declares the layout (`views` 16, `grid` [16,1],
# no `oct`); the NifSkope drawer reads it and blends the TWO neighbouring azimuths by angle.
#
# THE BRIEF'S GATE, verbatim: "a ring card set has 16 frames at azimuth k x 22.5 deg (+-0.5 deg, measured
# from the bake's own camera echo); a reader that assumes the N x N grid refuses it by name". Plus the
# IMPOSTORRING1 rows: ring azimuths uniform at 22.5 (fails on the rung), max popping step, IoU at the
# in-between angle 11.25. Every row has a red that is RUN here, not asserted.
#
#   R1  the bake's camera echo: 16 `ringview v azim elev` lines, azim within 0.5 deg of v x 22.5 and elev
#       within 0.5 deg of 0. The echo is read back from GLView::viewTransform() at the moment the frame is
#       drawn, not the angle asked for. RED: the rung exe, same bake command, writes no ring at all.
#   R2  the card .lodm carries the ring: views 16, grid [16,1], NO oct, frameOffset 2 x 16 numbers, and
#       the albedo sheet is 16 frames wide and one frame high. RED: the rung's lodgen, given the SAME ring
#       bake, writes a .lodm without `views` (it never heard of the `ring` sidecar line).
#   R3  the reader: this exe loads the ring .lodm and says RING; the rung's reader REFUSES it and names
#       `oct` (the N x N reader finds no grid). FLOOR: the rung loads this exe's N8 .lodm, so its refusal
#       is about the ring, not about the harness.
#   R4  IoU of card vs mesh at the 16 in-between azimuths (11.25 + 22.5k, el 0), the default draw, against
#       THE CEILING: the mesh's own picture at the frame's azimuth (22.5k) scored against the mesh at the
#       view -- what a perfect photograph from the nearest frame would score. Bar: card mean >=
#       RING_CEIL_FRAC (0.90) x ceiling mean, both from this run's own pictures.
#       HISTORY: run 1 (2026-09-24 23:1x, exe eaa4b0b6, gates/impostor_ring.run1.out) pre-registered an
#       ABSOLUTE 0.60 and failed at 0.4886 -- with the ceiling at 0.5015 (min 0.3435): no single frame
#       reaches 0.60 on this thin subject. The bar was set without measuring the subject; re-pinned, and
#       the failure is on file. RED: the same run with the frame choice turned a quarter round
#       (WW_IMPOSTOR_SHUFFLE=1) must fall below the same bar. R4a (sanity): at the bake directions (22.5k, el 0) mean >= 0.80, row 15's
#       photograph floor in impostor_draw.sh.
#   R5  popping: the worst silhouette change between two views 1 deg apart over a full turn at el 0, the
#       default draw (impostor_tear_pop.py pop). Bar: ring16 worst <= 1.5 x the N8 grid's worst, same exe,
#       same model, same tile -- row 18's own factor. RED: a ring of 8 (45 deg steps) must exceed it.
#   R6  the blend rule: at 5.625 + 22.5k (a quarter step past frame k) the selection is frames k and k+1
#       at weights 0.75 / 0.25 (+-0.01), and the third slot carries weight 0: TWO frames, weighted by
#       angle. Computed here from the logged camera azimuth, not from the drawer's arithmetic.
#   M   MEASUREMENT, not a gate: ring16 vs N8 IoU at the in-between azimuths at el 0/5/15/30/60 (the ring
#       has no frame above the horizon; this is what that costs, printed).
#
# Subject: TreeMapleblasted05 (000531b3), the asymmetric tree impostor_draw.sh uses (a symmetric subject
# cannot tell a right azimuth from a wrong one). TILE 256 for every set. Everything lands in RING_WORK
# (untracked: PNG/DDS never enter git).
set -u
here=$( cd "$( dirname "$0" )" && pwd )
root=$( cd "$here/../.." && pwd )
EXE="${EXE:-$root/release/NifSkope.exe}"
RUNG="${RUNG:-$root/release/NifSkope.before_cardfix1.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
MESH="${RING_MESH:-$DATA/meshes/Landscape/Trees/TreeMapleblasted05.nif}"
FID="${RING_FORMID:-000531b3}"
# every path the Windows exe sees is E:/ form (a Git-Bash parent converts nothing)
WORK="$( cygpath -m "${RING_WORK:-$root/scratchpad/cardfix1_20260924/ring}" )"
TILE="${RING_TILE:-256}"
CEIL_FRAC="${RING_CEIL_FRAC:-0.90}"
PORT="${RING_PORT:-27741}"
PY="${PY:-python}"
mkdir -p "$WORK"
log="$WORK/impostor_ring.log"; : > "$log"
steps=0; fails=0
say() { echo "$*" | tee -a "$log"; }
ok()  { steps=$(( steps + 1 )); say "  ok    $*"; }
bad() { steps=$(( steps + 1 )); fails=$(( fails + 1 )); say "  FAIL  $*"; }
say "exe  $EXE ($( sha1sum "$EXE" | cut -c1-8 ))"
say "rung $RUNG ($( sha1sum "$RUNG" | cut -c1-8 ))"
b="$( basename "$MESH" .nif | tr 'A-Z' 'a-z' )"

bake() {   # $1 tag  $2 exe  rest = env (WW_IMPOSTOR_RING=.. / WW_IMPOSTOR_OCT=..)
	tag="$1"; bx="$2"; shift 2
	PORT=$(( PORT + 1 ))
	rm -rf "$WORK/$tag"; mkdir -p "$WORK/$tag/bake" "$WORK/$tag/cards"
	env "$@" WW_IMPOSTOR_BAKE="$WORK/$tag/bake" WW_IMPOSTOR_TILE="$TILE" WW_WINDOW_AT=1960,40 \
		timeout 900 "$bx" "$MESH" --port "$PORT" > "$WORK/$tag/bake.stdout" 2>&1
	for s in albedo normal gsaos rmaos g e; do
		[ -f "$WORK/$tag/bake/${b}_oct_$s.png" ] && cp "$WORK/$tag/bake/${b}_oct_$s.png" "$WORK/$tag/cards/${FID}_oct_$s.png"
	done
	[ -f "$WORK/$tag/bake/${b}_front.png" ] && cp "$WORK/$tag/bake/${b}_front.png" "$WORK/$tag/cards/${FID}_front.png"
	[ -f "$WORK/$tag/bake/${b}_side.png" ] && cp "$WORK/$tag/bake/${b}_side.png" "$WORK/$tag/cards/${FID}_side.png"
	[ -f "$WORK/$tag/bake/${b}.txt" ] && cp "$WORK/$tag/bake/${b}.txt" "$WORK/$tag/cards/${FID}.txt"
	say "bake $tag: $( grep -cE '^(ring|oct) ' "$WORK/$tag/cards/${FID}.txt" 2>/dev/null ) layout line(s), $( grep -c '^ringview ' "$WORK/$tag/cards/${FID}.txt" 2>/dev/null ) ringview line(s)"
}
compress() {   # $1 tag  $2 exe  -> $WORK/$1/cards/<fid>_oct.lodm + textures/data/fo4cslod/cards
	O="$WORK/$1"
	"$2" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao \
		--impostors "$O/cards" --data-root "$DATA" -o "$O/chunk.bto" > "$O/lodgen.log" 2>&1
	lrc=$?
	T="$O/textures/data/fo4cslod/cards"; mkdir -p "$T"; rm -f "$T"/*.dds
	for f in "$O/cards/"*.DDS; do [ -e "$f" ] || continue; cp "$f" "$T/$( basename "$f" | tr 'A-Z' 'a-z' )"; done
	say "compress $1: lodgen rc $lrc; lodm $( [ -f "$O/cards/${FID}_oct.lodm" ] && echo yes || echo NO )"
}
orbit() {   # $1 tag (set)  $2 run name  $3 views  rest = env
	st="$1"; rn="$2"; vv="$3"; shift 3
	PORT=$(( PORT + 1 ))
	d="$WORK/shots/$st.$rn"; rm -rf "$d"; mkdir -p "$d"
	env "$@" WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$WORK/$st/cards/${FID}_oct.lodm" \
		WW_IMPOSTOR_LOG="$d.log" WW_IMPOSTOR_SHOT="$d/v" WW_IMPOSTOR_ORBIT_VIEWS="$vv" \
		WW_IMPOSTOR_CHANNEL=2 WW_IMPOSTOR_MESH_CHANNEL=8 WW_RENDER_CLEAN=1 \
		WW_RENDER_SIZE=1024x1024 WW_WINDOW_AT=1960,40 \
		timeout 1800 "$EXE" --port "$PORT" "$MESH" > "$d.stdout" 2>&1
}
views() {   # $1 start  $2 step  $3 count  $4 elevs (space list)
	"$PY" -c "
import sys
s,st,n=float(sys.argv[1]),float(sys.argv[2]),int(sys.argv[3])
print(','.join('%g:%g'%((s+st*k)%360,float(e)) for e in sys.argv[4].split() for k in range(n)))" "$1" "$2" "$3" "$4"
}

# ---------------------------------------------------------------- the bakes
bake ring16 "$EXE"  WW_IMPOSTOR_RING=16
bake rungring "$RUNG" WW_IMPOSTOR_RING=16
bake n8 "$EXE" WW_IMPOSTOR_OCT=8
bake ring8 "$EXE" WW_IMPOSTOR_RING=8
compress ring16 "$EXE"
compress n8 "$EXE"
compress ring8 "$EXE"
# R2's red: the rung's lodgen on the SAME ring bake, in its own folder
rm -rf "$WORK/runglodgen"; mkdir -p "$WORK/runglodgen"; cp -r "$WORK/ring16/cards" "$WORK/runglodgen/"
rm -f "$WORK/runglodgen/cards/"*.lodm "$WORK/runglodgen/cards/"*.DDS
compress runglodgen "$RUNG"

# ---------------------------------------------------------------- R1
r1() {   # $1 sidecar  -> prints a verdict line
	"$PY" - "$1" <<'PYEOF'
import sys, re
try:
    t = open(sys.argv[1]).read()
except OSError:
    print('BAD no sidecar'); sys.exit(0)
lay = re.findall(r'^ring (\d+) ', t, re.M)
ev = [(int(v), float(a), float(e)) for v, a, e in re.findall(r'^ringview (\d+) (\S+) (\S+)', t, re.M)]
if not lay:
    print('BAD no `ring` layout line (%d ringview lines)' % len(ev)); sys.exit(0)
if int(lay[0]) != 16 or len(ev) != 16:
    print('BAD ring %s with %d echoes' % (lay[0], len(ev))); sys.exit(0)
worstA = max(abs(((a - 22.5 * v) + 180.0) % 360.0 - 180.0) for v, a, e in ev)
worstE = max(abs(e) for v, a, e in ev)
steps = sorted(a for v, a, e in ev)
gaps = [((steps[(k + 1) % 16] - steps[k]) % 360.0) for k in range(16)]
print('%s 16 echoes, worst azimuth error %.4f deg, worst elevation %.4f deg, steps %.3f..%.3f deg'
      % ('OK' if worstA <= 0.5 and worstE <= 0.5 else 'BAD', worstA, worstE, min(gaps), max(gaps)))
PYEOF
}
v=$( r1 "$WORK/ring16/cards/${FID}.txt" )
case "$v" in OK*) ok "R1 the ring's camera echo is 16 azimuths at k x 22.5 deg, elevation 0: ${v#OK }" ;;
	*) bad "R1 the ring's camera echo: ${v#BAD }" ;; esac
v=$( r1 "$WORK/rungring/cards/${FID}.txt" )
case "$v" in OK*) bad "R1 red control did not bite: the rung wrote a ring: $v" ;;
	*) ok "R1 red control: the rung's bake of the same command has no ring: ${v#BAD }" ;; esac

# ---------------------------------------------------------------- R2
r2() {   # $1 lodm  $2 albedo png  $3 sidecar
	"$PY" - "$1" "$2" "$3" <<'PYEOF'
import sys, json, re
from PIL import Image
try:
    raw = open(sys.argv[1], 'rb').read()
except OSError:
    print('BAD no .lodm'); sys.exit(0)
c = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
m = re.search(r'^ring \d+ (\d+) (\d+) ', open(sys.argv[3]).read(), re.M)
tw, th = (int(m.group(1)), int(m.group(2))) if m else (0, 0)
W, H = Image.open(sys.argv[2]).size
fo = c.get('frameOffset', [])
why = []
if c.get('views') != 16: why.append('views %r' % c.get('views'))
if c.get('grid') != [16, 1]: why.append('grid %r' % c.get('grid'))
if 'oct' in c: why.append('oct %r present' % c.get('oct'))
if len(fo) != 32: why.append('frameOffset %d numbers' % len(fo))
if (W, H) != (16 * tw, th): why.append('albedo %dx%d vs 16 x %dx%d' % (W, H, tw, th))
print(('OK' if not why else 'BAD') + ' views %r grid %r oct %r frameOffset %d, albedo %dx%d (frame %dx%d)%s'
      % (c.get('views'), c.get('grid'), c.get('oct'), len(fo), W, H, tw, th, ('; ' + ', '.join(why)) if why else ''))
PYEOF
}
w16="$( cygpath -m "$WORK/ring16/cards" )"
v=$( r2 "$w16/${FID}_oct.lodm" "$w16/${FID}_oct_albedo.png" "$w16/${FID}.txt" )
case "$v" in OK*) ok "R2 the card .lodm declares the ring: ${v#OK }" ;; *) bad "R2 the card .lodm: ${v#BAD }" ;; esac
wr="$( cygpath -m "$WORK/runglodgen/cards" )"
v=$( r2 "$wr/${FID}_oct.lodm" "$wr/${FID}_oct_albedo.png" "$wr/${FID}.txt" )
case "$v" in OK*) bad "R2 red control did not bite: the rung's lodgen wrote a ring .lodm: $v" ;;
	*) ok "R2 red control: the rung's lodgen on the same bake writes no ring: ${v#BAD }" ;; esac

# ---------------------------------------------------------------- R3
load() {   # $1 exe  $2 lodm  $3 log
	PORT=$(( PORT + 1 ))
	: > "$3"
	WW_IMPOSTOR_PREVIEW=map WW_IMPOSTOR_LODM="$2" WW_IMPOSTOR_LOG="$3" WW_WINDOW_AT=1960,40 \
		timeout 300 "$1" --port "$PORT" "$MESH" > /dev/null 2>&1
}
load "$EXE"  "$w16/${FID}_oct.lodm" "$WORK/r3_new.log"
load "$RUNG" "$w16/${FID}_oct.lodm" "$WORK/r3_rung.log"
load "$RUNG" "$( cygpath -m "$WORK/n8/cards" )/${FID}_oct.lodm" "$WORK/r3_rung_n8.log"
if grep -q "grid: RING of 16 views" "$WORK/r3_new.log"; then ok "R3 this exe loads the ring set: $( grep -m1 'grid: RING' "$WORK/r3_new.log" )"
else bad "R3 this exe did not load the ring set: $( grep -m1 -iE 'refus|error|oct is' "$WORK/r3_new.log" )"; fi
if grep -q "oct is 0, outside" "$WORK/r3_rung.log"; then ok "R3 the rung's N x N reader refuses the ring set by name: $( grep -m1 -o 'oct is 0, outside.*' "$WORK/r3_rung.log" | cut -c1-80 )"
else bad "R3 the rung's reader did not refuse the ring set by name (log: $( head -c 200 "$WORK/r3_rung.log" | tr '\n' ' ' ))"; fi
if grep -q "grid: 8x8" "$WORK/r3_rung_n8.log"; then ok "R3 floor: the rung loads this exe's N8 set, so its refusal is the ring's"
else bad "R3 floor: the rung did not load the N8 set either -- the refusal above proves nothing"; fi

# ---------------------------------------------------------------- R4, R6, M
IN="$( views 11.25 22.5 16 '0 5 15 30 60' )"
AT="$( views 0 22.5 16 0 )"
Q="$( views 5.625 22.5 16 0 )"
orbit ring16 iou "$IN,$AT,$Q" WW_IMPOSTOR_ORBIT_SELECT=1
orbit n8 iou "$IN"
orbit ring16 shuffled "$( views 11.25 22.5 16 0 )" WW_IMPOSTOR_SHUFFLE=1
meaniou() {   # $1 log  $2 azimuth start  $3 elevation -> mean over the 16 azimuths start+22.5k (EXCLUDED dropped)
	"$PY" - "$1" "$2" "$3" <<'PYEOF'
import sys, re
t = open(sys.argv[1]).read().split('\n')
s, el = float(sys.argv[2]), float(sys.argv[3])
want = [(s + 22.5 * k) % 360.0 for k in range(16)]
near = lambda a: any(abs((a - w + 180.0) % 360.0 - 180.0) < 0.06 for w in want)
got, drop = [], 0
for i, l in enumerate(t):
    m = re.match(r'orbit azim (\S+) elev (\S+) .* iou (\S+)', l)
    if not m or abs(float(m.group(2)) - el) > 0.05 or not near(float(m.group(1))):
        continue
    if i + 1 < len(t) and 'EXCLUDED: mesh' in t[i + 1]:
        drop += 1; continue
    got.append(float(m.group(3)))
print('%.4f %d %d' % (sum(got) / len(got) if got else -1.0, len(got), drop))
PYEOF
}
set -- $( meaniou "$WORK/shots/ring16.iou.log" 11.25 0 ); iIn=$1; nIn=$2
set -- $( meaniou "$WORK/shots/ring16.iou.log" 0 0 );     iAt=$1; nAt=$2
set -- $( meaniou "$WORK/shots/ring16.shuffled.log" 11.25 0 ); iSh=$1; nSh=$2
ge() { "$PY" -c "import sys; sys.exit(0 if float('$1') >= float('$2') else 1)"; }
ceil=$( "$PY" - "$WORK/shots/ring16.iou" <<'PYEOF'
import sys, numpy as np
from PIL import Image
def m(p):
    a = np.asarray(Image.open(p).convert('RGB')); return (a != a[0, 0]).any(-1)
d, r = sys.argv[1], []
for k in range(16):
    A, B = m('%s/v_az%03d_el00_mesh.png' % (d, int(22.5 * k))), m('%s/v_az%03d_el00_mesh.png' % (d, int(22.5 * k + 11.25)))
    r.append((A & B).sum() / max(1, (A | B).sum()))
print('%.4f' % np.mean(r))
PYEOF
)
IOU_BAR=$( "$PY" -c "print('%.4f' % (float('${ceil:-9}') * float('$CEIL_FRAC')))" )
say "R4 ceiling: the mesh at 22.5k vs the mesh at 22.5k + 11.25, el 0: mean $ceil; bar = $CEIL_FRAC x = $IOU_BAR"
if [ "$nIn" -lt 12 ]; then bad "R4 only $nIn of 16 in-between views counted"
elif ge "$iIn" "$IOU_BAR"; then ok "R4 IoU at the 16 in-between azimuths (11.25 + 22.5k, el 0): mean $iIn >= $IOU_BAR ($CEIL_FRAC x the mesh ceiling $ceil) over $nIn views"
else bad "R4 IoU at the in-between azimuths: mean $iIn < $IOU_BAR ($CEIL_FRAC x the mesh ceiling $ceil) over $nIn views"; fi
if [ "$nSh" -ge 12 ] && ! ge "$iSh" "$IOU_BAR"; then ok "R4 red control: a quarter-turned frame choice scores $iSh < $IOU_BAR at the same views"
else bad "R4 red control did not bite: shuffled $iSh over $nSh views"; fi
if [ "$nAt" -ge 12 ] && ge "$iAt" 0.80; then ok "R4a at the ring's own bake directions (22.5k, el 0): mean $iAt >= 0.80 over $nAt views"
else bad "R4a at the bake directions: mean $iAt over $nAt views (floor 0.80)"; fi
v=$( "$PY" - "$WORK/shots/ring16.iou.log" <<'PYEOF'
import sys, re, math
t = open(sys.argv[1]).read().split('\n')
checked, bad = 0, []
cur = None
for l in t:
    m = re.match(r'orbit azim (\S+) elev (\S+) ', l)
    if m:
        cur = {'az': float(m.group(1)), 'el': float(m.group(2)), 'fr': {}}
        continue
    if cur is None:
        continue
    m = re.match(r'\s+select ring of (\d+) views, camera azimuth (\S+) deg', l)
    if m:
        cur['cam'] = float(m.group(2)) % 360.0
    m = re.match(r'\s+select frame (\d) index (\d+) .* weight (\S+)', l)
    if m:
        cur['fr'][int(m.group(1))] = (int(m.group(2)), float(m.group(3)))
        if len(cur['fr']) == 3 and 'cam' in cur and abs(cur['el']) < 0.05 and abs((cur['cam'] / 22.5) % 1.0 - 0.25) < 0.02:
            f = cur['cam'] / 22.5
            k = int(math.floor(f)) % 16
            t_ = f - math.floor(f)
            exp = {k: 1.0 - t_, (k + 1) % 16: t_}
            fr = cur['fr']
            gotw = {fr[0][0]: fr[0][1]}
            gotw[fr[1][0]] = gotw.get(fr[1][0], 0.0) + fr[1][1]
            checked += 1
            ok = (abs(fr[2][1]) < 1e-6 and set(gotw) == set(exp)
                  and all(abs(gotw[i] - exp[i]) <= 0.01 for i in exp) and abs(t_ - 0.25) <= 0.01)
            if not ok:
                bad.append('az %.3f cam %.3f got %s want %s' % (cur['az'], cur['cam'], fr, exp))
print(('OK' if checked == 16 and not bad else 'BAD') + ' %d quarter-step views checked, %d wrong%s'
      % (checked, len(bad), ('; ' + bad[0]) if bad else ''))
PYEOF
)
case "$v" in OK*) ok "R6 the ring blends TWO neighbours by angle (0.75 / 0.25 a quarter step past a frame, slot 3 weight 0): ${v#OK }" ;;
	*) bad "R6 the blend rule: ${v#BAD }" ;; esac
say "M  ring16 vs N8, IoU at the 16 in-between azimuths (mean, counted):"
for el in 0 5 15 30 60; do
	set -- $( meaniou "$WORK/shots/ring16.iou.log" 11.25 $el ); r=$1; rn=$2
	set -- $( meaniou "$WORK/shots/n8.iou.log" 11.25 $el ); g=$1; gn=$2
	say "M  el $el: ring16 $r ($rn)  N8 $g ($gn)"
done

# ---------------------------------------------------------------- R5
SW="$( views 0 1 360 0 )"
for st in ring16 n8 ring8; do orbit "$st" pop "$SW"; done
"$PY" "$here/impostor_tear_pop.py" pop "$( "$PY" -c "print(','.join('%d:0'%a for a in range(360)))" )" \
	"$( cygpath -m "$WORK/shots/ring16.pop" )" "$( cygpath -m "$WORK/shots/n8.pop" )" \
	"$( cygpath -m "$WORK/shots/ring8.pop" )" > "$WORK/pop.txt" 2>&1
tee -a "$log" < "$WORK/pop.txt" > /dev/null
v=$( "$PY" - "$WORK/pop.txt" <<'PYEOF'
import sys, re
w = {k: (int(c), int(m)) for k, c, m in re.findall(r'^pop (\S+)\s+card worst (\d+) .*mesh worst (\d+)', open(sys.argv[1]).read(), re.M)}
need = ('ring16.pop', 'n8.pop', 'ring8.pop')
if not all(k in w for k in need):
    print('MISSING %s' % sorted(w)); sys.exit(0)
r, g, e = (w[k][0] for k in need)
print('%s %s ring16 worst %d, N8 worst %d (ratio %.2f, bar 1.50), ring8 worst %d (ratio %.2f), mesh worst %d'
      % ('OK' if r <= 1.5 * g else 'BAD', 'BITES' if e > 1.5 * g else 'DEAD', r, g, r / max(1, g), e, e / max(1, g), w['n8.pop'][1]))
PYEOF
)
case "$v" in
	"OK "*) ok "R5 popping at el 0, 1 deg steps: ${v#OK * }" ;;
	"BAD "*) bad "R5 popping at el 0, 1 deg steps: ${v#BAD * }" ;;
	*) bad "R5 popping: $v" ;;
esac
case "$v" in *" BITES "*) ok "R5 red control: a ring of 8 pops past the bar" ;;
	*) bad "R5 red control did not bite: $v" ;; esac

say "M  full turn at el 0, 1-degree steps (the fair comparison: 11.25 sits 1.75 deg from an N8 frame):"
for st in ring16 n8 ring8; do
	say "M  $st: $( "$PY" - "$WORK/shots/$st.pop.log" <<'PYEOF'
import re, sys
t = open(sys.argv[1]).read().split('\n'); v = []
for i, l in enumerate(t):
    m = re.match(r'orbit azim \S+ elev \S+ .* iou (\S+)', l)
    if m and not (i + 1 < len(t) and 'EXCLUDED: mesh' in t[i + 1]):
        v.append(float(m.group(1)))
print('mean IoU %.4f over %d views, min %.4f' % (sum(v) / max(1, len(v)), len(v), min(v) if v else -1))
PYEOF
)"
done
say "M  ring16 ceiling over the full turn (mesh vs mesh at the nearest 22.5k): $( "$PY" - "$WORK/shots/ring16.pop" <<'PYEOF'
import sys, numpy as np
from PIL import Image
def m(p):
    a = np.asarray(Image.open(p).convert('RGB')); return (a != a[0, 0]).any(-1)
d = sys.argv[1]; M = {a: m('%s/v_az%03d_el00_mesh.png' % (d, a)) for a in range(360)}
r = [((M[a] & M[int(22.5 * (round(a / 22.5) % 16))]).sum()) / max(1, (M[a] | M[int(22.5 * (round(a / 22.5) % 16))]).sum()) for a in range(360)]
print('mean %.4f' % np.mean(r))
PYEOF
)"
say "$steps checks, $fails failures"
[ "$fails" -eq 0 ] && say "PASS" || say "FAIL"
[ "$fails" -eq 0 ]
