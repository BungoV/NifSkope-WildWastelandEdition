"""IMPOSTORSHRUB1: the bake keeps the first NON-EMPTY range of a
BSMeshLODTriShape, not the LOD0 slot. Exact-once anchor, LF-only file,
--check writes nothing."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/nifskope_ui.cpp'
OLD = (
"\t\t\t\t\t\t\tif ( mnif->isNiBlock( iB, \"BSMeshLODTriShape\" ) ) {\n"
"\t\t\t\t\t\t\t\t/* The in-mesh steps: the triangle list is [full][L1][L2]\n"
"\t\t\t\t\t\t\t\t * (the maple: 71 + 23 + 8), the engine draws one range and\n"
"\t\t\t\t\t\t\t\t * the viewer draws every range at its default level. */\n"
"\t\t\t\t\t\t\t\tconst uint l1 = mnif->get<uint>( iB, \"LOD1 Size\" ), l2 = mnif->get<uint>( iB, \"LOD2 Size\" );\n"
"\t\t\t\t\t\t\t\tif ( l1 || l2 ) {\n"
"\t\t\t\t\t\t\t\t\tmnif->set<uint>( iB, \"LOD1 Size\", 0 );\n"
"\t\t\t\t\t\t\t\t\tmnif->set<uint>( iB, \"LOD2 Size\", 0 );\n"
"\t\t\t\t\t\t\t\t\tms << \"ranges \" << name << \" \" << l1 << \"+\" << l2 << \"\\n\";\n"
"\t\t\t\t\t\t\t\t}\n"
"\t\t\t\t\t\t\t}\n")
NEW = (
"\t\t\t\t\t\t\tif ( mnif->isNiBlock( iB, \"BSMeshLODTriShape\" ) ) {\n"
"\t\t\t\t\t\t\t\t/* The in-mesh steps: the triangle list is [full][L1][L2]\n"
"\t\t\t\t\t\t\t\t * (the maple: 71 + 23 + 8), the engine draws one range and\n"
"\t\t\t\t\t\t\t\t * the viewer draws every range at its default level\n"
"\t\t\t\t\t\t\t\t * (BSShape::updateLodLevel: the first LOD0+LOD1+LOD2 triangles).\n"
"\t\t\t\t\t\t\t\t *\n"
"\t\t\t\t\t\t\t\t * THE FIRST NON-EMPTY RANGE, not the LOD0 slot (lane\n"
"\t\t\t\t\t\t\t\t * IMPOSTORSHRUB1, 2026-09-23). Vanilla's shrubs, saplings,\n"
"\t\t\t\t\t\t\t\t * hedges and undergrowth ship the LOD0 slot EMPTY and their\n"
"\t\t\t\t\t\t\t\t * full shape in the next one (Sapling01 0/38/22,\n"
"\t\t\t\t\t\t\t\t * DeadShrub01 0/123/198, TreeElmUndergrowth01 0/18/0,\n"
"\t\t\t\t\t\t\t\t * every shape of ShrubGroupLarge05 0/n/0). Zeroing LOD1 and\n"
"\t\t\t\t\t\t\t\t * LOD2 there left 0 triangles to draw and every one of them\n"
"\t\t\t\t\t\t\t\t * baked an EMPTY card (halfW 1.077, 0 covered texels).\n"
"\t\t\t\t\t\t\t\t * Measured (scratchpad/impostorshrub1_20260923/ranges.py):\n"
"\t\t\t\t\t\t\t\t * where two ranges follow an empty slot they share no vertex\n"
"\t\t\t\t\t\t\t\t * and the first has the larger box and 2.2..2.6x the\n"
"\t\t\t\t\t\t\t\t * triangle area - the full plant and a coarser copy of it,\n"
"\t\t\t\t\t\t\t\t * the same shape as [full][L1] on the maple. So the bake\n"
"\t\t\t\t\t\t\t\t * keeps the first range that has triangles; the list is in\n"
"\t\t\t\t\t\t\t\t * slot order, so with the slot before it empty that range\n"
"\t\t\t\t\t\t\t\t * starts at triangle 0 and drawing `LOD0 Size` triangles\n"
"\t\t\t\t\t\t\t\t * draws exactly it. A model whose LOD0 slot is filled bakes\n"
"\t\t\t\t\t\t\t\t * byte for byte as before; the `rangekept` line is written\n"
"\t\t\t\t\t\t\t\t * only when a later slot served. */\n"
"\t\t\t\t\t\t\t\tconst uint l0 = mnif->get<uint>( iB, \"LOD0 Size\" );\n"
"\t\t\t\t\t\t\t\tconst uint l1 = mnif->get<uint>( iB, \"LOD1 Size\" ), l2 = mnif->get<uint>( iB, \"LOD2 Size\" );\n"
"\t\t\t\t\t\t\t\tif ( l1 || l2 ) {\n"
"\t\t\t\t\t\t\t\t\tconst int kept = l0 ? 0 : ( l1 ? 1 : 2 );\n"
"\t\t\t\t\t\t\t\t\tconst uint keep = kept == 0 ? l0 : ( kept == 1 ? l1 : l2 );\n"
"\t\t\t\t\t\t\t\t\tif ( kept )\n"
"\t\t\t\t\t\t\t\t\t\tmnif->set<uint>( iB, \"LOD0 Size\", keep );\n"
"\t\t\t\t\t\t\t\t\tmnif->set<uint>( iB, \"LOD1 Size\", 0 );\n"
"\t\t\t\t\t\t\t\t\tmnif->set<uint>( iB, \"LOD2 Size\", 0 );\n"
"\t\t\t\t\t\t\t\t\tms << \"ranges \" << name << \" \" << l1 << \"+\" << l2 << \"\\n\";\n"
"\t\t\t\t\t\t\t\t\tif ( kept )\n"
"\t\t\t\t\t\t\t\t\t\tms << \"rangekept \" << name << \" LOD\" << kept << \" \" << keep << \" (LOD0 slot empty)\\n\";\n"
"\t\t\t\t\t\t\t\t}\n"
"\t\t\t\t\t\t\t}\n")
b = open(P, 'rb').read()
assert b.count(b'\r') == 0, 'file is not LF-only'
o, n = OLD.encode(), NEW.encode()
c = b.count(o)
print('anchor count', c)
assert c == 1
if '--check' in sys.argv:
    print('check only'); sys.exit(0)
b2 = b.replace(o, n)
open(P, 'wb').write(b2)
print('written; lines +%d, CR %d' % (b2.count(b'\n') - b.count(b'\n'), b2.count(b'\r')))
