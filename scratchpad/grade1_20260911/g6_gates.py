"""GRADE1 gate G3 -- the shipped switch, measured through the BINARY.

Pre-registered before the first graded bake was read (report section 0a):

  S1  with no flag, and with `--grade 1.0`, every file of both test tiles is
      byte-identical to the rung's bake.  [already run, 24/24 identical]
  S2  the binary reproduces the Python prediction for a given k to within
      1.0 level of RGB RMS.  Two sources of a gap are known and named in
      advance: the sheet is re-encoded to BC1 after grading (floor ~0.5 RMS),
      and the crevice term -3.242*div is added AFTER the grade in the binary
      but is already inside the sheet the Python scales, so the two differ by
      crevice*(1-k).
  S3  `--grade k_opt` strictly reduces the whole-tile RGB RMS on the tile whose
      optimum it is, on both tiles, each with its OWN k.
  S4  the REFUSED gate, stated as an assertion and not as prose: no single k
      reduces the error on both tiles.  Checked here against the binary's own
      output by running the pooled k = 0.8403 on both.
  S5  the flag is new: the rung exe exits 2 with "unknown option --grade".

Usage: python g6_gates.py
"""

import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gradelib as G                                        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
out = {'checks': 0, 'fails': 0, 'rows': []}


def chk(name, cond, detail):
    out['checks'] += 1
    if not cond:
        out['fails'] += 1
    print('%-4s %-44s %s' % ('ok' if cond else 'FAIL', name, detail))
    out['rows'].append({'name': name, 'pass': bool(cond), 'detail': detail})


ARMS = [('(-20,24)', (-20, 24), 'g8916', 0.8916),
        ('(-20,20)', (-20, 20), 'g1118', 1.1180),
        ('(-20,24)', (-20, 24), 'g8403', 0.8403),
        ('(-20,20)', (-20, 20), 'g8403', 0.8403)]

base = {}
for tag, (cx, cy), arm, k in ARMS:
    if tag in base:
        continue
    o, v = G.rgb(G.ours('def', cx, cy)), G.rgb(G.van(cx, cy))
    base[tag] = (o, v, G.resid(o, v)['rms'])

print('--- S2/S3 the binary against the prediction')
print('%-10s %-7s %9s %9s %9s %9s' % ('tile', 'k', 'before', 'predicted',
                                      'binary', 'gap'))
for tag, (cx, cy), arm, k in ARMS:
    o, v, r0 = base[tag]
    pred = G.resid(o * k, v)['rms']
    g = G.rgb(G.ours(arm, cx, cy))
    got = G.resid(g, v)['rms']
    print('%-10s %-7.4f %9.3f %9.3f %9.3f %+9.3f'
          % (tag, k, r0, pred, got, got - pred))
    chk('S2 binary matches prediction %s k=%.4f' % (tag, k),
        abs(got - pred) <= 1.0, 'gap %+0.3f levels (tolerance 1.0)' % (got - pred))
    out['rows'][-1].update({'before': r0, 'pred': pred, 'binary': got})
    if abs(k - dict((('(-20,24)', 0.8916), ('(-20,20)', 1.1180)))[tag]) < 1e-6:
        chk('S3 own-optimum k reduces the error %s' % tag, got < r0,
            '%.3f -> %.3f (%+0.1f%%)' % (r0, got, 100.0 * (got / r0 - 1)))

print('\n--- S4 the refusal, checked against the binary')
p24 = G.resid(G.rgb(G.ours('g8403', -20, 24)), base['(-20,24)'][1])['rms']
p20 = G.resid(G.rgb(G.ours('g8403', -20, 20)), base['(-20,20)'][1])['rms']
chk('S4 one k cannot reduce both (pooled k=0.8403)',
    (p24 < base['(-20,24)'][2]) != (p20 < base['(-20,20)'][2]),
    '(-20,24) %.3f->%.3f, (-20,20) %.3f->%.3f'
    % (base['(-20,24)'][2], p24, base['(-20,20)'][2], p20))

print('\n--- S5 the flag is new')
log = os.path.join(HERE, 'out', 'old_g8403', 't2024', 'bake.log')
txt = open(log).read() if os.path.exists(log) else ''
chk('S5 rung exe refuses --grade', 'unknown option --grade' in txt,
    txt.strip().splitlines()[-1] if txt.strip() else '(no log)')

print('\nGATES: %d checks, %d failures -> %s'
      % (out['checks'], out['fails'], 'PASS' if out['fails'] == 0 else 'FAIL'))
with open(os.path.join(HERE, 'g6_gates.json'), 'w') as f:
    json.dump(out, f, indent=1, default=float)
sys.exit(1 if out['fails'] else 0)
