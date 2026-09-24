#!/usr/bin/env python3
"""Lane BUILD7: APPLY lane HKX5's refusing hook-up (scratchpad/hkx5_20260910/hookup.py).

The EDITS table is IMPORTED from HKX5's own script -- the anchors are never
retyped here (skill `ww-anchored-hookup`: take the code from its own source).

Rules enforced:
  * measure line endings first, byte counts, never grep;
  * every anchor must occur EXACTLY ONCE, and its insertion must be ABSENT;
  * after the write: CR count unchanged, LF count +len(EDITS), bytes grow by
    exactly the inserted text.
Refuses (exit 3) and writes nothing if any assertion fails.
"""
import os, sys, importlib.util

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
HOOKUP = os.path.join(REPO, 'scratchpad', 'hkx5_20260910', 'hookup.py')

spec = importlib.util.spec_from_file_location('hkx5_hookup', HOOKUP)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
EDITS = mod.EDITS

pro = os.path.join(REPO, 'NifSkope.pro')
raw = open(pro, 'rb').read()
cr0, lf0, n0 = raw.count(b'\r'), raw.count(b'\n'), len(raw)
print('BEFORE  %s  bytes=%d  LF=%d  CR=%d' % (pro, n0, lf0, cr0))
if cr0 != 0:
    print('REFUSED: NifSkope.pro is not LF-only (CR=%d); the anchors carry no CR.' % cr0)
    sys.exit(3)

text = raw.decode('utf-8')
added = 0
for anchor, ins, what in EDITS:
    a = anchor + '\n'
    i = ins + '\n'
    print('  anchor %r  count=%d ; insertion %r  count=%d' % (anchor, text.count(a), ins, text.count(i)))
    if text.count(i) != 0:
        print('REFUSED: %r already present.' % ins)
        sys.exit(3)
    if text.count(a) != 1:
        print('REFUSED: anchor %r occurs %d times, not once.' % (anchor, text.count(a)))
        sys.exit(3)
    text = text.replace(a, a + i)
    added += len(i.encode('utf-8'))

out = text.encode('utf-8')
cr1, lf1, n1 = out.count(b'\r'), out.count(b'\n'), len(out)
assert cr1 == cr0, (cr1, cr0)
assert lf1 == lf0 + len(EDITS), (lf1, lf0)
assert n1 == n0 + added, (n1, n0, added)
open(pro, 'wb').write(out)
print('AFTER   bytes=%d (+%d)  LF=%d (+%d)  CR=%d  -- APPLIED %d lines'
      % (n1, n1 - n0, lf1, lf1 - lf0, cr1, len(EDITS)))
