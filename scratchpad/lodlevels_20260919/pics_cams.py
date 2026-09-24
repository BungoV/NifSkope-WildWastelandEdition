#!/usr/bin/env python3
"""LODLEVELS pictures -- the two cameras, copied VERBATIM (eye/target/fov) from
scratchpad/sunsim1_20260919/cams.py so this lane never imports that folder's
.lodo loader.  Both heights are OFFSETS above the terrain read with the field,
exactly as that lane wrote them."""
import numpy as np

from pics_render import Camera


def build(ter, w=1600, h=900):
	def g(x, y):
		return float(ter.atf(np.array([x]), np.array([y]))[0])

	cams = {}
	# "full" = sunsim1 candidate C: higher oblique of the whole chunk.
	e = (16600.0, -48800.0)
	t = (27500.0, -39000.0)
	cams['full'] = Camera((e[0], e[1], g(*e) + 6000.0), (t[0], t[1], g(*t) + 100.0),
						  56.0, w, h, name='full')
	# "east": stands in the east looking west-north-west across the built-up part.
	e = (30500.0, -45500.0)
	t = (22500.0, -39500.0)
	cams['east'] = Camera((e[0], e[1], g(*e) + 1600.0), (t[0], t[1], g(*t) + 200.0),
						  58.0, w, h, name='east')
	return cams
