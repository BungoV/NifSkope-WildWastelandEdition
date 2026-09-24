#!/usr/bin/env python3
"""LODLEVELS pictures -- the object set for ONE object-LOD level, built from
the STAT MNAM slot meshes themselves (never from a .lodo/.lodi bake).

For level L in (4, 8, 16, 32) -> MNAM slot k in (0, 1, 2, 3):
  every Commonwealth REFR in esm.pkl whose base has a NON-EMPTY slots[k] is
  placed with the repo's own convention (src/lodgen.cpp:3446,
  src/data/niftypes.cpp:215):

      M = Matrix::fromEuler(-rx, -ry, -rz)          (row major)
      world = pos + scale * (M @ local)

  `local` is the shape's vertex carried up its own NiNode chain to the file
  root first (node t/r/s, r row-major, s uniform).

A ref whose base has NOTHING in slot k is simply ABSENT at that level -- that
absence is the picture.
"""
import math
import os
import pickle
import sys

import numpy as np

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lodlevels_20260919'
MESH = 'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/'
sys.path.insert(0, LANE)

from pics_nifread import Nif, NifError            # noqa: E402

LEVELS = (4, 8, 16, 32)


# ------------------------------------------------------------------ transforms
def euler_neg_matrix(rx, ry, rz):
	"""Matrix::fromEuler(-x, -y, -z) -- lifted from
	tests/spells/lodgen_native_decode.py line 1031, row major."""
	x, y, z = -rx, -ry, -rz
	sx, cx = math.sin(x), math.cos(x)
	sy, cy = math.sin(y), math.cos(y)
	sz, cz = math.sin(z), math.cos(z)
	return np.array([
		[cy * cz, -cy * sz, sy],
		[sx * sy * cz + sz * cx, cx * cz - sx * sy * sz, -sx * cy],
		[sx * sz - cx * sy * cz, cx * sy * sz + sx * cz, cx * cy]], dtype=np.float64)


def _chain(nif, block):
	"""(R, t, s) of the local transform chain from the file root down to
	`block`, composed so that  v_root = t + s * (R @ v_local)."""
	out = []
	b = block
	seen = set()
	while b is not None and b not in seen:
		seen.add(b)
		if b in nif.shapes:
			n = nif.shapes[b]
		elif b in nif.nodes:
			n = nif.nodes[b]
		else:
			break
		out.append((np.array(n['r'], dtype=np.float64).reshape(3, 3),
					np.array(n['t'], dtype=np.float64), float(n['s'])))
		b = n['parent']
	R = np.eye(3)
	t = np.zeros(3)
	s = 1.0
	for r_, t_, s_ in out:              # child first -> compose parent LAST
		# parent(child(v)) : child gives  a = t_ + s_*(r_@v)
		# then the accumulated (R,t,s) so far is the CHILD side, so:
		#   new = parent applied to old  ->  R = r_@R ; t = t_ + s_*(r_@t) ; s = s_*s
		t = t_ + s_ * (r_ @ t)
		R = r_ @ R
		s = s_ * s
	return R, t, s


_MESH_CACHE = {}
MISSING = set()
UNREADABLE = {}


def load_mesh(rel):
	"""(verts (N,3), norms (N,3), tris (M,3)) in the NIF's own root space.
	None when the file is absent or the reader refuses it."""
	key = rel.lower()
	if key in _MESH_CACHE:
		return _MESH_CACHE[key]
	p = MESH + rel.replace('\\', '/')
	if not os.path.exists(p):
		MISSING.add(key)
		_MESH_CACHE[key] = None
		return None
	try:
		n = Nif(p)
	except Exception as e:                            # noqa: BLE001
		UNREADABLE[key] = '%s: %s' % (type(e).__name__, e)
		_MESH_CACHE[key] = None
		return None
	V, N, T = [], [], []
	base = 0
	for bi in sorted(n.shapes):
		sh = n.shapes[bi]
		nv = sh['numVerts']
		if nv == 0 or sh['numTris'] == 0:
			continue
		v = np.array(sh['verts'], dtype=np.float64)
		if v.shape != (nv, 3):
			continue
		nr = np.array(sh['norms'], dtype=np.float64) if len(sh['norms']) == nv \
			else np.zeros((nv, 3))
		R, t, s = _chain(n, bi)
		v = t[None, :] + s * (v @ R.T)
		nr = nr @ R.T
		tr = np.array(sh['tris'], dtype=np.int64).reshape(-1, 3)
		V.append(v)
		N.append(nr)
		T.append(tr + base)
		base += nv
	if not V:
		_MESH_CACHE[key] = None
		return None
	v = np.concatenate(V)
	nr = np.concatenate(N)
	tr = np.concatenate(T)
	# face normals where the stored triple is degenerate
	ln = np.linalg.norm(nr, axis=1)
	if (ln < 0.3).any():
		a, b, c = v[tr[:, 0]], v[tr[:, 1]], v[tr[:, 2]]
		fn = np.cross(b - a, c - a)
		fl = np.maximum(np.linalg.norm(fn, axis=1, keepdims=True), 1e-12)
		fn = fn / fl
		bad = ln < 0.3
		for k in range(3):
			idx = tr[:, k]
			m = bad[idx]
			nr[idx[m]] = fn[m]
		ln = np.linalg.norm(nr, axis=1)
	nr = nr / np.maximum(ln, 1e-12)[:, None]
	_MESH_CACHE[key] = (v, nr, tr)
	return _MESH_CACHE[key]


