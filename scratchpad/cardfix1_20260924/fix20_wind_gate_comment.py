P = 'E:/Projects/NifskopeWWE-cardfix1/tests/spells/impostor_wind.sh'
b = open(P, 'rb').read(); assert b.count(b'\r') == 0
s = b.decode('utf-8')
o = "BNS's treeredpinefull01 (ring 16, tile\n#       256, the shipped tree default):\n"
assert s.count(o) == 1
s = s.replace(o, "BNS's treeredpinefull01,\n"
                 "#       baked as 16-view RINGS at tile 256 -- a ring frame is a plain azimuth view at elevation 0, which\n"
                 "#       the independent rasteriser reprojects exactly (the law is the same for an octahedral set; the\n"
                 "#       GIF is taken on an N8 set):\n")
out = s.encode('utf-8'); assert out.count(b'\r') == 0
open(P, 'wb').write(out); print('patched')
