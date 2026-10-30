# lane FOGHEIGHT2 (copy of FOGHEIGHT1's notes/fogheight1/gate/mk.py, cut part only): cut the air's medium + ground-layer
# code by text out of a cellvolair.cpp into cut.inc.  usage: python -I mk.py <cellvolair.cpp> <out cut.inc>
import sys
src = open(sys.argv[1], 'rb').read().decode('utf-8')
a = src.index('struct VaMedium\n') if 'struct VaMedium\n' in src else src.index('struct VaMedium\r\n')
a = src.rindex('/*', 0, a)  # keep the comment above it
b = src.index('}\t// namespace', a)
open(sys.argv[2], 'w', encoding='utf-8', newline='\n').write(src[a:b].replace('\r\n', '\n'))
print('cut', src[a:b].count('\n'), 'lines')
