import io
import sys

p = r'E:\Projects\NifskopeWildWastelandEdition\src\nifcli.cpp'
s = io.open(p, encoding='utf-8', newline='').read()
old = '\t\t  << "                                          --road-opacity A (default 1)\\n"\n'
if s.count(old) != 1:
    sys.exit('REFUSED: found %d' % s.count(old))
new = '\t\t  << "  lodgen ... [--road-opacity 0..1]\\n"\n' + old
io.open(p, 'w', encoding='utf-8', newline='').write(s.replace(old, new))
print('synopsis line added')
