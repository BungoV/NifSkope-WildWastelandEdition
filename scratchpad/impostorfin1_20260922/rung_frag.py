#!/usr/bin/env python
"""IMPOSTORFIN1 -- rebuild the impostor_oct.frag the RUNG exe shipped with.

The rung exe reads release/shaders/ at run time (glcontext.cpp:937,
applicationDirPath()/shaders), so running it out of release/ draws with THIS
lane's shader. A true "before" needs its own folder with the old shader. The old
frag is the current one with this lane's two edits undone, exactly once each:
patch_cut.py's two frag pairs and hookup_cardlight.py's three pairs (new -> old).
The result must be 19978 bytes, sha1 badf7eb9..., which is what --check read
before the hook-up was applied; anything else is refused.

    python rung_frag.py OUT.frag
"""
import sys, io, hashlib, importlib.util

SRC = 'E:/Projects/NifskopeWildWastelandEdition/res/shaders/impostor_oct.frag'


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


pc = load('E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfin1_20260922/patch_cut.py', 'pc')
hc = load('E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorlook1_20260919/hookup_cardlight.py', 'hc')

t = io.open(SRC, 'r', encoding='utf-8', newline='').read()
pairs = [('cut F2', pc.F2_NEW, pc.F2_OLD), ('cut F1', pc.F1_NEW, pc.F1_OLD)]
pairs += [('cardlight ' + n, new, old) for n, old, new in reversed(hc.PAIRS)]
for n, new, old in pairs:
    c = t.count(new)
    print('%-40s %d match' % (n, c))
    assert c == 1, n
    t = t.replace(new, old, 1)
b = t.encode('utf-8')
h = hashlib.sha1(b).hexdigest()
print('rebuilt %d bytes sha1 %s' % (len(b), h))
assert len(b) == 19978 and h.startswith('badf7eb9'), 'not the pre-lane frag'
open(sys.argv[1], 'wb').write(b)
print('WROTE', sys.argv[1])
