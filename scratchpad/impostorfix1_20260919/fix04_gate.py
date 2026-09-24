#!/usr/bin/env python
"""IMPOSTORFIX1 fix04: raise the gate's IoU floor on measurement, and add the
two rows that fail on exe 88d6abb3 -- the sheet's own height (no application at
all) and the known-answer photograph row."""
import io

P = 'tests/spells/impostor_draw.sh'
b = io.open(P, 'rb').read()
cr = b.count(b'\r')
assert cr == 0, cr

OLD = b'IOU_FLOOR="${IMPOSTOR_IOU_FLOOR:-0.22}"\n'
NEW = b'''#    RAISED AGAIN TO 0.35, 2026-09-19, ON THE SAME LAW: two more defects were
#    repaired and a floor that still passes the build carrying them is not a
#    floor. The three numbers are all this row, all blast_n4, all 1024x1024:
#
#      exe 88d6abb3 + the sheets it shipped with            0.2778
#      this exe     + the same sheets (the ray repair only) 0.3047
#      this exe     + the sheets this lane re-baked         0.4469
#
#    0.35 sits above BOTH pre-repair numbers and a seventh below the repaired
#    one, so the row fails on the old application, fails on a new application
#    handed old sheets, and passes only when both halves are right. The two
#    defects:
#      * the `_n` sheet's height outside and at the edge of the silhouette was
#        the frame's flood average, not a depth -- `lodgenDilateFrames` floods
#        everything the silhouette does not cover with the FRAME'S AVERAGE, and
#        the parallax step then reads that as a displacement: +264 world units
#        on average and +743 at p95 against a card half-width of 135. See
#        `lodgenRepairOctHeight` in src/lodgen.cpp, and step 14 below, which
#        measures it with no application running at all;
#      * `cardOrtho` was written false unconditionally, so an ORTHOGRAPHIC
#        camera got the perspective ray fan and the parallax stopped being a
#        no-op at the very directions the frames were baked from
#        (res/shaders/impostor_oct.vert). Step 15 below is that one.
#    A floor still only ever moves UP, and only after a repair, on measurement.
# ---------------------------------------------------------------------------
IOU_FLOOR="${IMPOSTOR_IOU_FLOOR:-0.35}"
'''
assert b.count(OLD) == 1, b.count(OLD)
b = b.replace(OLD, NEW)

