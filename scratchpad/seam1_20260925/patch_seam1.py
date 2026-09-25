"""SEAM1 root fix: a quadrant with no BTXT and a NULL-LTEX layer paint the ENGINE's default land texture
(one world-wide set), not the enclosing chunk's dominant base. Each anchor asserted exactly once."""
import re
R = 'E:/Projects/NifskopeWWE-seam1/'
def patch(path, pairs):
    b = open(R + path, 'rb').read().decode('utf-8'); cr = b.count('\r')
    for old, new in pairs:
        n = b.count(old); assert n == 1, (path, n, old[:80])
        b = b.replace(old, new)
    assert b.count('\r') == cr
    open(R + path, 'wb').write(b.encode('utf-8'))
    print('patched', path)

H_OLD = "struct EsmLtexTextureSet\n{"
H_NEW = """/*! THE ENGINE'S DEFAULT LAND TEXTURE, as a form (lane SEAM1, 2026-09-25).
 *
 *  A LAND quadrant with no BTXT, and an ATXT layer whose LTEX is 0, are painted
 *  by the game with ONE texture set for the whole world: the INI settings
 *  `[Landscape] sDefaultLandDiffuseTexture` / `sDefaultLandNormalTexture` /
 *  `sDefaultLandSpecularTexture`, whose defaults sit beside their names in the
 *  exe's string table as `Ground\CommonwealthDefault01_d.dds`, `_n`, `_s`, and
 *  are resolved under `Landscape\` (the format string that follows them). It
 *  is not an LTEX, so it names no grass.
 *
 *  Until SEAM1 the bake painted those texels with the enclosing dim-4 chunk's
 *  DOMINANT base instead, a stand-in that changes from chunk to chunk: at
 *  Sanctuary, chunk (-20,20) came out LRiverbedSilt01 and its neighbours
 *  LRubbleRock01 / LRootsEroded01, a hard-edged block of 10-13 levels on the
 *  chunk grid that Bethesda's own LOD of the same cells does not have.
 *
 *  0xFFFFFFFF is never a record in a plugin (load-order byte 0xFF is the
 *  runtime's own), so ltexTextureSet() and ltexCover() answer it without a
 *  record lookup and every other caller is unchanged. */
constexpr quint32 ESM_LTEX_ENGINE_DEFAULT = 0xFFFFFFFFu;

struct EsmLtexTextureSet
{"""
patch('src/esmdata.h', [(H_OLD, H_NEW)])

C1_OLD = """	EsmLtexTextureSet s;
	const ESMFile::ESMRecord * lr = esm->findRecord( ltexForm );
	if ( lr && *lr == "LTEX" ) {"""
C1_NEW = """	EsmLtexTextureSet s;
	if ( ltexForm == ESM_LTEX_ENGINE_DEFAULT ) {
		// the engine's own fallback set (esmdata.h), paths as a TXST spells them
		s.diffuse = QStringLiteral( "Landscape\\Ground\\CommonwealthDefault01_d.dds" );
		s.normal = QStringLiteral( "Landscape\\Ground\\CommonwealthDefault01_n.dds" );
		s.specular = QStringLiteral( "Landscape\\Ground\\CommonwealthDefault01_s.dds" );
		s.exists = true;
		return *ltexCache.insert( ltexForm, s );
	}
	const ESMFile::ESMRecord * lr = esm->findRecord( ltexForm );
	if ( lr && *lr == "LTEX" ) {"""
C2_OLD = """	EsmLtexCover c;
	c.resolved = true;
	const ESMFile::ESMRecord * lr = esm->findRecord( ltexForm );"""
C2_NEW = """	EsmLtexCover c;
	c.resolved = true;
	if ( ltexForm == ESM_LTEX_ENGINE_DEFAULT ) {
		// the engine's default ground is a texture, not an LTEX: it exists and
		// grows nothing (D = 0, no tint)
		c.exists = true;
		return *ltexCoverCache.insert( ltexForm, c );
	}
	const ESMFile::ESMRecord * lr = esm->findRecord( ltexForm );"""
patch('src/esmdata.cpp', [(C1_OLD, C1_NEW), (C2_OLD, C2_NEW)])

L1_OLD = """	// quadrants painted with no BTXT fall back to the chunk's dominant base
	quint32 dominantBase = 0;
	{
		QMap<quint32, int> counts;
		for ( const EsmLand & land : cells )
			for ( int q = 0; q < 4; q++ )
				if ( land.baseTex[q] )
					counts[land.baseTex[q]]++;
		int best = 0;
		for ( auto it = counts.constBegin(); it != counts.constEnd(); ++it )
			if ( it.value() > best ) { best = it.value(); dominantBase = it.key(); }
	}
"""
L1_NEW = """	/* Quadrants painted with no BTXT, and NULL-LTEX layers, paint the ENGINE's
	 * default land texture -- one set for the whole world (esmdata.h,
	 * ESM_LTEX_ENGINE_DEFAULT; lane SEAM1). It used to be the chunk's dominant
	 * base, which changes from chunk to chunk and drew the chunk grid into the
	 * ground (Sanctuary, chunk (-20,20), 10-13 levels). The name stays so every
	 * site below reads as it did. */
	const quint32 dominantBase = ESM_LTEX_ENGINE_DEFAULT;
"""
L2_OLD = """	quint32 dominantBase = 0;
	{
		const int bx = lodgenVtFloorTo( cellX0, 4 ), by = lodgenVtFloorTo( cellY0, 4 );
		QMap<quint32, int> counts;
		for ( int y = 0; y < 4; y++ ) {
			for ( int x = 0; x < 4; x++ ) {
				const EsmLand * l = landCache.get( world, bx + x, by + y );
				if ( !l )
					continue;
				for ( int q = 0; q < 4; q++ )
					if ( l->baseTex[q] )
						counts[l->baseTex[q]]++;
			}
		}
		int best = 0;
		for ( auto it = counts.constBegin(); it != counts.constEnd(); ++it )
			if ( it.value() > best ) { best = it.value(); dominantBase = it.key(); }
	}
"""
L2_NEW = """	/* The engine's default land texture, world-wide (lane SEAM1; see the chunk
	 * baker's twin of this line). No longer scoped to the enclosing dim-4 chunk:
	 * the old per-chunk dominant base is what put the chunk grid into the VT. */
	const quint32 dominantBase = ESM_LTEX_ENGINE_DEFAULT;
"""
L3_OLD = """						// NULL-texture layers paint the engine's hardcoded
						// default ground; the chunk's dominant base is the
						// local stand-in
"""
L3_NEW = """						// NULL-texture layers paint the engine's default
						// ground (ESM_LTEX_ENGINE_DEFAULT, lane SEAM1)
"""
L4_OLD = """ *  `dominantBase` is computed over the ENCLOSING dim-4 chunk's cells, not over
 *  the tile's own: it is what NULL-LTEX layers and baseTex == 0 texels paint,
 *  so a tile scoped to its own two cells would paint them a different colour
 *  and the assembled chunk sheet would stop matching a direct bake. */"""
L4_NEW = """ *  `dominantBase` is what NULL-LTEX layers and baseTex == 0 texels paint. It
 *  is the engine's world-wide default land texture (ESM_LTEX_ENGINE_DEFAULT,
 *  lane SEAM1), so a tile, a chunk and the assembled sheet all agree on it
 *  with no scope to get wrong. It was the enclosing dim-4 chunk's dominant
 *  base until 2026-09-25. */"""
patch('src/lodgen.cpp', [(L1_OLD, L1_NEW), (L2_OLD, L2_NEW), (L3_OLD, L3_NEW), (L4_OLD, L4_NEW)])
