"""List every differing file between two bake trees: size, number of differing bytes, first offset,
and whether the difference vanishes once each tree's own root name (rung/off/on/sea_*) is masked."""
import os
import sys

A, B = sys.argv[1], sys.argv[2]
na, nb = os.path.basename(A.rstrip('/')).encode(), os.path.basename(B.rstrip('/')).encode()
for root, _, files in os.walk(A):
	for f in files:
		pa = os.path.join(root, f)
		rel = os.path.relpath(pa, A)
		if rel == 'bake.log':
			continue
		pb = os.path.join(B, rel)
		if not os.path.exists(pb):
			print('only in A', rel)
			continue
		a, b = open(pa, 'rb').read(), open(pb, 'rb').read()
		if a == b:
			continue
		n = sum(1 for x, y in zip(a, b) if x != y) + abs(len(a) - len(b))
		first = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
		# mask the root names and the exe dir names
		ma = a.replace(b'bakes/' + na, b'bakes/X').replace(b'bakes\\' + na, b'bakes\\X').replace(b'run_rung', b'run_X').replace(b'run_new', b'run_X')
		mb = b.replace(b'bakes/' + nb, b'bakes/X').replace(b'bakes\\' + nb, b'bakes\\X').replace(b'run_rung', b'run_X').replace(b'run_new', b'run_X')
		print('%-70s %10d B  %7d differ  first 0x%x  %s' % (rel, len(a), n, first,
			'same once root/exe names are masked' if ma == mb else 'STILL DIFFERS'))