TAIL = b'''
say "done  $steps steps, $fails failures"
'''
ROW = b'''# ---------------------------------------------------------------------------
# 14. THE SHEET'S OWN HEIGHT, WITH NO APPLICATION AT ALL.
#
#    Every row above needs a window, a driver and a scene, and each of those
#    can fail for its own reasons. This one reads the two BC3 sheets the
#    `.lodm` names and asks the question the drawer's parallax step asks: does
#    a texel's height lie inside the depth band this frame's own fully covered
#    texels occupy, and do the texels the object does NOT cover sit at the card
#    plane, where the parallax is a no-op?
#
#    The band comes from the frame's own whole texels, so the row cannot be
#    satisfied by editing a constant. It fails on exe 88d6abb3's sheets -- 16
#    frames of 16, worst excursion 1530 world units against a card half-width
#    of 135 -- and passes on the repaired ones, 0 of 16.
# ---------------------------------------------------------------------------
if "$PY" "$here/impostor_sheet_check.py" "$IMPOSTOR_LODM" > "$work/sheet_check.txt" 2>&1; then
	ok "14 height channel: $( tail -1 "$work/sheet_check.txt" )"
else
	sed -n '1,6p' "$work/sheet_check.txt" | while IFS= read -r l; do say "      $l"; done
	bad "14 height channel: $( tail -1 "$work/sheet_check.txt" )"
fi

# ---------------------------------------------------------------------------
# 15. THE PHOTOGRAPH ROW -- the drawer's one KNOWN ANSWER.
#
#    Every IoU row above compares two sparse silhouettes and therefore has a
#    floor somebody had to justify. This row does not: a frame of the card IS
#    an orthographic photograph of the mesh along one direction, so viewed
#    again from that direction the card must reproduce it. The neighbours carry
#    barycentric weight zero, and under an orthographic camera the parallax
#    step moves the sample along a ray antiparallel to the frame's own forward,
#    which `frameUvOf` projects away -- so the height channel cannot change the
#    picture there AT ALL, and the score has no excuse to be low.
#
#    The directions come from the `.lodm`'s own grid size by the bake's
#    hemi-octahedral map (impostor_bake_views.py), never from a stored list.
#
#    Measured, blast_n4, repaired sheets, 16 bake directions:
#      parallax OFF                       0.8823   (range 0.8563 .. 0.9107)
#      parallax ON,  ortho ray (this exe) 0.8823   -- equal, as the algebra says
#      parallax ON,  the perspective fan  0.7545   -- exe 88d6abb3's behaviour
#    and the same 16 directions with the SHIPPED sheets and the fan: 0.6507.
#
#    So the two clauses fail for two different reasons on the old exe. The
#    count clause fails because 88d6abb3 does not read WW_IMPOSTOR_ORBIT_VIEWS
#    and silently orbits its own ring instead -- an exe that ignores the
#    variable must fail LOUDLY rather than score some other set of views. The
#    equality clause fails because of the ray. Note what this row does NOT
#    catch: at a bake direction a correct ray makes the parallax a no-op, so a
#    BROKEN HEIGHT SHEET scores here exactly as well as a repaired one. That
#    defect is step 14's, and no single row covers both.
# ---------------------------------------------------------------------------
bakeviews=$( "$PY" "$here/impostor_bake_views.py" "$IMPOSTOR_LODM" 2>/dev/null )
nbake=$( printf '%s' "$bakeviews" | tr ',' '\\n' | grep -c ':' )
photo_iou() {   # $1 = WW_IMPOSTOR_BLEND, $2 = log name; sets $pn (counted) and $pi (mean)
	: > "$work/$2"
	WW_IMPOSTOR_PREVIEW=orbit \\
	WW_IMPOSTOR_LODM="$IMPOSTOR_LODM" \\
	WW_IMPOSTOR_LOG="$work/$2" \\
	WW_IMPOSTOR_ORBIT_VIEWS="$bakeviews" \\
	WW_IMPOSTOR_BLEND="$1" \\
	WW_WINDOW_AT=1960,40 \\
	WW_RENDER_SIZE="${IMPOSTOR_SIZE:-1024x1024}" \\
	"$exe" --port "$port" ${IMPOSTOR_NIF:+"$IMPOSTOR_NIF"} >> "$log" 2>&1
	pn=$( sed -n 's/^orbit counted \\([0-9]*\\) of.*/\\1/p' "$work/$2" | tail -1 )
	pi=$( sed -n 's/^orbit iou mean //p' "$work/$2" | tail -1 )
}
PHOTO_FLOOR="${IMPOSTOR_PHOTO_FLOOR:-0.80}"
PHOTO_GAP="${IMPOSTOR_PHOTO_GAP:-0.01}"
if [ -z "$IMPOSTOR_NIF" ]; then
	bad "15 no mesh: the photograph row scores the card against the mesh it was baked from -- REFUSED without it"
elif [ -z "$bakeviews" ] || [ "${nbake:-0}" -lt 4 ]; then
	bad "15 could not derive the bake directions from $IMPOSTOR_LODM"
else
	photo_iou 0 "ww_impostor_photo_off.log"; nOff="$pn"; iOff="$pi"
	photo_iou 1 "ww_impostor_photo_on.log";  nOn="$pn";  iOn="$pi"
	if [ -z "$iOff" ] || [ -z "$iOn" ]; then
		bad "15 the harness wrote no 'orbit iou mean' line (parallax off '$iOff', on '$iOn')"
	elif [ "${nOff:-0}" -ne "$nbake" ] || [ "${nOn:-0}" -ne "$nbake" ]; then
		bad "15 asked for $nbake bake directions, the exe counted $nOff / $nOn -- WW_IMPOSTOR_ORBIT_VIEWS was not honoured"
	elif ! "$PY" -c "import sys; sys.exit(0 if float('$iOff') >= float('$PHOTO_FLOOR') else 1)"; then
		bad "15 a single un-blended frame at its own bake direction scored $iOff < $PHOTO_FLOOR -- the card is not a photograph of the mesh"
	elif ! "$PY" -c "import sys; sys.exit(0 if abs(float('$iOn')-float('$iOff')) <= float('$PHOTO_GAP') else 1)"; then
		bad "15 parallax changed the picture at a bake direction: on $iOn vs off $iOff (> $PHOTO_GAP) -- the view ray is not antiparallel to the frame, i.e. an orthographic camera is being given the perspective fan"
	else
		ok "15 photograph: $nbake bake directions, parallax off $iOff, on $iOn (no-op, as the algebra requires)"
	fi
fi

'''
assert b.count(TAIL) == 1, b.count(TAIL)
b = b.replace(TAIL, b'\n' + ROW + TAIL[1:])

assert b.count(b'\r') == cr
io.open(P, 'wb').write(b)
print('fix04 applied, %d bytes, CR=%d' % (len(b), b.count(b'\r')))
