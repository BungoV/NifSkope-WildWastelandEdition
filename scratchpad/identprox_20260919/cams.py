#!/usr/bin/env python3
"""SUNSIM1 -- the cameras.  Heights are read off the terrain, never typed."""
import numpy as np
from render import Camera
from scene import CHUNK

X0, Y0, X1, Y1 = CHUNK


def build(ter, w=1600, h=900):
	def g(x, y):
		return float(ter.atf(np.array([x]), np.array([y]))[0])

	cams = {}
	# Chosen off the 640x360 contact sheet images/_cand_sheet.png (five candidates
	# A..E).  Every eye/target Z below is an OFFSET above the terrain read with
	# g(), never a typed absolute height.
	#
	# (1) "close" = candidate A: low oblique from the SOUTH-WEST across the
	#     built-up part, eye at about a far rooftop's height, and INSIDE the
	#     chunk so the baked sheet has something to say about the ground.
	e = (18200.0, -48000.0)
	t = (26800.0, -41000.0)
	cams['close'] = Camera((e[0], e[1], g(*e) + 3400.0), (t[0], t[1], g(*t) + 100.0),
						   58.0, w, h, name='close')
	# (2) "full" = candidate C: higher oblique of the whole chunk, same bearing.
	#     It deliberately overshoots the chunk edge, so the RIGHT panel carries
	#     the no-data tint out there.
	e = (16600.0, -48800.0)
	t = (27500.0, -39000.0)
	cams['full'] = Camera((e[0], e[1], g(*e) + 6000.0), (t[0], t[1], g(*t) + 100.0),
						  56.0, w, h, name='full')
	# (3) "street" = candidate J, chosen after the first full-size pass rejected
	#     candidate D.  D's bearing was azimuth 120 -- exactly the azimuth of
	#     four of the five suns -- so at elevation 5 and 15 the whole frame was
	#     a contre-jour silhouette and no cast shadow was legible.  J stands at
	#     the same near-ground height but looks along azimuth 289, i.e. 11 deg
	#     off the ANTI-sun direction, so the long shadows run down the street
	#     toward the lens across lit ground and the faces are lit.  (For the
	#     fifth sun, azimuth 240, this camera is the partly back-lit one.)
	e = (27200.0, -40600.0)
	t = (20800.0, -38400.0)
	cams['street'] = Camera((e[0], e[1], g(*e) + 120.0), (t[0], t[1], g(*t) + 250.0),
							62.0, w, h, name='street')
	# (4) "east", added after the first full-size pass: FOUR of the five sun
	#     positions stand at azimuth 120 (east-south-east), and a camera in the
	#     south-west is then looking at the shaded backs of everything -- the
	#     shadows are there but they fall on surfaces the lens cannot see.  This
	#     one stands in the east looking west-north-west, so those four suns
	#     light the faces in frame and the cast shadows lie across open ground.
	e = (30500.0, -45500.0)
	t = (22500.0, -39500.0)
	cams['east'] = Camera((e[0], e[1], g(*e) + 1600.0), (t[0], t[1], g(*t) + 200.0),
						  58.0, w, h, name='east')
	return cams


def topdown(w=2048, h=2048, crop=False):
	"""North-up orthographic-looking pair, kept as the secondary picture: a very
	long lens high above the centre is an orthographic view to within a
	fraction of a texel, and it goes through the same ray path as the rest."""
	if crop:
		cx, cy, half = 23500.0, -39600.0, 3600.0
	else:
		cx, cy, half = (X0 + X1) / 2, (Y0 + Y1) / 2, 8300.0
	H = 1.0e6                      # a million units up
	fov = 2.0 * np.degrees(np.arctan(half / H))
	return Camera((cx, cy, H), (cx, cy, 0.0), fov, w, h, up=(0.0, 1.0, 0.0),
				  name='top_crop' if crop else 'top_full')
