"""lane PERFLOAD1 P7: offline gate for the far tree ladder (src/features/far-lod/treeladder.h).

A twin of the header's pick / select / advance / weight; the constants are read from the header so the twin cannot
drift. Gates: the screen fraction follows the live projection (two fields of view, two answers); a sweep from near to
far walks every available step in order, each switch at its own threshold; a tree hovering round a threshold switches
once, not every frame (20 % hysteresis); the way back needs threshold x 1.2; missing MNAM slots fall through to the next
step present; the cross-fade's two weights sum to 1 and a turn back mid-fade runs the same fade backwards.
Red: --red drops the hysteresis; the hover gate must then FAIL.
Round 2: the rows' dither (TreeLadder::rowFade + fo4_default.frag's rule) and the card's tile every pixel once through
each fade; a fade ending before the row commit pops nothing; four MNAM slots walk in order; one caster per tree.
Red: --red2 gives the coarser row the finer side's pixels; the coverage gate must then FAIL.
Run: python -I tests/features/far-lod/treeladder/ladder_gate.py [--red | --red2]
"""
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HEADER = os.path.join(HERE, '..', '..', '..', '..', 'src', 'features', 'far-lod', 'treeladder.h')

src = open(HEADER, encoding='utf-8').read()
HYST = float(re.search(r'kHysteresis = ([0-9.]+)f', src).group(1))
FADE = float(re.search(r'kFadeSeconds = ([0-9.]+)f', src).group(1))
THR = [float(x.strip().rstrip('f')) for x in re.search(r'thr\[Steps - 1\] = \{([^}]*)\}', src).group(1).split(',')]
STEPS, IMPOSTOR = 6, 5
RED = '--red' in sys.argv
if RED:
    HYST = 1.0

fails = []
checks = 0


def gate(ok, what):
    global checks
    checks += 1
    print(('PASS ' if ok else 'FAIL ') + what)
    if not ok:
        fails.append(what)


def frac(radius, dist, p11):
    return 1.0 if dist <= 1.0 else radius / dist * p11 * 0.5


def keep(s, avail):
    for n in range(s + 1, STEPS):
        if avail & (1 << n):
            return THR[n - 1]
    return -1.0


def pick(f, avail, factor):
    last = -1
    for s in range(STEPS):
        if not avail & (1 << s):
            continue
        last = s
        b = keep(s, avail)
        if b < 0 or f >= b * factor:
            return s
    return last


def select(f, avail, cur):
    now = pick(f, avail, 1.0)
    if cur < 0 or cur >= STEPS or not avail & (1 << cur) or now >= cur:
        return now
    finer = pick(f, avail, HYST)
    return min(finer, cur)


class State:
    def __init__(self):
        self.step, self.frm, self.t = -1, -1, 1.0


def fading(s):
    return s.frm >= 0 and s.t < 1.0


def advance(s, want, dt):
    if s.t < 1.0:
        s.t = min(1.0, s.t + dt / FADE)
    if not fading(s):
        s.frm = -1
    if want == s.step:
        return False
    if s.step < 0:
        s.step, s.frm, s.t = want, -1, 1.0
        return True
    if fading(s) and want == s.frm:
        s.frm, s.step, s.t = s.step, want, 1.0 - s.t
        return True
    s.frm, s.step, s.t = s.step, want, 0.0
    return True


def weight(s, which):
    if which == s.step:
        return s.t if fading(s) else 1.0
    if fading(s) and which == s.frm:
        return 1.0 - s.t
    return 0.0


ALL = 0x3F
print('header: thresholds %s, hysteresis x%.2f, fade %.2f s%s' % (THR, HYST, FADE, ' (RED: no hysteresis)' if RED else ''))

# 1. the live projection: P11 = 1 / tan(fovY / 2); a narrower field doubles the size on screen
p75, p40 = 1.0 / math.tan(math.radians(37.5)), 1.0 / math.tan(math.radians(20.0))
f75, f40 = frac(600.0, 30000.0, p75), frac(600.0, 30000.0, p40)
gate(abs(f75 - 600.0 / 30000.0 * p75 / 2) < 1e-9 and f40 > f75 * 2.0,
     'fraction from the live projection: 75 deg %.4f, 40 deg %.4f of the screen height' % (f75, f40))

# 2. near to far: every step in order, each switch at its own threshold (r 600, 75 deg)
cur, seen, at = -1, [], {}
for i in range(4000):
    d = 1000.0 * 1.002 ** i
    f = frac(600.0, d, p75)
    n = select(f, ALL, cur)
    if n != cur:
        seen.append(n)
        at[n] = f
    cur = n
