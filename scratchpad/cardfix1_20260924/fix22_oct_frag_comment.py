# CARDFIX1 step 6: the drawer's sway comment named only the synthetic law. Comment only.
P = 'E:/Projects/NifskopeWWE-cardfix1/res/shaders/impostor_oct.frag'
b = open(P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')
nl = '\r\n' if cr else '\n'
old = ' * The sway weight is h^2 x (0.35 + 0.65 r) per TEXEL, and the law it feeds is' + nl
assert s.count(old) == 1
new = (' * The sway weight is per TEXEL: W x h on a model with a tree-animation shape' + nl
       + ' * (its own vertex-alpha wind weight, `lodm` 2, sway A), else the synthetic' + nl
       + ' * h^2 x (0.35 + 0.65 r). The law it feeds is' + nl)
s = s.replace(old, new)
out = s.encode('utf-8')
assert out.count(b'\r') == cr + (2 if cr else 0)
open(P, 'wb').write(out)
print('patched', P, 'CR', cr)
