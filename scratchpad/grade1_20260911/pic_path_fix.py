"""Put the two pictures where every other lane keeps them: <lane>/images/.

Patches g7_pictures.py's two save paths and moves the files already drawn, so
the script and the disk agree and a re-run overwrites rather than forks.
"""
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'g7_pictures.py')

t = open(SRC, encoding='utf-8').read()
assert t.count('\r') == 0
for name in ('cmp_tone.png', 'curve.png'):
    old = "os.path.join(HERE, '%s')" % name
    assert t.count(old) == 1, name
    t = t.replace(old, "os.path.join(HERE, 'images', '%s')" % name)
assert "os.makedirs" not in t
t = t.replace("HERE = os.path.dirname(os.path.abspath(__file__))",
              "HERE = os.path.dirname(os.path.abspath(__file__))\n"
              "os.makedirs(os.path.join(HERE, 'images'), exist_ok=True)", 1)
open(SRC, 'w', encoding='utf-8', newline='\n').write(t)

d = os.path.join(HERE, 'images')
os.makedirs(d, exist_ok=True)
for name in ('cmp_tone.png', 'curve.png'):
    s = os.path.join(HERE, name)
    if os.path.exists(s):
        shutil.move(s, os.path.join(d, name))
    print('%-14s %d bytes' % (name, os.path.getsize(os.path.join(d, name))))
