"""Print, per cell row -12..-1, the dim-2 one-value columns east of x=10, to pick a sea-edge box."""
import numpy as np
Z = np.load('work/installed.npz')
h = Z['Commonwealth.2.height']
west, north, dim = -96, 95, 2
for cy in range(-1, -13, -2):
	ty = (north - cy) // dim
	row = ''.join('#' if h[ty, tx] == 2 else '.' for tx in range(len(h[ty])))
	# cells 10..47
	print('%4d %s' % (cy, row[(10 - west) // dim:(48 - west) // dim]))
print('     cells 10..47, one char = 2 cells, # = one-value height')
