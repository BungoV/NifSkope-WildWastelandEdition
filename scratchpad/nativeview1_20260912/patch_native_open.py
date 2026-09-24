import sys

p = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/native_open.sh'
s = open(p, 'rb').read().decode('utf-8')
before = len(s)

# 1. the chunk-local centre for every legacy (.BTO/.BTR) control
old = 'ORTHO=$(( DIM * 4096 / 2 ))\n'
new = ('ORTHO=$(( DIM * 4096 / 2 ))\n'
       '# The .BTO/.BTR of a chunk are CHUNK-LOCAL: their root translation is 0,0,0\n'
       '# and their vertices run 0..(DIM*4096).  Pointing the world pin at them gives\n'
       '# an empty frame (measured: a 5,179-byte PNG).  Every legacy control therefore\n'
       "# keeps the same ortho half-width and window but sits at the chunk's own centre.\n"
       'LCENTER="$(( DIM * 4096 / 2 )),$(( DIM * 4096 / 2 )),0"\n')
assert s.count(old) == 1, 'ORTHO anchor %d' % s.count(old)
s = s.replace(old, new)

# 2. shot() honours a per-call centre override
old = '\tlocal out="$1" file="$2"; shift 2\n'
new = '\tlocal out="$1" file="$2"; shift 2\n\tlocal ctr="${SHOT_CENTER:-$CENTER}"\n'
assert s.count(old) == 1, 'shot anchor %d' % s.count(old)
s = s.replace(old, new)

old = 'WW_RENDER_CENTER="$CENTER" WW_RENDER_ORTHO="$ORTHO"'
new = 'WW_RENDER_CENTER="$ctr" WW_RENDER_ORTHO="$ORTHO"'
assert s.count(old) == 1, 'center anchor %d' % s.count(old)
s = s.replace(old, new)

# 3. the pin line names both centres
old = 'echo "pin: centre $CENTER, ortho half-width $ORTHO'
new = 'echo "pin: centre $CENTER (legacy chunk-local centre $LCENTER), ortho half-width $ORTHO'
assert s.count(old) == 1, 'pin anchor %d' % s.count(old)
s = s.replace(old, new)

# 4. every .BTO / .BTR shot gets SHOT_CENTER=$LCENTER
pairs = [
    ('\tif shot "$W/c_bto.png" "$BTO" ',
     '\tif SHOT_CENTER="$LCENTER" shot "$W/c_bto.png" "$BTO" '),
    ('\tif [ -f "$BTO2" ] && shot "$W/c_other.png" "$BTO2" ',
     '\tif [ -f "$BTO2" ] && SHOT_CENTER="$LCENTER" shot "$W/c_other.png" "$BTO2" '),
    ('\tif [ -f "$BTR" ] && shot "$W/d_btr.png" "$BTR" ',
     '\tif [ -f "$BTR" ] && SHOT_CENTER="$LCENTER" shot "$W/d_btr.png" "$BTR" '),
]
for old, new in pairs:
    assert s.count(old) == 1, 'legacy anchor %r %d' % (old, s.count(old))
    s = s.replace(old, new)

open(p, 'wb').write(s.encode('utf-8'))
print('ok %d -> %d bytes' % (before, len(s)))
