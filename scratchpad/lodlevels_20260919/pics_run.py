#!/usr/bin/env python3
"""LODLEVELS pictures -- the 16 frames and the 4 contact sheets.

Two cameras (sunsim1's 'east' and 'full', eye/target/fov copied verbatim), four
object-LOD levels (4/8/16/32 = STAT MNAM slots 0/1/2/3), two sets:

  A "grey"      -- sun-shaded grey objects over the terrain.
  B "identity"  -- the SAME geometry, every PLACEMENT (REFR) flat-filled with
                   its own colour, terrain plain dark grey.

The camera never moves between levels.  That is the whole point: what changes
between the four pictures is only what the object-LOD data contains.

SIMPLIFICATIONS, stated so nobody has to guess:
  * no cast shadows.  sunsim1's sheared-sun suffix-maximum is not built here;
    every surface gets lit = 1.0 and only N.L + ambient models it.  This lane
    is about WHAT IS THERE, not about light transport.
  * no textures, no vertex colour, no alpha -- one grey albedo for objects,
    one warmer grey for terrain, exactly as sunsim1's shade.py has it.
  * the terrain ray march is run ONCE per camera and reused by all eight of
    that camera's frames (the terrain is identical at every object LOD level).
"""
import os
import sys
import time

import numpy as np
from PIL import Image, ImageDraw, ImageFont

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lodlevels_20260919'
IMG = LANE + '/images'
sys.path.insert(0, LANE)

import pics_build as B                                      # noqa: E402
import pics_shade as SH                                     # noqa: E402
from pics_cams import build as build_cams                   # noqa: E402
from pics_render import march_terrain, raster_objects       # noqa: E402
from pics_scene import Terrain                              # noqa: E402

W, H = 1600, 900
AZ, EL = 120.0, 45.0            # sun: east-south-east, 45 deg up
MAXDIST = 140000.0
FUDGE = 1.6

KITRULE = ("kit rule: base MODL's first path component == 'Architecture', OR any path "
		   "component of the MODL path or of this level's LOD path contains 'kit' "
		   "(MetalKit, DecoKit, BldgKit, ...)")

ALB_TER_B = np.array([0.155, 0.155, 0.165])
SKY_B = np.array([0.045, 0.050, 0.062])
HAZE_B = np.array([0.10, 0.11, 0.135])
HAZE_B_D = 170000.0


# ------------------------------------------------------------------ g-buffer
class GB(object):
	"""sunsim1's GBuffer, with the terrain march handed in already solved."""

	def __init__(self, cam, ter, ob, tt, th, d):
		tri, dep, bary, dropped = raster_objects(cam, ob, verbose=True)
		n = cam.h * cam.w
		tri = tri.ravel(); dep = dep.ravel(); bary = bary.reshape(-1, 3)
		objfirst = (tri >= 0) & ((~th) | (dep < tt))
		terfirst = th & (~objfirst)
		self.kind = np.zeros(n, dtype=np.int8)
		self.kind[terfirst] = 1
		self.kind[objfirst] = 2
		self.t = np.where(terfirst, tt, np.where(objfirst, dep, np.inf))
		self.pos = cam.eye[None, :] + d * np.where(np.isfinite(self.t), self.t, 0.0)[:, None]
		self.dir = d
		self.nrm = np.zeros((n, 3)); self.nrm[:, 2] = 1.0
		if terfirst.any():
			self.nrm[terfirst] = ter.normal(self.pos[terfirst, 0], self.pos[terfirst, 1])
		self.tri = tri
		self.bary = bary
		if objfirst.any():
			ia = ob.tri[tri[objfirst]]
			nn = (ob.n[ia[:, 0]] * bary[objfirst, 0:1] + ob.n[ia[:, 1]] * bary[objfirst, 1:2]
				  + ob.n[ia[:, 2]] * bary[objfirst, 2:3])
			ln = np.maximum(np.linalg.norm(nn, axis=1, keepdims=True), 1e-9)
			nn = nn / ln
			flip = np.sum(nn * d[objfirst], axis=1) > 0     # LOD shells are not consistently wound
			nn[flip] *= -1.0
			self.nrm[objfirst] = nn
		self.objfirst = objfirst
		self.terfirst = terfirst


def shade_identity(gb, ob, pal):
	"""Set B: flat per-PLACEMENT colour, slight N.L so edges read."""
	n = len(gb.kind)
	a = np.radians(AZ); e = np.radians(EL)
	sd = np.array([np.sin(a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)])
	ndl = np.clip(gb.nrm @ sd, 0.0, 1.0)
	alb = np.tile(ALB_TER_B, (n, 1))
	oi = gb.objfirst
	if oi.any():
		alb[oi] = pal[ob.inst[gb.tri[oi]]]
	col = alb * (0.52 + 0.62 * ndl)[:, None]
	out = np.where(gb.kind[:, None] == 0, SKY_B[None, :], col)
	t = np.where(np.isfinite(gb.t), gb.t, 0.0)
	f = np.where(gb.kind == 0, 0.0, 1.0 - np.exp(-t / HAZE_B_D))[:, None]
	return out * (1 - f) + HAZE_B[None, :] * f


# ------------------------------------------------------------------ captions
F_TITLE = 'C:/Windows/Fonts/arialbd.ttf'
F_BODY = 'C:/Windows/Fonts/consola.ttf'
PAD, TOP, BOT = 12, 64, 100


