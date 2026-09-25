# CARDFIX1 step 6: the ring card array's FILE NAME (fix24). LF-only.
P = 'E:/Projects/NifskopeWWE-cardfix1/docs/LODGEN_LODM_FORMAT.md'
b = open(P, 'rb').read(); assert b.count(b'\r') == 0
s = b.decode('utf-8')
old = 'groups only with ring sets of its own sheet size (the group key gains `|ring`).\n'
assert s.count(old) == 1
s = s.replace(old, 'groups only with ring sets of its own sheet size (the group key gains `|ring`,\n'
                   'and the file name gains `.ring` after its `WxH`:\n'
                   '`<ws>.LodgenCards.legacy.2304x256.ring_d.DDS`; until 2026-09-25 the name took the\n'
                   'key\'s `|` and the array could not be written at all).\n')
out = s.encode('utf-8'); assert out.count(b'\r') == 0
open(P, 'wb').write(out); print('patched')
