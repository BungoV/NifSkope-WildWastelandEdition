"""lane PERFLOAD1 P7: offline gate for the far tree ladder (src/features/far-lod/treeladder.h).

A twin of the header's pick / select / advance / weight; the constants are read from the header so the twin cannot
drift. Gates: the screen fraction follows the live projection (two fields of view, two answers); a sweep from near to
far walks every available step in order, each switch at its own threshold; a tree hovering round a threshold switches
once, not every frame (20 % hysteresis); the way back needs threshold x 1.2; missing MNAM slots fall through to the next
step present; the cross-fade's two weights sum to 1 and a turn back mid-fade runs the same fade backwards.
Red: --red drops the hysteresis; the hover gate must then FAIL.
Run: python -I tests/features/far-lod/treeladder/ladder_gate.py [--red]
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

print('%s: %d of %d checks pass%s' % ('LADDER GATE PASS' if not fails else 'LADDER GATE FAIL', checks - len(fails), checks,
                                      ' (RED)' if RED else ''))
sys.exit(1 if fails else 0)
