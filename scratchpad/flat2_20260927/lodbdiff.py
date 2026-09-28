"""Print the printable runs (>= 6 chars) that differ between two .lodb bake records, and the flat_objects
report's first differing line, so the G1 differences can be named word by word."""
import re
import sys

a, b = open(sys.argv[1], 'rb').read(), open(sys.argv[2], 'rb').read()
ra = re.findall(rb'[\x20-\x7e]{6,}', a)
rb_ = re.findall(rb'[\x20-\x7e]{6,}', b)
sa, sb = set(ra), set(rb_)
print('runs A %d B %d; only in A %d, only in B %d' % (len(ra), len(rb_), len(sa - sb), len(sb - sa)))
for s in [x for x in ra if x not in sb][:40]:
	print('A:', s[:200].decode())
for s in [x for x in rb_ if x not in sa][:40]:
	print('B:', s[:200].decode())
