import re
import sys

BS = chr(92)
d = open(sys.argv[1], 'rb').read()
for m in re.finditer(b'[\x20-\x7e]{6,}', d[:8000]):
    s = m.group().decode()
    if 'DDS' in s.upper() or 'errain' in s or 'andscape' in s or BS in s:
        print(m.start(), repr(s))
