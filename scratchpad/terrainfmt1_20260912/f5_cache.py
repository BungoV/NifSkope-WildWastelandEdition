"""F5 (a) -- bungo's cleaned 2K `_msn` cache, measured before anything ships.

Read-only on E:/Tools/Upscale/esrgan-bat/output.

WHAT THE CHANNELS ACTUALLY ARE was measured first, because the bat's
description and the handoff note disagreed and neither matched the bytes.  The
cross-correlation of every cache channel against every vanilla channel, on the
box-downsampled 2K sheet, says:

    cache R  <->  vanilla R (east)
    cache G  <->  vanilla B (north)
    cache B  ==   0 on every texel

so UP is the channel that is gone and must be recomputed, and the cache is a
two-channel tangential normal, not vanilla's layout with one channel cleaned.

Everything below is measured on that mapping.  Every claim carries a floor:
  * correlation with vanilla -> a phase-randomised twin of vanilla's own sheet;
  * "the BC1 block grid is gone" -> the SAME statistic on a plain 4x box
    upscale of vanilla, which cleans nothing, so a number that only says
    "upscaling smears a grid" cannot pass as "his chain removed it".
"""
import json
import os
import sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dds_np import Dds                                            # noqa: E402

CACHE = r'E:/Tools/Upscale/esrgan-bat/output'
VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
OUT = os.path.join(HERE, 'f5_cache.json')
NSAMPLE = int(os.environ.get('NSAMPLE', '24'))
R = np.random.RandomState(20260912)


def grid_line(img, period):
	"""Block-grid residue at `period`: mean |neighbour difference| on the lines
	where index % period == 0, over the mean on every other line, x and y
	averaged.  1.0 = no grid."""
	a = img.astype(np.float64)
	if a.ndim == 2:
		a = a[:, :, None]
	dx = np.abs(a[:, 1:] - a[:, :-1]).mean(axis=(0, 2))
	xs = np.arange(1, a.shape[1])
	dy = np.abs(a[1:, :] - a[:-1, :]).mean(axis=(1, 2))
	ys = np.arange(1, a.shape[0])
	return float(0.5 * (dx[xs % period == 0].mean() / dx[xs % period != 0].mean()
						+ dy[ys % period == 0].mean() / dy[ys % period != 0].mean()))


