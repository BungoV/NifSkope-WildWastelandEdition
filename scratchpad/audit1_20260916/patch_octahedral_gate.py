"""AUDIT1 step 1/6: lodgen_octahedral.sh pins the card sheets' GAME-RELATIVE
path, and lane LAYOUT1 moved it on 2026-09-16.

  src/lodgen.cpp:2928  "`Data\\Textures\\Lodgen\\Cards\\` until today,
                        `Data\\FO4CSLOD\\Cards\\` now"
  src/lodgenlayout.cpp:39  lodgenFo4csGameCardPath() = FO4CSLOD\\Cards

Two checks in the gate still spell the old prefix, so both read FAIL on a
writer that is doing exactly what the ruling says. That is a STALE GATE, and
the string stays a LITERAL: pinning the path is the whole point of the check,
and the proof that it still has teeth is that it was red until this edit.
"""
import io
import os
import sys
import tempfile

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_octahedral.sh'
BS = chr(92)
OLD = "'Data" + BS + BS + "Textures" + BS + BS + "Lodgen" + BS + BS + "Cards" + BS + BS + "'"
NEW = "'Data" + BS + BS + "FO4CSLOD" + BS + BS + "Cards" + BS + BS + "'"

s = io.open(P, encoding='utf-8', newline='').read()
n = s.count(OLD)
if n != 2:
    print('ABORT: the old card path literal appears %d times, expected 2' % n)
    sys.exit(1)
out = s.replace(OLD, NEW)
assert out.count(NEW) == 2 and out.count(OLD) == 0
assert out.count('\r') == s.count('\r')
d = os.path.dirname(P)
f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d, delete=False, suffix='.tmp')
f.write(out)
f.close()
os.replace(f.name, P)
print('lodgen_octahedral.sh: 2 card paths moved to %s; %d -> %d bytes, CR %d'
      % (NEW, len(s), len(out), out.count('\r')))
