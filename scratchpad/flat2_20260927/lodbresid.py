"""Print the .lodb residual runs gates.py's provenance rule leaves (debug aid)."""
import sys
from collections import Counter
import re
import gates

ra, rb_ = sys.argv[1], sys.argv[2]
p = '/mod/FO4CSLOD/Commonwealth/Commonwealth.lodb'
ba, bb = open(ra + p, 'rb').read(), open(rb_ + p, 'rb').read()
na = Counter(gates._norm_run(s, ra) for s in re.findall(rb'[\x20-\x7e]{6,}', ba))
nb = Counter(gates._norm_run(s, rb_) for s in re.findall(rb'[\x20-\x7e]{6,}', bb))
for k in (na - nb):
	print('A:', k[:300])
for k in (nb - na):
	print('B:', k[:300])
