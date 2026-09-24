# fix04_drawgate.py -- lane IMPOSTORDEPTH2: impostor_draw.sh rows 17 and 18 measured the
# 3-frame stippled cut, which WAS the default. The default is now the slider's crisp end
# (snap, one frame), so those rows name the drawer they measure: the smooth end.
P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_draw.sh'
b = open(P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')
E = [
    ('\torbit_grab "$tmp/tear/blend" "$tear_views" WW_IMPOSTOR_BLEND=1\n',
     '\torbit_grab "$tmp/tear/blend" "$tear_views" WW_IMPOSTOR_BLEND=1 WW_IMPOSTOR_SLIDER=1\n'),
    ('\t\torbit_grab "$tmp/pop/$sw/default" "$vv"\n'
     '\t\torbit_grab "$tmp/pop/$sw/mean"    "$vv" WW_IMPOSTOR_CUT=mean\n'
     '\t\torbit_grab "$tmp/pop/$sw/strong"  "$vv" WW_IMPOSTOR_CUT=strong\n',
     '\t\torbit_grab "$tmp/pop/$sw/default" "$vv" WW_IMPOSTOR_SLIDER=1\n'
     '\t\torbit_grab "$tmp/pop/$sw/mean"    "$vv" WW_IMPOSTOR_SLIDER=1 WW_IMPOSTOR_CUT=mean\n'
     '\t\torbit_grab "$tmp/pop/$sw/strong"  "$vv" WW_IMPOSTOR_SLIDER=1 WW_IMPOSTOR_CUT=strong\n'),
    ('on the rung by construction of that clause\n'
     '#     and on nothing else the lane measured.\n',
     'on the rung by construction of that clause\n'
     '#     and on nothing else the lane measured.\n'
     '#\n'
     '#     RE-PINNED 2026-09-23 (lane IMPOSTORDEPTH2): the stippled blend this row\n'
     '#     measures is no longer the default -- the default is the slider\'s crisp\n'
     '#     end, one snapped frame, which cannot tear this way. The blended run\n'
     '#     now names the drawer: WW_IMPOSTOR_SLIDER=1, the smooth end (stipple +\n'
     '#     depth search 16 + the decoded coverage filter). The bar did not move.\n'),
    ('#     cannot run the red control -- an exe that cannot demonstrate its bar can\n'
     '#     fire does not pass it.\n',
     '#     cannot run the red control -- an exe that cannot demonstrate its bar can\n'
     '#     fire does not pass it.\n'
     '#\n'
     '#     RE-PINNED 2026-09-23 (lane IMPOSTORDEPTH2): all three runs are the\n'
     '#     smooth end (WW_IMPOSTOR_SLIDER=1), the stippled cut\'s home now that the\n'
     '#     default is the crisp end (snap, which jumps by bungo\'s ruling and is\n'
     '#     gated in impostor_trunk.sh, not here). The mean and strong cuts run with\n'
     '#     the same search, so the comparison is still one cut against another on\n'
     '#     this exe and this bake. The bar did not move.\n'),
]
for old, new in E:
    c = s.count(old)
    assert c == 1, (old[:70], c)
    s = s.replace(old, new)
out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(P, 'wb').write(out)
print('patched', P)