# ------------------------------------------------------------------ kit rule
def is_kit(base, slotpath):
	"""KIT RULE (stated in every caption): the base's near MODL path's FIRST
	component is 'Architecture', OR any path component of the MODL path or of
	this level's slot path contains 'kit' (case-insensitive) -- which catches
	DecoKit, MetalKit, WoodKit, BldgKit, ... ."""
	modl = (base.get('modl') or '').replace('/', '\\')
	parts = [q for q in modl.split('\\') if q]
	if parts and parts[0].lower() == 'architecture':
		return True
	for q in parts:
		if 'kit' in q.lower():
			return True
	for q in (slotpath or '').replace('/', '\\').split('\\'):
		if 'kit' in q.lower():
			return True
	return False


# ------------------------------------------------------------------ the set
class ObjSet(object):
	"""What raster_objects() wants: .v (N,3) float, .n (N,3), .tri (M,3) int,
	plus .inst (M,) placement index and .refid (P,) / .kit (P,) per placement."""
	pass


def visible_refs(refs, bases, k, cam, maxdist=140000.0, fudge=1.6):
	"""Frustum + distance cull on each placement's OBND bounding sphere."""
	sel = []
	for i, r in enumerate(refs):
		b = bases.get(r[1])
		if b is None or not b['slots'][k]:
			continue
		sel.append(i)
	if not sel:
		return np.zeros(0, dtype=np.int64)
	sel = np.array(sel, dtype=np.int64)
	P = np.array([[refs[i][2], refs[i][3], refs[i][4]] for i in sel], dtype=np.float64)
	rad = np.empty(len(sel))
	for j, i in enumerate(sel):
		b = bases[refs[i][1]]
		o = b['obnd'] or (0, 0, 0, 0, 0, 0)
		ex = np.array([o[3] - o[0], o[4] - o[1], o[5] - o[2]], dtype=np.float64)
		ct = np.array([o[3] + o[0], o[4] + o[1], o[5] + o[2]], dtype=np.float64) * 0.5
		rad[j] = (0.5 * np.linalg.norm(ex) + np.linalg.norm(ct) + 64.0) * abs(refs[i][8])
	rad = np.maximum(rad, 64.0)
	d = P - cam.eye[None, :]
	zc = d @ cam.f
	xc = d @ cam.r
	yc = d @ cam.u
	keep = (zc > -rad) & (zc < maxdist + rad)
	zp = np.maximum(zc, 0.0)
	keep &= np.abs(xc) <= zp * cam.tx * fudge + rad + 256.0
	keep &= np.abs(yc) <= zp * cam.ty * fudge + rad + 256.0
	return sel[keep]


def build_level(refs, bases, k, idx, verbose=True):
	"""Place every ref in `idx` at MNAM slot k.  Returns an ObjSet."""
	V, N, T, INST = [], [], [], []
	refid, kit, nplaced = [], [], 0
	base = 0
	for j, i in enumerate(idx):
		r = refs[i]
		b = bases[r[1]]
		m = load_mesh(b['slots'][k])
		if m is None:
			continue
		lv, ln, lt = m
		M = euler_neg_matrix(r[5], r[6], r[7])
		s = float(r[8]) or 1.0
		w = np.array([r[2], r[3], r[4]]) + s * (lv @ M.T)
		wn = ln @ M.T
		V.append(w)
		N.append(wn)
		T.append(lt + base)
		INST.append(np.full(len(lt), nplaced, dtype=np.int32))
		base += len(lv)
		refid.append(r[0])
		kit.append(is_kit(b, b['slots'][k]))
		nplaced += 1
		if verbose and (j + 1) % 5000 == 0:
			print('    ... %d/%d refs, %d verts' % (j + 1, len(idx), base), flush=True)
	ob = ObjSet()
	if not V:
		ob.v = np.zeros((0, 3), np.float32)
		ob.n = np.zeros((0, 3), np.float32)
		ob.tri = np.zeros((0, 3), np.int32)
		ob.inst = np.zeros(0, np.int32)
		ob.refid = np.zeros(0, np.int64)
		ob.kit = np.zeros(0, bool)
		return ob
	ob.v = np.concatenate(V).astype(np.float32)
	ob.n = np.concatenate(N).astype(np.float32)
	ob.tri = np.concatenate(T).astype(np.int32)
	ob.inst = np.concatenate(INST)
	ob.refid = np.array(refid, dtype=np.int64)
	ob.kit = np.array(kit, dtype=bool)
	nn = np.linalg.norm(ob.n, axis=1)
	bad = nn < 0.3
	if bad.any():
		ob.n[bad] = np.array([0.0, 0.0, 1.0], np.float32)
		nn = np.linalg.norm(ob.n, axis=1)
	ob.n = (ob.n / np.maximum(nn, 1e-9)[:, None]).astype(np.float32)
	return ob


def load_pickle():
	return pickle.load(open(LANE + '/esm.pkl', 'rb'))


def hue_colours(n, seed=0.137):
	"""Golden-ratio hue walk -> saturated distinct RGB, one per placement."""
	import colorsys
	g = 0.6180339887498949
	out = np.zeros((n, 3))
	for i in range(n):
		h = (seed + i * g) % 1.0
		s = 0.62 + 0.30 * ((i * 7) % 5) / 4.0
		v = 0.72 + 0.28 * ((i * 11) % 3) / 2.0
		out[i] = colorsys.hsv_to_rgb(h, s, v)
	return out