def box4(a):
	h, w = a.shape[:2]
	if a.ndim == 2:
		return a.reshape(h // 4, 4, w // 4, 4).mean(axis=(1, 3))
	return a.reshape(h // 4, 4, w // 4, 4, -1).mean(axis=(1, 3))


def up4(a):
	"""Plain 4x BICUBIC upscale -- the control that invents no detail and cleans
	nothing on purpose.  A nearest/kron upscale was tried first and is useless as
	a control: it manufactures a hard step at every 4th texel, so it reads a
	block grid of 5.49 where there is none.  Bicubic is what an upscaler that is
	NOT a model would do."""
	im = Image.fromarray(a.astype(np.uint8))
	return np.asarray(im.resize((a.shape[1] * 4, a.shape[0] * 4), Image.BICUBIC),
					  np.float64)


def phase_twin(a, rs):
	F = np.fft.rfft2(a.astype(np.float64))
	ph = rs.uniform(-np.pi, np.pi, F.shape)
	return np.fft.irfft2(np.abs(F) * np.exp(1j * ph), a.shape)


def pearson(x, y):
	x = x.ravel().astype(np.float64)
	y = y.ravel().astype(np.float64)
	x = x - x.mean()
	y = y - y.mean()
	d = np.sqrt((x * x).sum() * (y * y).sum())
	return float((x * y).sum() / d) if d > 0 else 0.0


def hp(a, k):
	"""RMS of what a k-box blur removes: energy finer than k texels."""
	a = a.astype(np.float64)
	if a.ndim == 2:
		a = a[:, :, None]
	ker = np.ones(k) / k
	s = np.apply_along_axis(lambda v: np.convolve(v, ker, 'same'), 0, a)
	s = np.apply_along_axis(lambda v: np.convolve(v, ker, 'same'), 1, s)
	m = k  # ignore the border the convolution's zero padding spoils
	return float(np.sqrt(((a[m:-m, m:-m] - s[m:-m, m:-m]) ** 2).mean()))


# ---------------------------------------------------------------- inventory
names = sorted(n for n in os.listdir(CACHE) if n.lower().endswith('.png'))
sizes = {}
for n in names[:40]:
	with Image.open(os.path.join(CACHE, n)) as im:
		sizes[(im.size, im.mode)] = sizes.get((im.size, im.mode), 0) + 1
vanmsn = set(n for n in os.listdir(VAN)
			 if n.lower().endswith('_msn.dds') and n.split('.')[1] == '4')
covered = sum(1 for n in names if n[:-4] + '_msn.DDS' in vanmsn)
tot = sum(os.path.getsize(os.path.join(CACHE, n)) for n in names)
print('cache: %d PNG, first 40 sizes/modes %s' % (len(names), sizes))
print('cache names with a shipped dim-4 vanilla _msn: %d of %d '
	  '(vanilla dim-4 _msn on disk: %d)' % (covered, len(names), len(vanmsn)))
print('cache on disk: %.2f GB as PNG' % (tot / 2 ** 30))

smp = [names[i] for i in R.choice(len(names), NSAMPLE, replace=False)]
if 'Commonwealth.4.-20.24.png' not in smp:
	smp[0] = 'Commonwealth.4.-20.24.png'
rows = []
for n in smp:
	vp = os.path.join(VAN, n[:-4] + '_msn.DDS')
	if not os.path.exists(vp):
		continue
	with Image.open(os.path.join(CACHE, n)) as im:
		c = np.asarray(im.convert('RGB'), np.uint8)
	v = Dds(vp).rgb(0)
	rs = np.random.RandomState(abs(hash(n)) & 0xFFFF)
	r = {'file': n}

	# ---- channel identity, as a matrix, on the downsampled cache --------
	cd = box4(c)
	r['xcorr'] = [[pearson(cd[:, :, i], v[:, :, j]) for j in range(3)]
				  for i in range(3)]
	r['cache_stats'] = [[float(c[:, :, k].min()), float(c[:, :, k].mean()),
						 float(c[:, :, k].max())] for k in range(3)]
	r['van_stats'] = [[float(v[:, :, k].min()), float(v[:, :, k].mean()),
					   float(v[:, :, k].max())] for k in range(3)]
	r['blue_all_zero'] = bool((c[:, :, 2] == 0).all())

	# ---- can UP be recomputed?  e^2 + n^2 <= 1 ---------------------------
	e = c[:, :, 0].astype(np.float64) / 255.0 * 2.0 - 1.0     # east
	nn = c[:, :, 1].astype(np.float64) / 255.0 * 2.0 - 1.0    # north
	s = e * e + nn * nn
	r['frac_over_unit'] = float((s > 1.0).mean())
	r['max_tangential_len'] = float(np.sqrt(s.max()))
	up = np.sqrt(np.clip(1.0 - s, 0.0, 1.0))
	r['up_mean_level'] = float(((up * 0.5 + 0.5) * 255.0).mean())
	r['van_up_mean_level'] = float(v[:, :, 1].mean())
	# unit length of the RENORMALISED triple, in levels of 255
	L = np.sqrt(e * e + nn * nn + up * up)
	r['len_after_renorm_mean'] = float(L.mean())
	r['len_after_renorm_maxdev_levels'] = float(np.abs(L - 1.0).max() * 127.5)

	# ---- coarse agreement vs vanilla, against the phase twin -------------
	pairs = (('east', 0, 0), ('north', 1, 2))
	r['coarse'] = {}
	for nm, ci, vi in pairs:
		tw = phase_twin(v[:, :, vi], rs)
		r['coarse'][nm] = {'r': pearson(cd[:, :, ci], v[:, :, vi]),
						   'twin': pearson(cd[:, :, ci], tw)}
	tw_up = phase_twin(v[:, :, 1], rs)
	r['coarse']['up'] = {'r': pearson(box4(up), v[:, :, 1].astype(np.float64)),
						 'twin': pearson(box4(up), tw_up)}

	# ---- fine energy ------------------------------------------------------
	# at the 512 sheet's own 4-texel band: cache downsampled vs vanilla vs twin
	twE = phase_twin(v[:, :, 0], rs)
	r['fine512_cache'] = hp(cd[:, :, 0], 4)
	r['fine512_van'] = hp(v[:, :, 0], 4)
	r['fine512_twin'] = hp(twE, 4)
	# below vanilla's resolution: the 2K sheet's own 4-texel band, cache vs the
	# plain 4x upscale of vanilla (which has nothing there by construction)
	r['fine2k_cache'] = hp(c[:, :, 0], 4)
	r['fine2k_upscale'] = hp(up4(v[:, :, 0]), 4)

	# ---- the BC1 4x4 block grid ------------------------------------------
	r['grid_van_p4'] = grid_line(v[:, :, 0], 4)
	r['grid_cache_p16'] = grid_line(c[:, :, 0], 16)
	r['grid_upscale_p16'] = grid_line(up4(v[:, :, 0]), 16)
	r['grid_cache_p4'] = grid_line(c[:, :, 0], 4)
	rows.append(r)
	print('%-26s blue0=%-5s over-unit %.5f  coarse east %.3f/%.3f north '
		  '%.3f/%.3f  grid van@4 %.4f cache@16 %.4f upscale@16 %.4f'
		  % (n, r['blue_all_zero'], r['frac_over_unit'],
			 r['coarse']['east']['r'], r['coarse']['east']['twin'],
			 r['coarse']['north']['r'], r['coarse']['north']['twin'],
			 r['grid_van_p4'], r['grid_cache_p16'], r['grid_upscale_p16']))


def med(k):
	return float(np.median([r[k] for r in rows]))


summ = {
	'n': len(rows),
	'blue_all_zero_count': sum(1 for r in rows if r['blue_all_zero']),
	'xcorr_median': [[float(np.median([r['xcorr'][i][j] for r in rows]))
					  for j in range(3)] for i in range(3)],
	'frac_over_unit': med('frac_over_unit'),
	'max_tangential_len': float(np.max([r['max_tangential_len'] for r in rows])),
	'up_mean_level': med('up_mean_level'),
	'van_up_mean_level': med('van_up_mean_level'),
	'len_after_renorm_mean': med('len_after_renorm_mean'),
	'len_after_renorm_maxdev_levels': float(
		np.max([r['len_after_renorm_maxdev_levels'] for r in rows])),
	'coarse': {nm: {'r': float(np.median([r['coarse'][nm]['r'] for r in rows])),
					'twin': float(np.median([r['coarse'][nm]['twin'] for r in rows])),
					'beats_twin': sum(1 for r in rows
									  if abs(r['coarse'][nm]['r'])
									  > abs(r['coarse'][nm]['twin']))}
			   for nm in ('east', 'north', 'up')},
	'fine512_cache': med('fine512_cache'),
	'fine512_van': med('fine512_van'),
	'fine512_twin': med('fine512_twin'),
	'fine2k_cache': med('fine2k_cache'),
	'fine2k_upscale': med('fine2k_upscale'),
	'grid_van_p4': med('grid_van_p4'),
	'grid_cache_p16': med('grid_cache_p16'),
	'grid_upscale_p16': med('grid_upscale_p16'),
	'grid_cache_p4': med('grid_cache_p4'),
	'inventory': {'count': len(names), 'png_bytes': tot,
				  'with_vanilla_dim4_msn': covered,
				  'vanilla_dim4_msn': len(vanmsn),
				  'sizes': {str(k): v for k, v in sizes.items()}},
}
print('\n--- summary over %d sampled chunks ---' % len(rows))
print(json.dumps(summ, indent=1))
json.dump({'summary': summ, 'rows': rows}, open(OUT, 'w'), indent=1)
print('wrote %s' % OUT)
