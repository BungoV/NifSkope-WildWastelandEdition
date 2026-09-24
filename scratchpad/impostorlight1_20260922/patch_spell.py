p = 'tests/spells/impostor_draw.sh'
s = open(p, encoding='utf-8').read()
a = '\tlight_run nrm WW_IMPOSTOR_MESH_CHANNEL=8 WW_IMPOSTOR_CHANNEL=13\n'
b = a + '\tlight_run alb WW_IMPOSTOR_MESH_CHANNEL=12 WW_IMPOSTOR_CHANNEL=1\n'
assert s.count(a) == 1
s = s.replace(a, b)
a = '"$( cygpath -m "$tmp/light/nrm" )" "$nbake" > "$lchk" 2>&1'
b = '"$( cygpath -m "$tmp/light/nrm" )" "$nbake" "$( cygpath -m "$tmp/light/alb" )" > "$lchk" 2>&1'
assert s.count(a) == 1
s = s.replace(a, b)
a = '#     card through debug channel 13, the normal its light is dotted with. The\n'
b = ('#     card through debug channel 13, the normal its light is dotted with --\n'
     '#     and the ALBEDO pair (mesh LOD channel 12, card debug channel 1, both\n'
     '#     unlit), the colour-sheet rung 16c\'s transfer bar is relative to. The\n')
assert s.count(a) == 1
s = s.replace(a, b)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('patched')
