"""Lane CELLVIEW4B, fix 01 -- four repairs to CELLVIEW4's never-compiled code,
each one measured before it was written.  Every anchor is asserted to match
exactly once and the CR count is asserted unchanged (all three files are
LF-only).  Nothing here is a toggle.

1. THE ROOT GOES BACK TO NiNode AND THE GROUND GETS ITS OWN BSOrderedNode.
   CELLVIEW4 made the scene ROOT a BSOrderedNode so the splat passes composite
   in paint order.  src/gl/glnode.cpp:166 `orderedNodeSort()` sets
   `presorted = true` on every child and `Node::drawShapes` recurses, so a
   BSOrderedNode ROOT marks EVERY node in the scene presorted -- and
   `compareNodesAlpha` then sorts the WHOLE transparent pass by block number
   instead of depth.  That is CELLVIEW4's own stated refuter.
   `compareNodesAlpha` only takes the block-order branch when BOTH nodes are
   presorted, so a BSOrderedNode holding only the land shapes gives the ground
   its paint order and leaves every other pair on exactly the depth rule it had
   before.  The A/B that measures it is in this lane's report.

2. THE REFUSAL MUST SURVIVE THE FALLBACK.  The blend writes its refusal into
   `groundNote` and then the mosaic fallback overwrites `groundNote` with its
   own legend, so past the 12M cap the census said nothing about a refusal at
   all.  CONSTITUTION 10: a fallback is never a silent downgrade and a refusal
   states its reason in words.  The note is now carried and prefixed.

3. THE VERTEX BUDGET IS PRINTED WHETHER OR NOT IT REFUSES.  `splatVerts` was
   computed and then thrown away on the success path, so the number that
   decides the refusal was unreadable in the one case anyone looks at.

4. THE BARE QUADS ARE SPLIT BY REASON.  `cellBuildSplat` already has the two
   reasons in hand -- no pass at all, versus every pass naming an LTEX that
   resolved to no texture -- and threw the distinction away into one counter.
   tests/spells/cell_pick.sh row 7 needs exactly that split, and a counter
   whose two halves cannot be told apart cannot fail on a broken reader.
"""
import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CHECK = '--check' in sys.argv


def edit(rel, anchor, new, mode='replace'):
    path = os.path.join(ROOT, rel)
    with io.open(path, 'rb') as fh:
        raw = fh.read()
    cr_before = raw.count(b'\r')
    txt = raw.decode('utf-8')
    n = txt.count(anchor)
    print('%-22s %-8s %d  %r' % (rel, mode, n, anchor[:56]))
    assert n == 1, 'anchor matched %d times, not once: %r' % (n, anchor[:80])
    if mode == 'replace':
        out = txt.replace(anchor, new)
    elif mode == 'after':
        out = txt.replace(anchor, anchor + new)
    else:
        raise ValueError(mode)
    if CHECK:
        return
    data = out.encode('utf-8')
    assert data.count(b'\r') == cr_before, 'CR count moved: %d -> %d' % (
        cr_before, data.count(b'\r'))
    with io.open(path, 'wb') as fh:
        fh.write(data)


# ---------------------------------------------------------------- 1. the root
edit('src/cellview.cpp', '''	/* BSOrderedNode, not NiNode (lane CELLVIEW4): it marks every child
	 * presorted, and compareNodesAlpha then sorts the transparent pass
	 * by BLOCK NUMBER instead of depth -- so the splat layers composite
	 * in the order cellsplat.cpp emitted their buckets, which is the
	 * ATXT paint order. It inherits NiNode and is valid for FO4
	 * (build/nif.xml, BSOrderedNode, FO4 default flags 0x200E).
	 * THE REFUTER: this changes the sort for EVERY transparent shape in
	 * the scene, not just the ground. If tree billboards or glass come
	 * back sorting worse in the downtown picture, the ground needs its
	 * own BSOrderedNode child instead of the scene root. */
	QModelIndex iRoot = nif->insertNiBlock( QStringLiteral( "BSOrderedNode" ) );''',
'''	/* AN ORDINARY NiNode ROOT (lane CELLVIEW4B, and this is CELLVIEW4's own
	 * refuter being acted on rather than argued with).
	 *
	 * CELLVIEW4 made the ROOT a BSOrderedNode so the splat passes composite in
	 * paint order. That works, and it also does something nobody asked for:
	 * src/gl/glnode.cpp NodeList::orderedNodeSort() sets `presorted = true` on
	 * every child, and Node::drawShapes() recurses, so a BSOrderedNode root
	 * marks EVERY node in the scene presorted -- after which compareNodesAlpha
	 * sorts the ENTIRE transparent pass by block number instead of by depth.
	 * Every tree card, every pane of car glass, every alpha-tested railing in
	 * a downtown cell changes its draw order, to pay for a guarantee only the
	 * ground needed.
	 *
	 * compareNodesAlpha takes the block-order branch only when BOTH nodes are
	 * presorted; any pair with one ordinary node falls through to the same
	 * alpha-then-depth rule it used before. So a BSOrderedNode that holds ONLY
	 * the land shapes buys the ground its paint order and costs the rest of
	 * the scene nothing -- see `iGround` below. */
	QModelIndex iRoot = nif->insertNiBlock( QStringLiteral( "NiNode" ) );''')