def frame(img8, title, lines, path):
	im = Image.new('RGB', (PAD * 2 + W, TOP + H + BOT), (17, 18, 20))
	im.paste(Image.fromarray(img8), (PAD, TOP))
	d = ImageDraw.Draw(im)
	d.text((PAD, 12), title, font=ImageFont.truetype(F_TITLE, 30), fill=(238, 233, 224))
	fc = ImageFont.truetype(F_BODY, 16)
	y = TOP + H + 8
	for ln in lines:
		d.text((PAD, y), ln, font=fc, fill=(198, 202, 210))
		y += 20
	d.rectangle([PAD - 1, TOP - 1, PAD + W, TOP + H], outline=(70, 72, 78))
	im.save(path)
	return im


def sheet(frames, title, sub, path, scale=0.5):
	w = int(frames[0].width * scale)
	h = int(frames[0].height * scale)
	GAP, T = 10, 74
	im = Image.new('RGB', (w * 2 + GAP * 3, T + h * 2 + GAP * 2), (12, 13, 15))
	for i, f in enumerate(frames):
		im.paste(f.resize((w, h), Image.LANCZOS),
				 (GAP + (i % 2) * (w + GAP), T + (i // 2) * (h + GAP)))
	d = ImageDraw.Draw(im)
	d.text((GAP, 12), title, font=ImageFont.truetype(F_TITLE, 32), fill=(240, 235, 226))
	d.text((GAP, 48), sub, font=ImageFont.truetype(F_BODY, 17), fill=(160, 166, 176))
	im.save(path)


# ------------------------------------------------------------------ the run
def main():
	t0 = time.time()
	ter = Terrain()
	cams = build_cams(ter, W, H)
	D = B.load_pickle()
	refs, bases = D['refs'], D['bases']
	print('loaded %d refs / %d bases in %.1fs' % (len(refs), len(bases), time.time() - t0), flush=True)
	table = {}
	for cn in ('east', 'full'):
		cam = cams[cn]
		cache = LANE + '/pics_ter_%s.npz' % cn
		d = cam.rays().reshape(-1, 3)
		if os.path.exists(cache):
			z = np.load(cache)
			tt, th = z['tt'], z['th']
			print('[%s] terrain march from cache' % cn, flush=True)
		else:
			t = time.time()
			tt, th = march_terrain(ter, cam.eye, d, verbose=True)
			np.savez_compressed(cache, tt=tt, th=th)
			print('[%s] terrain march %.1fs' % (cn, time.time() - t), flush=True)
		fa, fb = [], []
		for k, L in enumerate(B.LEVELS):
			t = time.time()
			idx = B.visible_refs(refs, bases, k, cam, maxdist=MAXDIST, fudge=FUDGE)
			ob = B.build_level(refs, bases, k, idx, verbose=False)
			print('[%s] LOD%d: %d placements, %d tris (%.1fs build)'
				  % (cn, L, len(ob.refid), len(ob.tri), time.time() - t), flush=True)
			gb = GB(cam, ter, ob, tt, th, d)
			seen = np.unique(ob.inst[gb.tri[gb.objfirst]]) if gb.objfirst.any() else np.zeros(0, int)
			nvis = int(len(seen))
			nviskit = int(ob.kit[seen].sum()) if nvis else 0
			nkit = int(ob.kit.sum())
			table[(cn, L)] = dict(placed=int(len(ob.refid)), tris=int(len(ob.tri)),
								  kit=nkit, visible=nvis, viskit=nviskit,
								  px=float(gb.objfirst.mean()))
			base = 'lod%02d_%s' % (L, cn)
			lines = [
				'level %d  (STAT MNAM slot %d)   placements drawn %s   triangles drawn %s   of those placements, kit pieces %s (%.0f%%)'
				% (L, k, f'{len(ob.refid):,}', f'{len(ob.tri):,}', f'{nkit:,}',
				   100.0 * nkit / max(len(ob.refid), 1)),
				'placements that actually cover a pixel in this frame: %s (%s of them kit pieces); objects fill %.1f%% of the frame'
				% (f'{nvis:,}', f'{nviskit:,}', 100.0 * gb.objfirst.mean()),
				KITRULE,
			]
			# ---- set A, grey
			lit = np.ones(len(gb.kind))
			a8 = SH.to8(SH.shade(gb, lit, EL, AZ), W, H)
			pa = IMG + '/grey_%s.png' % base
			fa.append(frame(a8, 'LOD %d  --  %s camera  --  grey' % (L, cn), lines, pa))
			# ---- set B, identity
			pal = B.hue_colours(max(len(ob.refid), 1))
			b8 = SH.to8(shade_identity(gb, ob, pal), W, H)
			pb = IMG + '/ident_%s.png' % base
			fb.append(frame(b8, 'LOD %d  --  %s camera  --  placement identity' % (L, cn),
							lines + ['every REFR placement gets its own flat colour; terrain is plain dark grey'], pb))
			print('   wrote %s (%d B) and %s (%d B)'
				  % (os.path.basename(pa), os.path.getsize(pa),
					 os.path.basename(pb), os.path.getsize(pb)), flush=True)
		sub = ('same camera in all four panels; sun az %.0f el %.0f; cull = camera frustum x1.6 '
			   'out to %.0f units' % (AZ, EL, MAXDIST))
		sheet(fa, 'Object LOD levels 4 / 8 / 16 / 32  --  %s camera  --  grey' % cn, sub,
			  IMG + '/sheet_grey_%s.png' % cn)
		sheet(fb, 'Object LOD levels 4 / 8 / 16 / 32  --  %s camera  --  placement identity' % cn, sub,
			  IMG + '/sheet_ident_%s.png' % cn)
		print('[%s] sheets written' % cn, flush=True)
	import json
	json.dump({'%s_%d' % k: v for k, v in table.items()},
			  open(LANE + '/pics_table.json', 'w'), indent=1)
	print('done %.1fs' % (time.time() - t0))


if __name__ == '__main__':
	main()
