#!/usr/bin/env python
"""Lane FLAT2 step 1: how many .lodt tile sheets are one value?

For every present tile of every level container given, and for every sheet of
that tile, decode every mip and classify the sheet:
  a  = bit-exactly uniform after decode (every texel of every mip, RGBA, the same)
  b  = not a, but every channel of every texel of every mip within 1 level of
       one value (max - min <= 1 per channel)
The R16 height sheet has no codec, so for it only a is meaningful.

Cheap path: mip 1 is decoded first; a sheet whose mip 1 already spans more
than one level cannot be a or b, and mip 0 is never read.

Usage: measure.py OUT.npz file.lodt [file.lodt ...]
Prints one table row per (file, sheet) and a total. Per-tile classes are
saved to OUT.npz for the map picture (key <edid>.<dim>.<sheet> -> int8 array
tilesY x tilesX: -1 absent, 0 not uniform, 1 case b, 2 case a).
"""
import sys
import time
import numpy as np
import vtread

ROLE = {1: 'colour', 2: 'msn', 4: 'height', 5: 'mask', 6: 'emissive', 7: 'horizon'}


def bc1_alpha(blk):
	"""BC1 punch-through alpha: 0 where c0 <= c1 and the index is 3, else 255."""
	by, bx = blk.shape[:2]
	c0 = blk[..., 0].astype(np.uint16) | (blk[..., 1].astype(np.uint16) << 8)
	c1 = blk[..., 2].astype(np.uint16) | (blk[..., 3].astype(np.uint16) << 8)
	bits = (blk[..., 4].astype(np.uint32) | (blk[..., 5].astype(np.uint32) << 8)
			| (blk[..., 6].astype(np.uint32) << 16) | (blk[..., 7].astype(np.uint32) << 24))
	idx = np.stack([(bits >> (2 * i)) & 3 for i in range(16)], -1)
	a = np.where(((c0 <= c1)[..., None]) & (idx == 3), 0, 255).astype(np.uint8)
	return a.reshape(by, bx, 4, 4).transpose(0, 2, 1, 3).reshape(by * 4, bx * 4)


def decode_rgba(L, index, sheet, mip):
	blk, fmt = L.blocks(index, sheet, mip)
	if L.sheets[sheet]['role'] == 4:
		return blk[..., None].astype(np.int32)
	if fmt in (77, 78):
		col = vtread.decode_bc1_blocks(blk[..., 8:16], four=True)
		alp = vtread.decode_bc3_alpha(blk[..., 0:8])
	else:
		col = vtread.decode_bc1_blocks(blk, four=False)
		alp = bc1_alpha(blk)
	return np.concatenate([col, alp[..., None]], -1).astype(np.int32)


def classify(L, index, sheet):
	lo = None
	hi = None
	for mip in range(L.mips - 1, -1, -1):  # coarsest first: the cheap refusal
		px = decode_rgba(L, index, sheet, mip)
		px = px.reshape(-1, px.shape[-1])
		mn = px.min(0)
		mx = px.max(0)
		lo = mn if lo is None else np.minimum(lo, mn)
		hi = mx if hi is None else np.maximum(hi, mx)
		if int((hi - lo).max()) > 1:
			return 0
	if int((hi - lo).max()) == 0:
		return 2
	return 1


def sheet_bytes(L, sheet, cover):
	return sum(L.sheetMipBytes(sheet, m, cover) for m in range(L.mips))


def main():
	out = sys.argv[1]
	maps = {}
	grand = {}
	total_bytes = 0
	for path in sys.argv[2:]:
		t0 = time.time()
		L = vtread.Lodt(path)
		import os
		total_bytes += os.path.getsize(path)
		flags = L.table['flags'].astype(np.int32)
		for s in range(L.sheetCount):
			role = L.sheets[s]['role']
			cls = np.full(L.tileCount, -1, np.int8)
			na = nb = 0
			ba = bb = 0
			bsheet = 0
			for i in range(L.tileCount):
				f = int(flags[i])
				if not (f & 1):
					continue
				cover = bool(f & 2)
				sb = sheet_bytes(L, s, cover)
				bsheet += sb
				c = classify(L, i, s)
				cls[i] = c
				if c == 2:
					na += 1
					ba += sb
				elif c == 1:
					nb += 1
					bb += sb
			maps['%s.%d.%s' % (L.edid, L.levelDim, ROLE.get(role, str(role)))] = cls.reshape(L.tilesY, L.tilesX)
			present = int((flags & 1).sum())
			print('%-14s dim %2d %-7s present %5d  a %5d (%12d B)  b-only %5d (%12d B)  sheet bytes %13d' % (
				L.edid, L.levelDim, ROLE.get(role, role), present, na, ba, nb, bb, bsheet), flush=True)
			g = grand.setdefault((L.edid, ROLE.get(role, role)), [0, 0, 0, 0, 0, 0])
			g[0] += present; g[1] += na; g[2] += ba; g[3] += nb; g[4] += bb; g[5] += bsheet
		print('  (%s: %.1f s)' % (path.split('/')[-1].split(chr(92))[-1], time.time() - t0), flush=True)
	print('TOTALS per worldspace x sheet kind')
	sa = sb_ = 0
	for k in sorted(grand):
		g = grand[k]
		print('%-14s %-7s sheets %6d  a %6d  %13d B   b-only %6d  %13d B   of %14d B' % (k[0], k[1], g[0], g[1], g[2], g[3], g[4], g[5]))
		sa += g[2]; sb_ += g[4]
	print('files %d B; case a saves %d B (%.3f %%); case a+b saves %d B (%.3f %%)' % (
		total_bytes, sa, 100.0 * sa / max(1, total_bytes), sa + sb_, 100.0 * (sa + sb_) / max(1, total_bytes)))
	np.savez_compressed(out, **maps)


if __name__ == '__main__':
	main()