gate(seen == [0, 1, 2, 3, 4, 5], 'sweep out walks full, rep0..rep3, impostor in order: %s' % seen)
gate(all(abs(at[s] - THR[s - 1]) / THR[s - 1] < 0.003 for s in range(1, 6)),
     'each coarser switch at its own threshold: %s' % ['%.4f' % at.get(s, -1) for s in range(1, 6)])

# 3. hover: a tree breathing +-10 % round the impostor threshold for 600 frames switches once at most
cur, sw = select(THR[4] * 1.15, ALL, -1), 0
for i in range(600):
    f = THR[4] * (1.0 + 0.10 * math.sin(i * 0.37))
    n = select(f, ALL, cur)
    sw += n != cur
    cur = n
gate(sw <= 1, 'hover +-10 %% round %.3f: %d switches (1 at most)' % (THR[4], sw))

# 4. the way back needs threshold x 1.2
cur = select(THR[4] * 0.9, ALL, -1)
back_at = None
for i in range(400):
    f = THR[4] * 0.9 * 1.001 ** i
    n = select(f, ALL, cur)
    if n < cur and back_at is None:
        back_at = f
    cur = n
gate(back_at is not None and back_at >= THR[4] * 1.2 * 0.999,
     'back from the impostor at %.4f (needs >= %.4f)' % (back_at or -1, THR[4] * 1.2))

# 5. missing slots: {rep1, impostor} keeps rep1 down to the impostor's own boundary; {impostor} alone never climbs
av = (1 << 2) | (1 << IMPOSTOR)
gate(pick(THR[4] * 1.01, av, 1.0) == 2 and pick(THR[4] * 0.99, av, 1.0) == IMPOSTOR,
     'MNAM gaps: rep1 + impostor switch at the impostor boundary %.3f' % THR[4])
gate(select(0.9, 1 << IMPOSTOR, -1) == IMPOSTOR, 'impostor only (no mesh row): always the impostor')
gate(pick(0.5, 0, 1.0) == -1, 'nothing available: -1')

# 6. the cross-fade
s = State()
advance(s, 1, 0.0)
advance(s, IMPOSTOR, 0.016)
ok = True
for i in range(10):
    advance(s, IMPOSTOR, 0.016)
    ok &= abs(weight(s, 1) + weight(s, IMPOSTOR) - 1.0) < 1e-6
mid = weight(s, IMPOSTOR)
advance(s, 1, 0.0)
gate(ok, 'fade weights sum to 1 every frame')
gate(abs(weight(s, IMPOSTOR) - mid) < 1e-6 and fading(s),
     'a turn back mid-fade runs the same fade backwards: the card stays at %.3f, no jump' % mid)
for i in range(40):
    advance(s, 1, 0.016)
gate(not fading(s) and weight(s, 1) == 1.0 and weight(s, IMPOSTOR) == 0.0, 'the fade ends in %.2f s' % FADE)
s2 = State()
advance(s2, IMPOSTOR, 0.016)
gate(not fading(s2), 'first sight: no fade')

# --- round 2 (P7 r2): the mesh rows dither too, every MNAM step is a row, the casters take the target step ---
FRAG = open(os.path.join(HERE, '..', '..', '..', '..', 'res', 'shaders', 'fo4_default.frag'), encoding='utf-8').read()
OCT = open(os.path.join(HERE, '..', '..', '..', '..', 'res', 'shaders', 'features', 'far-lod', 'impostor_oct.frag'),
           encoding='utf-8').read()
RED2 = '--red2' in sys.argv	# the coarser row keeps the finer side's pixels: the coverage gate must FAIL
gate('n >= farLadder.x - 1.0 : n < 2.0 + farLadder.x' in FRAG and '< cardFadeOut' in OCT
     and '5.588238 * farFadeFrame' in FRAG and '5.588238 * cardFadeFrame' in OCT,
     'fo4_default.frag dithers the rows with the cards\' noise and rule (impostor_oct.frag)')


def drawmask(s):
    m = 0
    if 0 <= s.step < STEPS:
        m |= 1 << s.step
    if fading(s):
        m |= 1 << s.frm
    return m


def rowfade(s, draw, mask):
    if not mask & draw:
        return 1.0
    if not fading(s):
        return 0.0
    has_to, has_from = bool(mask >> s.step & 1), bool(mask >> s.frm & 1)
    if has_to == has_from:
        return 0.0
    w = s.t if has_to else 1.0 - s.t
    finer = s.step < s.frm if has_to else s.frm < s.step
    if RED2:
        finer = True
    return 1.0 + w if finer else -(1.0 + w)


