"""IMPOSTORFIX3 fix 05 -- `tests/spells/impostor_draw.sh`.

Three edits, all anchored exact-once:

  1. IOU_FLOOR 0.35 -> 0.50. UP ONLY, and on measurement: four subjects were
     re-measured at 1024x1024 on this exe's sheets and the lowest is 0.5610.
     0.50 sits above the number this exe scored on af457755's own sheets
     (0.4469) and a ninth below the lowest repaired one.

  2. NEW ROW 14a -- the BC3 alpha decoder's own known answer. The ruler that
     row 14 reads was one ramp step short; a gate whose instrument is wrong
     has no business passing. Red control: the file as it stood this morning,
     10 of 16 ramp entries wrong.

  3. NEW ROW 14b -- THE ROW THAT FAILS ON exe af457755 FOR REPAIR 1. It asks
     whether the height fill REACHED outside the silhouette at all: over the
     frames of the set, the mean fraction of the 8-ring band whose height is
     more than 12 levels off the card plane. af457755's law was "card plane
     everywhere outside", so that fraction is the BC3 noise floor. Measured:

         subject    af457755's sheets   this bake
         blast_n4         2.0%            19.8%
         blast_n8         2.4%            25.8%
         maple_n4         3.4%            27.7%
         dead_n4          4.3%            28.5%
         rock_n4          5.7%            72.9%

     The floor is 10 per cent: twice the worst old number and half the best
     new one. It is a MEAN over frames and not a per-frame test on purpose --
     the measurement above shows frames on BOTH sheet sets at 0.0%, because a
     frame whose local surface happens to sit at the card plane has nothing to
     carry outward, and a per-frame test would convict the repair for
     geometry.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_draw.sh'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')


def once(a):
    if s.count(a) != 1:
        print('ANCHOR COUNT %d (want 1):\n%s' % (s.count(a), a[:140])); sys.exit(2)


A1 = """#    A floor still only ever moves UP, and only after a repair, on measurement.
# ---------------------------------------------------------------------------
IOU_FLOOR="${IMPOSTOR_IOU_FLOOR:-0.35}"
"""
B1 = """#    A floor still only ever moves UP, and only after a repair, on measurement.
# ---------------------------------------------------------------------------
#    RAISED AGAIN TO 0.50, 2026-09-19 (IMPOSTORFIX3), on the same law and for
#    one more repair: the `_n` height outside the silhouette is no longer
#    slammed to the card plane, it carries the object's own depth for eight
#    rings first (`kOutRings` in `lodgenRepairOctHeight`). Four subjects
#    re-measured by THIS row's own instrument, 1024x1024, this exe:
#
#      blast_n4  0.4469 on af457755's sheets  ->  0.5610 on the 8-ring bake
#      blast_n8                               ->  0.7280
#      dead_n4                                ->  0.6143
#      rock_n4                                ->  0.8536
#
#    0.50 is above 0.4469 by a ninth and below the lowest repaired number by a
#    ninth, so the row fails a new exe handed old sheets and passes only when
#    the bake is right too.
#
#    WHAT THIS FLOOR DOES NOT ADMIT, said rather than hidden: the BARE FOREST
#    MAPLE (TreeMapleForest2) scores 0.3674 over 24 views with everything
#    repaired, and scored 0.3545 before. It was already within 0.017 of the
#    0.35 floor and it is not a drawing defect -- it is 297 whole texels in
#    32,768, the photography-resolution defect IMPOSTORFIX1 named. A floor is
#    not bent around it.
# ---------------------------------------------------------------------------
IOU_FLOOR="${IMPOSTOR_IOU_FLOOR:-0.50}"
"""
once(A1); s = s.replace(A1, B1)

A2 = """	bad "14 height channel: $( tail -1 "$work/sheet_check.txt" )"
fi

