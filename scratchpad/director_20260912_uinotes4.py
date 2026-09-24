p = 'HANDOFF.md'
s = open(p, 'rb').read().decode('utf-8')
assert s.count(chr(13)) == 0
a1 = '  three rulings collected BEFORE any animation-workspace work, in his'
a2 = '  of the track (his case: COM of Running_To_Slide keeps Z, loses X/Y).'
a3 = '  BUILDING; found an hour late (director\'s mistake: no liveness check;\n  the resume lane records it in MISTAKES_ENTRIES.md). Its work is on'
for a in (a1, a2, a3):
    assert s.count(a) == 1, a
s = s.replace(a1, '  four rulings collected BEFORE any animation-workspace work, in his')
s = s.replace(a2, a2 + '\n  4 the transport-bar icons redrawn (Blender\'s transport, our SVG in the\n  wwskin palette, tooltips with shortcuts; "Load..." / "root" header too).')
s = s.replace(a3, '  BUILDING; found an hour late (director\'s mistake: no liveness check;\n  ALREADY in root MISTAKES.md 01:4x -- drop the lane\'s duplicate at\n  splice time). Its work is on')
open(p, 'wb').write(s.encode('utf-8'))
print('ok', s.count('four rulings collected'))