# ------------------------------------------------- the ground's ordered parent
edit('src/cellview.cpp', '''	// ---- emit
	qint64 shapes = 0, verts = 0, tris = 0;
	bool ok = true;
	QStringList keys = buckets.keys();
	std::sort( keys.begin(), keys.end() );
	for ( const QString & k : keys ) {
		if ( !emitBucket( nif, iRoot, buckets.value( k ), origin, shapes, verts, tris, error ) ) {
			ok = false;
			break;
		}
	}''',
'''	// ---- emit
	qint64 shapes = 0, verts = 0, tris = 0;
	bool ok = true;
	QStringList keys = buckets.keys();
	std::sort( keys.begin(), keys.end() );
	/* THE GROUND'S OWN ORDERED NODE (lane CELLVIEW4B). Created lazily, and
	 * only when there is a land bucket to put in it, so a cell with the
	 * terrain off produces the identical document it produced before.
	 * It must be inserted BEFORE the first land shape, because the sort it
	 * enables is by BLOCK NUMBER: the land buckets are keyed "\\x01land%03d"
	 * in bucket order, the keys are sorted here, so the shapes are created in
	 * paint order and their block numbers ascend in paint order with them. */
	QPersistentModelIndex iGround;
	for ( const QString & k : keys ) {
		QModelIndex iParent = iRoot;
		if ( k.startsWith( QLatin1String( "\\x01land" ) ) ) {
			if ( !iGround.isValid() ) {
				QModelIndex iG = nif->insertNiBlock( QStringLiteral( "BSOrderedNode" ) );
				nif->set<QString>( iG, "Name", QStringLiteral( "ground" ) );
				nif->set<quint32>( iG, "Flags", 14 );
				nif->set<float>( iG, "Scale", 1.0f );
				addLink( nif, iRoot, QStringLiteral( "Children" ),
					nif->getBlockNumber( iG ) );
				iGround = iG;
			}
			iParent = iGround;
		}
		if ( !emitBucket( nif, iParent, buckets.value( k ), origin, shapes, verts, tris, error ) ) {
			ok = false;
			break;
		}
	}''')

# ------------------------------------------- 2+3. the refusal and the budget
edit('src/cellview.cpp', '''		if ( spec.terrain ) {
			const qint64 splatVerts = cellSplatCountVerts( world, x0, y0, x1, y1 );''',
'''		QString splatNote;   // carried past the mosaic fallback (lane CELLVIEW4B)
		if ( spec.terrain ) {
			const qint64 splatVerts = cellSplatCountVerts( world, x0, y0, x1, y1 );
			/* THE BUDGET IS PRINTED WHETHER OR NOT IT REFUSES (lane CELLVIEW4B).
			 * The count was computed and then discarded on the success path, so
			 * the one number that decides the refusal was unreadable in the only
			 * case anybody ever looks at. */
			splatNote = QStringLiteral( "; %L1 land vertices counted before "
				"allocating, against the %L2 cap" )
				.arg( splatVerts ).arg( qint64( CELL_MAX_TOTAL_VERTS ) );''')

edit('src/cellview.cpp', '''			} else if ( splatVerts > CELL_MAX_TOTAL_VERTS ) {
				groundNote = QStringLiteral( "ground: the BLEND refused -- %L1 vertices "
					"is past the %L2 cap; the hard-edged mosaic was drawn instead" )
					.arg( splatVerts ).arg( qint64( CELL_MAX_TOTAL_VERTS ) );
			}
		}''',
'''			} else if ( splatVerts > CELL_MAX_TOTAL_VERTS ) {
				/* THE REFUSAL HAS TO SURVIVE THE FALLBACK (lane CELLVIEW4B). It
				 * was written into groundNote here and the mosaic below then
				 * overwrote groundNote with its own legend, so past the cap the
				 * census said nothing about a refusal at all -- a silent
				 * downgrade, which CONSTITUTION 10 forbids by name. It is
				 * carried in splatNote instead and prefixed onto whatever the
				 * fallback says. */
				splatNote = QStringLiteral( "ground: the BLEND REFUSED -- %L1 vertices "
					"is past the %L2 cap; the hard-edged mosaic was drawn instead. " )
					.arg( splatVerts ).arg( qint64( CELL_MAX_TOTAL_VERTS ) );
			}
		}''')

