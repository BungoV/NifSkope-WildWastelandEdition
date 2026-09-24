import io
import ast

p = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_lodl_pyramid.py'
s = io.open(p, encoding='utf-8').read()
bad1 = "    ck.check('B every sampled height is inside its own cell's stored range '"
good1 = "    ck.check('B every sampled height is inside the stored range of its own cell '"
bad2 = "    ck.check('C every water cell's stored table is inside that cell's own height range '"
good2 = "    ck.check('C every water table is inside the stored height range of its own cell '"
for b, g in ((bad1, good1), (bad2, good2)):
    assert s.count(b) == 1, b
    s = s.replace(b, g)
io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
ast.parse(s)
print('repaired and parses')
