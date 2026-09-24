"""LAND1 gate A3 -- BORDER CONTINUITY -- and gate A7 -- thread identity.

A3 asks the only question that matters for a rule that reads the terrain: is the
sheet a function of WORLD position alone, or of the region rectangle the baker
happened to be handed?  Four region rectangles, three of which contain the chunk
(-20,20) and three of which contain its eastern neighbour (-16,20):

    split20   r:-20,20,-17,23     the chunk alone
    split16   r:-16,20,-13,23     the neighbour alone
    joint     r:-20,20,-13,23     both in ONE bake  -> the ring offsets differ
    joint4    r:-24,20,-13,23     both again from a THIRD origin

Every chunk sheet that appears in more than one rectangle must be byte-identical
in all of them, for every rule.  A lattice anchored to the RING rather than to
the WORLD, or a macro gradient that reaches past the ring and clamps, cannot
survive this -- the ring origin is different in all four.

THE FLOOR IS NOT SYNTHETIC.  The same chunk baked at two macro scales (1024 and
512) must DIFFER.  Without that, the identity above could be identity by
inaction -- a guide that reads nothing is trivially continuous.

A7 is the same comparator on the joint bake at 1 chunk thread against 16.

    python a3_gate.py  ->  logs/a3_gate.txt
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'out')
RULES = [('off', 'no rule (hex 256 + mip bias only)'),
         ('aspecthex10', 'aspecthex:1.0'),
         ('drag171', 'drag:171'),
         ('aspect10', 'aspect:1.0'),
         ('slopewarp10', 'slopewarp:1.0 --land-warp 341'),
         ('flatwarp10', 'flatwarp:1.0 --land-warp 341')]
TILE = {'split20': 'r_-20_20_-17_23', 'split16': 'r_-16_20_-13_23',
        'joint': 'r_-20_20_-13_23', 'joint4': 'r_-24_20_-13_23'}
L = []


def say(s):
    L.append(s)
    print(s)


def sha(p):
    if not os.path.isfile(p):
        return None
    return hashlib.sha1(open(p, 'rb').read()).hexdigest()


def sheets(variant, arm, cx, cy):
    """the three tex files of one chunk: colour, _msn, _data."""
    d = os.path.join(OUT, variant, TILE[arm], 'tex')
    out = {}
    if not os.path.isdir(d):
        return out
    for f in sorted(os.listdir(d)):
        if f.startswith('Commonwealth.4.%d.%d' % (cx, cy)):
            out[f] = sha(os.path.join(d, f))
    return out


def main():
    say('LAND1 gate A3 -- border continuity: the sheet is a function of WORLD')
    say('position, not of the region rectangle.  Four rectangles, every chunk')
    say('sheet that appears in more than one compared byte for byte.')
    say('')
    say('   %-14s %-9s %-28s %s' % ('rule', 'chunk', 'rectangles', 'verdict'))
    bad = 0
    nfiles = 0
    for tag, name in RULES:
        for cx, cy, arms in ((-20, 20, ['split20', 'joint', 'joint4']),
                             (-16, 20, ['split16', 'joint', 'joint4'])):
            got = {}
            for a in arms:
                got[a] = sheets('c_%s_%s' % (tag, a), a, cx, cy)
            base = got[arms[0]]
            if not base:
                say('   %-14s %-9s NOT BAKED' % (name[:14], '%d,%d' % (cx, cy)))
                bad += 1
                continue
            diff = []
            for a in arms[1:]:
                if set(got[a]) != set(base):
                    diff.append('%s: file set differs' % a)
                    continue
                for f in base:
                    nfiles += 1
                    if got[a][f] != base[f]:
                        diff.append('%s: %s' % (a, f))
            say('   %-14s %-9s %-28s %s'
                % (name[:14], '%d,%d' % (cx, cy), '+'.join(arms),
                   'identical' if not diff else 'RED ' + '; '.join(diff[:3])))
            if diff:
                bad += 1
    say('')
    say('   %d file comparisons across the rectangles, %d rows RED' % (nfiles, bad))

    say('')
    say('THE FLOOR THAT MUST FIRE -- the same chunk, two macro scales:')
    a = sheets('c_floor_s1024', 'split20', -20, 20)
    b = sheets('c_floor_s512', 'split20', -20, 20)
    moved = [f for f in a if f in b and a[f] != b[f]]
    fired = len(moved) > 0
    say('   --land-guide-scale 1024 vs 512, aspecthex:1.0   %d of %d files differ  %s'
        % (len(moved), len(a),
           'the guide IS reading the heightmap' if fired
           else 'RED: THE GUIDE READS NOTHING, so A3 above is identity by inaction'))
    for f in moved:
        say('       %-34s %s vs %s' % (f, a[f][:12], b[f][:12]))
    if a and not moved:
        say('   (the colour sheet is the one that must move; _msn and _data must not)')

    say('')
    say('GATE A7 -- the winning rule at 1 chunk thread against 16, whole tile:')
    d1 = os.path.join(OUT, 'c_thr1', TILE['joint'])
    d16 = os.path.join(OUT, 'c_thr16', TILE['joint'])
    n = 0
    d = []
    for dp, _dn, fn in os.walk(d1):
        for f in fn:
            if f == 'bake.log':
                continue
            p1 = os.path.join(dp, f)
            p16 = os.path.join(d16, os.path.relpath(p1, d1))
            n += 1
            if sha(p1) != sha(p16):
                d.append(os.path.relpath(p1, d1))
    thr_ok = n > 0 and not d
    say('   %d files compared, %d differ   %s'
        % (n, len(d), 'IDENTICAL' if thr_ok else 'RED ' + ', '.join(d[:4])))

    say('')
    verdict = (bad == 0) and fired
    say('A3 %s    A7 %s' % ('GREEN' if verdict else 'RED',
                            'GREEN' if thr_ok else 'RED'))
    with open(os.path.join(HERE, 'logs', 'a3_gate.txt'), 'w', newline='\n') as f:
        f.write('\n'.join(L) + '\n')
    return 0 if (verdict and thr_ok) else 1


if __name__ == '__main__':
    sys.exit(main())