edit('src/cellview.cpp', '''				landsDrawn = sb.cells;
					groundNote = cellSplatLegend( sb );''',
'''				landsDrawn = sb.cells;
					groundNote = cellSplatLegend( sb ) + splatNote;
					splatNote.clear();''')

edit('src/cellview.cpp', '''			landsDrawn = gb.cells;
				groundNote = cellGroundLegend( gb );''',
'''			landsDrawn = gb.cells;
				groundNote = splatNote + cellGroundLegend( gb );''')

# --------------------------------------------------- 4. the bare-quad split
edit('src/cellsplat.h', '''	int quadsBare = 0;              //!< no base and no layer with any weight''',
'''	int quadsBare = 0;              //!< no base and no layer with any weight
	/* THE TWO REASONS A QUAD IS BARE, KEPT APART (lane CELLVIEW4B). The builder
	 * has both in hand and was folding them into one number, and a counter
	 * whose halves cannot be told apart cannot fail on a broken LTEX reader:
	 * a resolver that returned nothing for every texture in the game would
	 * report the same `quadsBare` as a genuinely unpainted cell. */
	int quadsBareUnpainted = 0;     //!< the record paints nothing on this quad
	int quadsBareNoTexture = 0;     //!< every pass named an LTEX that resolved to no texture''')

edit('src/cellsplat.cpp', 'if ( !np ) {\n\t\t\t\t\t\tout.quadsBare++;',
     'if ( !np ) {\n\t\t\t\t\t\tout.quadsBare++;\n\t\t\t\t\t\tout.quadsBareUnpainted++;')

edit('src/cellsplat.cpp', 'if ( !emitted ) {\n\t\t\t\t\t\tout.quadsBare++;',
     'if ( !emitted ) {\n\t\t\t\t\t\tout.quadsBare++;\n\t\t\t\t\t\tout.quadsBareNoTexture++;')

edit('src/cellsplat.cpp', '''if ( b.quadsPromotedBase )''',
'''	s += QString( "; of the bare quads %L1 are unpainted and %L2 chose an LTEX "
		"that named no texture" ).arg( b.quadsBareUnpainted ).arg( b.quadsBareNoTexture );
	if ( b.quadsPromotedBase )''')

# ------------------------------------- the seam claim in the header was false
edit('src/cellsplat.h', ''' * The 17x17 grid is not a texture to be sampled: the cell's 33x33 land vertex
 * grid maps ONE-TO-ONE onto the four 17x17 quadrant grids, with the middle row
 * and column shared.  So every corner of every 128-unit quad already HAS a
 * stored opacity -- no interpolation is invented here, and because neighbours
 * share their corner values there is no seam between quads.  Interpolating
 * that weight across the quad, which the rasteriser does for free, IS the
 * engine's bilinear blend.''',
''' * The 17x17 grid is not a texture to be sampled: the cell's 33x33 land vertex
 * grid maps ONE-TO-ONE onto the four 17x17 quadrant grids, with the middle row
 * and column shared.  So every corner of every 128-unit quad already HAS a
 * stored opacity -- no interpolation is invented here.  Interpolating that
 * weight across the quad, which the rasteriser does for free, IS the engine's
 * bilinear blend.
 *
 * WHAT THAT DOES **NOT** BUY, corrected by lane CELLVIEW4B against the record
 * bytes of Sanctuary -20,7 (scratchpad/cellview4b_20260919/seam_probe.py).
 * This file used to end that paragraph with "because neighbours share their
 * corner values there is no seam between quads".  That is true INSIDE a
 * quadrant and false ACROSS one.  A quadrant's layer list is its own: two
 * quadrants need not carry the same textures at all, and where they do carry
 * the same LTEX the two stored grids are independent numbers that need not
 * agree on the line they share.  Measured on -20,7: across the BL|BR line the
 * same DriedGrass01 layer differs by at most 0.0353, but across the BR|TR line
 * the same texture differs by up to 0.7490.  A discontinuity along a cell's
 * centre lines is therefore something the RECORD can contain, and the viewer
 * neither creates nor can remove it.  The measurement that separates the two:
 * of the 64 quad pairs straddling -20,7's centre lines, 30 change their
 * dominant texture, and all 30 are pairs where one side is a quadrant with no
 * BTXT at all; of the 32 pairs where both sides have a BTXT, ZERO change.  The
 * quadrant arithmetic is clean; the seam is in the data.''')

print()
print('CHECK ONLY, nothing written' if CHECK else 'APPLIED')
