"""F1 -- vanilla's far-terrain sheet format law, measured over the corpus.

Measures, before any code is written:
  (a) fourCC and mip count over EVERY shipped Commonwealth terrain sheet;
  (b) the ALPHA channel's content over a sample, colour and _msn separately:
      min/max/mean per file, and a pooled histogram;
  (c) a cross-check of this lane's numpy reader against the tree's own
      pure-Python reader on one file of each family.
"""
import json
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from dds_np import Dds, header                                    # noqa: E402

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
OUT = os.path.join(HERE, 'f1_corpus.json')

names = sorted(n for n in os.listdir(VAN) if n.lower().endswith('.dds'))
print('files in %s: %d' % (VAN, len(names)))

fam = {}
for n in names:
	h = header(os.path.join(VAN, n))
	stem = n[:-4]
	kind = '_msn' if stem.lower().endswith('_msn') else 'colour'
	dim = stem.split('.')[1] if '.' in stem else '?'
	key = (kind, h['fourcc'].decode('latin1'), h['mips'], h['width'], h['height'])
	fam.setdefault(key, []).append(n)
	fam.setdefault(('ANYDIM', kind, dim), []).append(n)

print('\n--- (a) format law over the whole folder ---')
law = []
for k in sorted(fam, key=lambda t: str(t)):
	if k[0] == 'ANYDIM':
		continue
	law.append({'kind': k[0], 'fourcc': k[1], 'mips': k[2], 'w': k[3], 'h': k[4],
				'count': len(fam[k])})
	print('%-7s fourCC=%s mips=%2d %4dx%-4d : %5d files'
		  % (k[0], k[1], k[2], k[3], k[4], len(fam[k])))
dims = {}
for k in sorted(fam):
	if k[0] == 'ANYDIM':
		dims.setdefault(k[1], {})[k[2]] = len(fam[k])
print('by dim:', json.dumps(dims, sort_keys=True))

# also: is the declared mip count the count actually present in the bytes?
short = []
for n in names[:200]:
	d = Dds(os.path.join(VAN, n))
	if d.mips != d.declaredMips:
		short.append((n, d.declaredMips, d.mips))
print('declared-vs-present mip mismatch in first 200 files: %d' % len(short))

# reserved1 non-zero anywhere?
nz = [n for n in names if any(header(os.path.join(VAN, n))['reserved1'])]
print('files with a non-zero dwReserved1: %d' % len(nz))

print('\n--- (b) the ALPHA channel, sampled ---')
msn = [n for n in names if n.lower().endswith('_msn.dds') and '.4.' in n]
col = [n for n in names if not n.lower().endswith('_msn.dds') and '.4.' in n]
rng = np.random.RandomState(20260912)
smp_msn = [msn[i] for i in rng.choice(len(msn), 50, replace=False)]
smp_col = [col[i] for i in rng.choice(len(col), 50, replace=False)]

res = {}
for tag, smp in (('_msn', smp_msn), ('colour', smp_col)):
	hist = np.zeros(256, np.int64)
	rows = []
	for n in smp:
		d = Dds(os.path.join(VAN, n))
		a = d.alpha(0)
		hist += np.bincount(a.ravel(), minlength=256)
		rows.append({'file': n, 'min': int(a.min()), 'max': int(a.max()),
					 'mean': float(a.mean()), 'std': float(a.std()),
					 'frac255': float((a == 255).mean())})
	allmin = min(r['min'] for r in rows)
	allmax = max(r['max'] for r in rows)
	f255 = float(np.mean([r['frac255'] for r in rows]))
	print('%-7s over %d files: alpha min %d max %d, mean %.3f, '
		  'fraction of texels == 255: %.6f'
		  % (tag, len(rows), allmin, allmax,
			 float(np.mean([r['mean'] for r in rows])), f255))
	nzh = [(v, int(c)) for v, c in enumerate(hist) if c]
	print('        distinct alpha values present: %d  %s'
		  % (len(nzh), nzh[:8] if len(nzh) <= 8 else str(nzh[:4]) + ' ... ' + str(nzh[-4:])))
	res[tag] = {'rows': rows, 'min': allmin, 'max': allmax, 'frac255': f255,
				'distinct': len(nzh),
				'hist_nonzero': nzh[:40]}

# (c) cross-check against the tree's own reader on one file of each family
print('\n--- (c) cross-check vs tests/spells/lodgen_terrain_model.py ---')
try:
	import importlib.util
	spec = importlib.util.spec_from_file_location(
		'ltm', os.path.join(HERE, '..', '..', 'tests', 'spells',
							'lodgen_terrain_model.py'))
	ltm = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(ltm)
	for n in (smp_msn[0], smp_col[0]):
		p = os.path.join(VAN, n)
		a = Dds(p)
		b = ltm.Dds(p)
		lv = b._level(0)
		px, w, h = (lv if isinstance(lv, tuple) and len(lv) == 3
					else (lv, b.width, b.height))
		mine = a.rgb(0).astype(np.float64) / 255.0
		theirs = np.array([[c for c in q[:3]] for q in px],
						  np.float64).reshape(h, w, 3)
		d = np.abs(mine - theirs) * 255.0
		ma = a.alpha(0).astype(np.float64) / 255.0
		ta = np.array([q[3] for q in px], np.float64).reshape(h, w)
		print('%-40s rgb maxdiff %.2f/255 mean %.4f | alpha maxdiff %.2f/255'
			  % (n, float(d.max()), float(d.mean()),
				 float(np.abs(ma - ta).max() * 255.0)))
except Exception as e:                                            # pragma: no cover
	print('cross-check could not run: %r' % (e,))

json.dump({'law': law, 'dims': dims, 'nonzero_reserved1': len(nz),
		   'declared_vs_present_mismatch': len(short), 'alpha': res,
		   'nfiles': len(names)}, open(OUT, 'w'), indent=1)
print('\nwrote %s' % OUT)