# ---------------------------------------------------------------------------
# 15. THE PHOTOGRAPH ROW -- the drawer's one KNOWN ANSWER.
"""
B2 = '''	bad "14 height channel: $( tail -1 "$work/sheet_check.txt" )"
fi

# ---------------------------------------------------------------------------
# 14a. THE RULER'S OWN KNOWN ANSWER.
#
#     Row 14 decodes two BC3 sheets in Python and convicts the bake with the
#     result. Until 2026-09-19 that decoder built the eight-level alpha ramp
#     one step short -- `((7-k)*a0 + k*a1)/7` with k running 0..5 into slots
#     2..7 instead of 1..6 -- so every interpolated alpha came out LOW, worst
#     error 36.4 of 255, and the six-level ramp had (4-k) where D3D says
#     (5-k) and no slot 5 at all.
#
#     `impostor_bc_decode.py` run as a program hand-builds one 16-byte block
#     per mode with every index present and checks all 16 entries against the
#     D3D rule. Red control, the decoder as it stood that morning: 10 of 16
#     wrong. A gate whose instrument is wrong has no business passing rows.
# ---------------------------------------------------------------------------
if "$PY" "$here/impostor_bc_decode.py" > "$work/bc_known_answer.txt" 2>&1; then
	ok "14a BC3 alpha decoder: $( tail -1 "$work/bc_known_answer.txt" )"
else
	sed -n '1,20p' "$work/bc_known_answer.txt" | while IFS= read -r l; do say "      $l"; done
	bad "14a BC3 alpha decoder: $( tail -1 "$work/bc_known_answer.txt" )"
fi

# ---------------------------------------------------------------------------
# 14b. DID THE HEIGHT FILL REACH OUTSIDE THE SILHOUETTE AT ALL?
#
#     THIS IS THE ROW THAT FAILS ON exe af457755. Its bake's law was "the card
#     plane everywhere the object does not cover"; this exe's is "the object's
#     own depth for eight rings, the plane beyond", because that is where a
#     neighbouring frame's ray lands. Row 14 asks whether the field is
#     CONTINUOUS across the silhouette; this one asks the blunter question
#     that no amount of BC3 noise can fake: is anything out there off the
#     plane at all?
#
#     Over the frames of the set, the MEAN fraction of the 8-ring band whose
#     height is more than 12 levels off 128. Measured on five subjects, both
#     sheet sets, same instrument:
#
#       subject    af457755's sheets   this exe's bake
#       blast_n4         2.0%               19.8%
#       blast_n8         2.4%               25.8%
#       maple_n4         3.4%               27.7%
#       dead_n4          4.3%               28.5%
#       rock_n4          5.7%               72.9%
#
#     The floor is 10 per cent: twice the worst old number, half the best new
#     one. It is a MEAN over frames and NOT a per-frame test on purpose --
#     frames at 0.0% exist on BOTH sheet sets, because a frame whose local
#     surface sits at the card plane has nothing to carry outward, and a
#     per-frame test would convict the repair for the subject's geometry.
# ---------------------------------------------------------------------------
REACH_FLOOR="${IMPOSTOR_REACH_FLOOR:-0.10}"
if "$PY" - "$IMPOSTOR_LODM" "$REACH_FLOOR" "$here" > "$work/reach.txt" 2>&1 <<'PYEOF'
import sys, os, glob, json
import numpy as np
sys.path.insert(0, sys.argv[3])          # tests/spells, passed in as $here
from impostor_bc_decode import load_dds

FULL, FLOOR, PLANE, SLACK, NEAR = 250, 16, 128, 12, 8
lodm, floor = sys.argv[1], float(sys.argv[2])


def rings(mask, n):
    m = mask
    for _ in range(n):
        p = np.zeros((m.shape[0] + 2, m.shape[1] + 2), bool)
        p[1:-1, 1:-1] = m
        m = (p[:-2, :-2] | p[:-2, 1:-1] | p[:-2, 2:] |
             p[1:-1, :-2] | p[1:-1, 1:-1] | p[1:-1, 2:] |
             p[2:, :-2] | p[2:, 1:-1] | p[2:, 2:])
    return m


raw = open(lodm, 'rb').read()
j = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
N = int(j['oct'])
d = os.path.dirname(os.path.abspath(lodm))
stem = os.path.basename(lodm)[:-len('_oct.lodm')]
alb = load_dds(glob.glob(os.path.join(d, stem + '_oct_d.DDS'))[0])[0]
nsh = load_dds(glob.glob(os.path.join(d, stem + '_oct_n.DDS'))[0])[0]
H, W = alb.shape[:2]
fw, fh = W // N, H // N
a = alb[..., 3] * 255.0
h = nsh[..., 2] * 255.0
fr = []
for jj in range(N):
    for ii in range(N):
        ys, xs = slice(jj * fh, (jj + 1) * fh), slice(ii * fw, (ii + 1) * fw)
        af, hf = a[ys, xs], h[ys, xs]
        full = af >= FULL
        if full.sum() < 8:
            continue
        band = (af < FLOOR) & rings(full, NEAR)
        if band.sum() < 16:
            continue
        fr.append(float((np.abs(hf[band] - PLANE) > SLACK).sum()) / band.sum())
if not fr:
    print('outside-band reach: no frame has both whole texels and an 8-ring band -- REFUSED')
    sys.exit(2)
m = float(np.mean(fr))
print('outside-band reach: %d frames, mean %.1f%% of the 8-ring band off the card '
      'plane (floor %.0f%%), worst frame %.1f%%, best %.1f%%'
      % (len(fr), 100 * m, 100 * floor, 100 * min(fr), 100 * max(fr)))
sys.exit(0 if m >= floor else 1)
PYEOF
then
	ok "14b $( tail -1 "$work/reach.txt" )"
else
	sed -n '1,4p' "$work/reach.txt" | while IFS= read -r l; do say "      $l"; done
	bad "14b $( tail -1 "$work/reach.txt" ) -- the height outside the silhouette is the card plane, i.e. the bake predates the 8-ring fill"
fi

# ---------------------------------------------------------------------------
# 15. THE PHOTOGRAPH ROW -- the drawer's one KNOWN ANSWER.
'''
once(A2); s = s.replace(A2, B2)

out = s.encode('utf-8')
if out.count(b'\r') != cr0:
    print('CR COUNT MOVED %d -> %d' % (cr0, out.count(b'\r'))); sys.exit(2)
open(P, 'wb').write(out)
print('fix05 applied: %d -> %d bytes, CR %d -> %d' % (len(b), len(out), cr0, out.count(b'\r')))
