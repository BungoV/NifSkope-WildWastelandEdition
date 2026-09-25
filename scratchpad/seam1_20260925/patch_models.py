R = 'E:/Projects/NifskopeWWE-seam1/tests/spells/'
def patch(path, pairs):
    b = open(R + path, 'rb').read().decode('utf-8'); cr = b.count('\r')
    for old, new in pairs:
        n = b.count(old); assert n == 1, (path, n, old[:80])
        b = b.replace(old, new)
    assert b.count('\r') == cr
    open(R + path, 'wb').write(b.encode('utf-8')); print('patched', path)
patch('lodgen_cover_model.py', [
("def ltex_cover(esm, form):\n",
 """# THE ENGINE'S DEFAULT LAND TEXTURE (lane SEAM1, 2026-09-25): what a BTXT-less
# quadrant and a NULL-LTEX layer paint, one set world-wide -- the INI
# [Landscape] sDefaultLandDiffuseTexture, default Ground\\CommonwealthDefault01_d.dds
# under Landscape\\. Mirrors ESM_LTEX_ENGINE_DEFAULT in src/esmdata.h. It is not an
# LTEX and grows nothing. The per-chunk dominant base it replaces is kept below
# only so a report can name what the old law would have painted.
ENGINE_DEFAULT = 0xFFFFFFFF
ENGINE_DEFAULT_TEX = ('Landscape/Ground/CommonwealthDefault01_d.dds',
					  'Landscape/Ground/CommonwealthDefault01_n.dds',
					  'Landscape/Ground/CommonwealthDefault01_s.dds')


def ltex_cover(esm, form):
"""),
("""	grass mesh and its texture, which this reader deliberately does not open.\"\"\"
	rec = esm.ltex.get(form)""",
 """	grass mesh and its texture, which this reader deliberately does not open.\"\"\"
	if form == ENGINE_DEFAULT:
		return (0.0, 0.0)
	rec = esm.ltex.get(form)"""),
("	dom = dominant_base(esm, cx0, cy0, dim)\n	dtex =",
 "	dom = ENGINE_DEFAULT\n	dtex ="),
])
patch('lodgen_terrain_model.py', [
("from lodgen_cover_model import Esm, bilinear, dominant_base     # noqa: E402",
 "from lodgen_cover_model import Esm, bilinear, dominant_base, ENGINE_DEFAULT, ENGINE_DEFAULT_TEX     # noqa: E402"),
("""		self.why = 'not an LTEX'
		rec = esm.ltex.get(form)""",
 """		self.why = 'not an LTEX'
		if form == ENGINE_DEFAULT:
			# the engine's default ground (lane SEAM1): TX00 + TX07, no material
			self.edid = 'ENGINE_DEFAULT'
			self.rule = 'legacy-inverted'
			d = find_asset(data, ENGINE_DEFAULT_TEX[0])
			sp = find_asset(data, ENGINE_DEFAULT_TEX[2])
			self.diffuse = Dds(d) if d else None
			self.spec = Dds(sp) if sp else None
			self.roughConst = 0.0
			self.gateable = self.diffuse is not None
			self.why = 'ok' if self.gateable else 'engine default diffuse not found'
			return
		rec = esm.ltex.get(form)"""),
])
b = open(R + 'lodgen_terrain_model.py').read()
n = b.count("domBase = dominant_base(e, (cx0 // 4) * 4, (cy0 // 4) * 4, 4)"); assert n == 2, n
b = b.replace("domBase = dominant_base(e, (cx0 // 4) * 4, (cy0 // 4) * 4, 4)", "domBase = ENGINE_DEFAULT   # lane SEAM1: was the dim-4 chunk's dominant base")
open(R + 'lodgen_terrain_model.py', 'w', newline='\n').write(b); print('domBase x2')
