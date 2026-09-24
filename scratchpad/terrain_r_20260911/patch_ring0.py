p = 'tests/spells/lodgen_terrain_model.py'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:70], s.count(a))
    s = s.replace(a, b)


# a base-only blend: the FLOOR that must bite whatever the data looks like
rep("""def composite(esm, data, cache, cx0, cy0, dim, domBase, wx, wy, upt, withVclr=True,
			  invertRough=True):""",
    """def composite(esm, data, cache, cx0, cy0, dim, domBase, wx, wy, upt, withVclr=True,
			  invertRough=True, withLayers=True):""")

rep("""	for lay in land['layers'][q]:
		a = bilinear(lay['op'], qx, qy)""",
    """	for lay in (land['layers'][q] if withLayers else []):
		a = bilinear(lay['op'], qx, qy)""")

rep("""			f = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt, False)
			p = px[j * side + i]
			d = max(abs(round(c['colour'][k] * 255) - p[k]) for k in range(3))
			errs.append(d)
			floorErrs.append(max(abs(round(f['colour'][k] * 255) - p[k]) for k in range(3)))
			n += 1""",
    """			f = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt, False)
			g = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt,
						  True, True, False)
			p = px[j * side + i]
			d = max(abs(round(c['colour'][k] * 255) - p[k]) for k in range(3))
			errs.append(d)
			floorErrs.append(max(abs(round(f['colour'][k] * 255) - p[k]) for k in range(3)))
			baseErrs.append(max(abs(round(g['colour'][k] * 255) - p[k]) for k in range(3)))
			n += 1""")

rep("""	errs, floorErrs = [], []
	n = 0
	total = 0
	skipped = 0""",
    """	errs, floorErrs, baseErrs = [], [], []
	n = 0
	total = 0
	skipped = 0""")

rep("""	if fmean > mean * 1.5:
		ok('T4 FLOOR an un-graded blend (no VCLR) is refused',
		   'floor mean %.2f vs %.2f' % (fmean, mean))
	else:
		bad('T4 FLOOR an un-graded blend (no VCLR) reads worse than the graded one',
			'floor mean %.2f vs %.2f -- this tile carries little VCLR' % (fmean, mean))
	# THE CEILING: the bake against itself, which must be exactly 0
	ceil = max(abs(px[k][0] - px[k][0]) for k in range(0, len(px), 997))
	if ceil == 0:
		ok('T4 CEILING the bake compared against itself reads exactly 0')
	else:
		bad('T4 CEILING the bake compared against itself reads 0')""",
    """	# THE FLOOR THAT BITES: drop the per-texel LTEX WEIGHTS and paint the base
	# alone. The gate's whole claim is that the runtime's weights blend lands on
	# the pyramid's colour, so a blend WITHOUT the weights must miss it.
	bmean = sum(baseErrs) / len(baseErrs)
	if bmean > mean * 3.0:
		ok('T4 FLOOR a blend that ignores the per-texel LTEX weights is refused',
		   'base-only mean %.2f vs %.2f, a %.1fx separation'
		   % (bmean, mean, bmean / max(mean, 0.01)))
	else:
		bad('T4 FLOOR a blend that ignores the per-texel LTEX weights reads far worse',
			'base-only mean %.2f vs %.2f' % (bmean, mean))
	# The VCLR arm, REPORTED and not gated, with the reason measured rather than
	# assumed: only 2,362 of the Commonwealth's 36,864 cells carry a VCLR at all,
	# and in this region every one of them is within 249..255 of white -- so
	# omitting the multiply is not a wrong grading here, it is no change.
	say('T4 the same texels with the VCLR multiply omitted: mean %.2f (reported, '
		'not gated -- this region\\'s VCLR is 249..255 of white)' % fmean)
	# THE CEILING: the same comparison, through the same decode path, against the
	# bake itself. A ceiling that cannot read non-zero would be decoration, so it
	# re-opens the file and re-decodes rather than differencing one array.
	v2 = Lodt(lodt)
	px2, _ = v2.sheet(idx, 1, 0)
	ceil = max(max(abs(px2[k][c] - px[k][c]) for c in range(3))
			   for k in range(len(px)))
	if ceil == 0:
		ok('T4 CEILING the bake re-decoded and compared against itself reads exactly 0',
		   'over all %d texels of the sheet' % len(px))
	else:
		bad('T4 CEILING the bake re-decoded and compared against itself reads 0',
			'max %d' % ceil)""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
