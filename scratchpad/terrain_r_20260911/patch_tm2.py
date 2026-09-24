p = 'tests/spells/lodgen_terrain_model.py'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:70], s.count(a))
    s = s.replace(a, b)


# composite gains the inversion switch, so the FLOOR runs through the SAME blend
rep("def composite(esm, data, cache, cx0, cy0, dim, domBase, wx, wy, upt, withVclr=True):",
    "def composite(esm, data, cache, cx0, cy0, dim, domBase, wx, wy, upt, withVclr=True,\n"
    "\t\t\t  invertRough=True):")

rep("""	col = list(cache[base].sample_colour(wx, wy, upt)) if base else [0.5, 0.5, 0.5, 1.0]
	rough = cache[base].sample_rough(wx, wy, upt) if base else 1.0""",
    """	col = list(cache[base].sample_colour(wx, wy, upt)) if base else [0.5, 0.5, 0.5, 1.0]
	roughOf = (lambda l: l.sample_rough(wx, wy, upt)) if invertRough \\
		else (lambda l: l.sample_gloss_uninverted(wx, wy, upt))
	rough = roughOf(cache[base]) if base else 1.0""")

rep("""		rough += (cache[f].sample_rough(wx, wy, upt) - rough) * a""",
    """		rough += (roughOf(cache[f]) - rough) * a""")

# T1 over every gateable texel, blended, with the un-inverted blend as the floor
rep("""	singles = []
	for (i, j, wx, wy) in samples:
		c = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt)
		if c is None:
			continue
		if c['layerCount'] == 0 and c['gateable'] and cache[c['base']].spec:
			singles.append((i, j, wx, wy, c))""",
    """	singles = []
	total = 0
	specSeen = []
	for (i, j, wx, wy) in samples:
		c = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt)
		if c is None:
			continue
		total += 1
		if not c['gateable']:
			continue
		f = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt,
					  True, False)
		singles.append((i, j, wx, wy, c, f))
		lay = cache[c['base']]
		if lay.spec and len(specSeen) < 400:
			specSeen.append(round(lay._tap(lay.spec, wx, wy, upt)[1] * 255))
	say('%d of %d sampled texels modelled; %d refused (a material-backed layer '
		'this reader does not open)' % (len(singles), total, total - len(singles)))""")

rep("""	specG = []
	for (i, j, wx, wy, c) in singles[:40]:
		lay = cache[c['base']]
		if lay.spec:
			specG.append(round(lay._tap(lay.spec, wx, wy, upt)[0] * 255))
	if specG and len(set(specG)) > 1:
		ok('T2 FLOOR the legacy specular is NOT constant, so a G of 0 is a decision',
		   '%d distinct specular R values over %d single-layer samples'
		   % (len(set(specG)), len(specG)))
	else:
		bad('T2 FLOOR the legacy specular varies on this tile, so constant 0 means something',
			'specular R values: %s' % sorted(set(specG))[:8])""",
    """	if specSeen and len(set(specSeen)) > 1:
		ok('T2 FLOOR the legacy gloss channel is NOT constant here, so a metallic '
		   'of 0 is a decision and not an empty sheet',
		   '%d distinct gloss values over %d modelled samples'
		   % (len(set(specSeen)), len(specSeen)))
	else:
		bad('T2 FLOOR the legacy gloss varies on this tile',
			'gloss values: %s' % sorted(set(specSeen))[:8])""")

rep("""	errs, floorErrs = [], []
	for (i, j, wx, wy, c) in singles:
		want = c['rough']
		floor = cache[c['base']].sample_gloss_uninverted(wx, wy, upt)
		gotv = px[j * side + i][0] / 255.0
		errs.append(abs(gotv - want) * 255.0)
		floorErrs.append(abs(gotv - floor) * 255.0)""",
    """	errs, floorErrs = [], []
	for (i, j, wx, wy, c, f) in singles:
		gotv = px[j * side + i][0] / 255.0
		errs.append(abs(gotv - c['rough']) * 255.0)
		floorErrs.append(abs(gotv - f['rough']) * 255.0)""")

rep("""	say('T1 %d single-layer texels; model vs sheet: mean %.2f, p95 %.2f, max %.2f (8-bit)'
		% (len(errs), mean, p95, errs[-1]))""",
    """	say('T1 %d modelled texels; model vs sheet: mean %.2f, p95 %.2f, max %.2f (8-bit)'
		% (len(errs), mean, p95, errs[-1]))""")

rep("""	if not singles:
		bad('T1 the tile has texels painted by exactly one landscape texture')
		return""",
    """	if not singles:
		bad('T1 the tile has texels this model can read independently')
		return""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