def ign(x, y, frame):
    fx, fy = x + 0.5 + 5.588238 * frame, y + 0.5 + 5.588238 * frame
    v = (fx * 0.06711056 + fy * 0.00583715) % 1.0
    return (52.9829189 * v) % 1.0


def row_keeps(f, n):	# the shader's rule
    if f == 0.0:
        return True
    return not (n >= f - 1.0 if f > 0.0 else n < 2.0 + f)


def card_keeps(w, n):	# impostor_oct.frag: discard when n < cardFadeOut = 1 - w
    return not (1.0 - w > 0.0 and n < 1.0 - w)


def coverage(s, masks, card, frame):
	# every pixel of a 48 x 48 tile drawn by exactly one side; returns (worst count error, kept fraction per side)
    draw = drawmask(s)
    bad, kept = 0, [0] * (len(masks) + 1)
    for y in range(48):
        for x in range(48):
            n = ign(x, y, frame)
            c = 0
            for k, m in enumerate(masks):
                if m & draw and row_keeps(rowfade(s, draw, m), n):
                    c += 1
                    kept[k] += 1
            if card and weight(s, IMPOSTOR) > 0.0 and card_keeps(weight(s, IMPOSTOR), n):
                c += 1
                kept[-1] += 1
            bad += c != 1
    return bad, [k / 2304.0 for k in kept]


R = [1 << (1 + r) for r in range(4)]	# one row a distinct MNAM mesh: rep0..rep3
worst = 0
for a, b, card in ((1, 3, False), (3, 1, False), (2, IMPOSTOR, True), (IMPOSTOR, 4, True), (1, 2, False)):
    s = State()
    advance(s, a, 0.0)
    advance(s, b, 0.0)
    for i in range(1, 22):
        advance(s, b, 0.016)
        bad, kept = coverage(s, R, card, i)
        worst = max(worst, bad)
gate(worst == 0, 'mesh row vs mesh row and mesh row vs card: every pixel drawn once in every frame of each fade '
     '(worst %d of 2304)' % worst)
s = State()
advance(s, 2, 0.0)
advance(s, 4, 0.0)
advance(s, 4, FADE * 0.25)
bad, kept = coverage(s, R, False, 3)
gate(abs(kept[3] - 0.25) < 0.04 and abs(kept[1] - 0.75) < 0.04,
     'the dither keeps the fade weights: incoming %.3f (0.25), outgoing %.3f (0.75)' % (kept[3], kept[1]))

# a fade that ends before the row filter's commit: the old row keeps no pixel (1), the new one all (0): no pop
s = State()
advance(s, 1, 0.0)
advance(s, 3, 0.0)
advance(s, 3, FADE * 2.0)
gate(not fading(s) and rowfade(s, drawmask(s), R[0]) == 1.0 and rowfade(s, drawmask(s), R[2]) == 0.0,
     'fade over before the commit: the old row dithers to nothing, the new one whole')
# one mesh for two slots (rep0 == rep1): its row draws whole through that switch
s = State()
advance(s, 1, 0.0)
advance(s, 2, 0.0)
advance(s, 2, 0.1)
gate(rowfade(s, drawmask(s), R[0] | R[1]) == 0.0, 'one mesh serving both steps: no dither between them')

# every step a tree has: rep0..rep3 rows + the impostor, walked in order (the far rings hold no full mesh)
av = (1 << IMPOSTOR) | R[0] | R[1] | R[2] | R[3]
cur, seen = -1, []
for i in range(4000):
    n = select(frac(600.0, 1000.0 * 1.002 ** i, p75), av, cur)
    if n != cur:
        seen.append(n)
    cur = n
gate(seen == [1, 2, 3, 4, 5], 'a tree with four MNAM slots walks rep0, rep1, rep2, rep3, impostor: %s' % seen)

# the casters: one per tree at every step (its target step's row, else the card)
ok = True
for st in range(1, 6):
    cast = (1 << st) & (av & 0x1E)
    rows_cast = sum(1 for m in R if m & cast)
    card_cast = 0 if cast else 1
    ok &= rows_cast + card_cast == 1
gate(ok, 'shadow casters: exactly one per tree on every step (the step\'s row, the card on the impostor)')

print('%s: %d of %d checks pass%s' % ('LADDER GATE PASS' if not fails else 'LADDER GATE FAIL', checks - len(fails), checks,
                                      ' (RED)' if RED else (' (RED2)' if RED2 else '')))
sys.exit(1 if fails else 0)
