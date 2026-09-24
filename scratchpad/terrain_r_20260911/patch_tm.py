p = 'tests/spells/lodgen_terrain_model.py'
s = open(p, 'r', encoding='utf-8', newline='').read()

start = s.index('# --------------------------------------------------------------------------\n'
                '# The BGSM slice the mask law needs')
end = s.index('# --------------------------------------------------------------------------\n'
              '# The container:')
s = s[:start] + s[end:]

a = """class Layer(object):
	def __init__(self, esm, data, form):
		self.form = form
		self.rule = 'none-default'
		self.diffuse = None
		self.spec = None
		self.smoothness = 1.0
		self.roughConst = 1.0
		self.metalConst = 0.0
		self.edid = ''
		rec = esm.ltex.get(form)
		if not rec:
			return
		self.edid = rec['edid']
		ts = esm.txst.get(rec['tnam'])
		if not ts:
			return
		dpath = ts['tx00'] or ts['mnam']
		mat = find_material(data, ts['mnam']) if ts['mnam'] else None
		spec = ts['tx07']
		if mat and mat.lower().endswith('.bgsm'):
			pbrm = mat[:-5] + '.pbrm'
			if os.path.isfile(pbrm):
				self.rule = 'pbrm'
				return                       # measured: none in the vanilla corpus
			try:
				m = read_bgsm(mat)
			except Exception:
				m = None
			if m:
				t = m['textures']
				if not ts['tx00'] and t and t[0]:
					dpath = t[0]
		if mat or spec:
			self.rule = 'legacy-inverted'
			self.roughConst = 1.0 - min(max(self.smoothness, 0.0), 1.0)
		d = find_asset(data, dpath)
		self.diffuse = Dds(d) if d else None
		sp = find_asset(data, spec) if spec else None
		self.spec = Dds(sp) if sp else None
"""

b = """class Layer(object):
	\"\"\"One landscape texture, as the model can read it WITHOUT our material
	reader.

	`gateable` is the honest half of this: a TXST that names a `.bgsm` carries
	its diffuse, its specular map AND its smoothness constant inside the
	material, and reading a BGSM independently means re-deriving a binary layout
	that this model has no second source for. Such a layer is therefore NOT
	gated -- it is counted and named. A TXST that names TX00 and TX07 and no
	material has everything in the record, the smoothness constant is the
	shader's own 1.0 on both sides, and those texels are what T1 and T4 are
	measured on.\"\"\"

	def __init__(self, esm, data, form):
		self.form = form
		self.rule = 'none-default'
		self.diffuse = None
		self.spec = None
		self.smoothness = 1.0
		self.roughConst = 1.0
		self.gateable = False
		self.edid = ''
		self.why = 'not an LTEX'
		rec = esm.ltex.get(form)
		if not rec:
			return
		self.edid = rec['edid']
		ts = esm.txst.get(rec['tnam'])
		if not ts:
			self.why = 'the LTEX names no TXST'
			return
		if ts['mnam']:
			self.rule = 'legacy-inverted'
			self.why = 'material-backed (%s): not modelled independently' % ts['mnam']
			return
		if not ts['tx00']:
			self.why = 'the TXST names no diffuse'
			return
		self.rule = 'legacy-inverted' if ts['tx07'] else 'none-default'
		self.roughConst = 1.0 - min(max(self.smoothness, 0.0), 1.0)
		d = find_asset(data, ts['tx00'])
		try:
			self.diffuse = Dds(d) if d else None
		except Exception as ex:
			self.why = 'diffuse unreadable: %s' % ex
			return
		sp = find_asset(data, ts['tx07']) if ts['tx07'] else None
		try:
			self.spec = Dds(sp) if sp else None
		except Exception as ex:
			self.why = 'specular unreadable: %s' % ex
			return
		self.gateable = self.diffuse is not None
		self.why = 'ok'
"""

assert s.count(a) == 1
s = s.replace(a, b)

# the composite must report whether every layer it touched was gateable
a2 = """	base = land['base'][q] or domBase
	forms = [base] + [(lay['ltex'] or domBase) for lay in land['layers'][q]]
	layers_for(esm, data, forms, cache)"""
b2 = """	base = land['base'][q] or domBase
	forms = [base] + [(lay['ltex'] or domBase) for lay in land['layers'][q]]
	layers_for(esm, data, forms, cache)
	# a texel any of whose layers the model cannot read independently is
	# REFUSED, not approximated: an approximation would put the model's own
	# error into a number that is supposed to measure the generator's
	gateable = all(cache[f].gateable for f in forms if f)"""
assert s.count(a2) == 1
s = s.replace(a2, b2)

a3 = """	return {'colour': col, 'rough': rough, 'q': q, 'base': base,
			'layerCount': len(land['layers'][q])}"""
b3 = """	return {'colour': col, 'rough': rough, 'q': q, 'base': base,
			'gateable': gateable, 'layerCount': len(land['layers'][q])}"""
assert s.count(a3) == 1
s = s.replace(a3, b3)

# T1 only on single-layer, gateable texels with a real specular map
a4 = """		if c['layerCount'] == 0:
			singles.append((i, j, wx, wy, c))"""
b4 = """		if c['layerCount'] == 0 and c['gateable'] and cache[c['base']].spec:
			singles.append((i, j, wx, wy, c))"""
assert s.count(a4) == 1
s = s.replace(a4, b4)

# ring0: skip un-modelled texels and report the coverage
a5 = """			c = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt, True)
			if c is None:
				continue
			f = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt, False)"""
b5 = """			c = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt, True)
			if c is None:
				continue
			total += 1
			if not c['gateable']:
				skipped += 1
				continue
			f = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt, False)"""
assert s.count(a5) == 1
s = s.replace(a5, b5)

a6 = """	errs, floorErrs = [], []
	n = 0
	for j in range(border, side - border, 5):"""
b6 = """	errs, floorErrs = [], []
	n = 0
	total = 0
	skipped = 0
	for j in range(border, side - border, 5):"""
assert s.count(a6) == 1
s = s.replace(a6, b6)

a7 = """	errs.sort()
	mean = sum(errs) / len(errs)
	fmean = sum(floorErrs) / len(floorErrs)
	print('RING0 tile %d,%d texels %d meanErr %.2f p95 %d maxErr %d floorMean %.2f'
		  % (tx, ty, n, mean, errs[int(n * 0.95)], errs[-1], fmean))"""
b7 = """	errs.sort()
	mean = sum(errs) / len(errs)
	fmean = sum(floorErrs) / len(floorErrs)
	say('%d of %d sampled texels modelled; %d refused (a material-backed layer '
		'this reader does not open)' % (n, total, skipped))
	print('RING0 tile %d,%d texels %d meanErr %.2f p95 %d maxErr %d floorMean %.2f'
		  % (tx, ty, n, mean, errs[int(n * 0.95)], errs[-1], fmean))"""
assert s.count(a7) == 1
s = s.replace(a7, b7)

# the tint: the model does NOT fold the grass tint in, so say so where it counts
a8 = """	if cmd == 'mask':"""
b8 = """	say('NOTE: this model does not fold in the grass tint (1.5). Run the bake '
		'with --grass-tint 0 for the ring-0 gate, or the tint\\'s own mix lands '
		'in the error.')
	if cmd == 'mask':"""
assert s.count(a8) == 1
s = s.replace(a8, b8)

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
