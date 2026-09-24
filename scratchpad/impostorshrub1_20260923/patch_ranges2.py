"""IMPOSTORSHRUB1, second form: the bake keeps ONE LEVEL FOR THE WHOLE MODEL --
the lowest mesh-LOD slot that any BSMeshLODTriShape of the model fills -- not
the first non-empty range per shape (patch_ranges.py, withdrawn: it would have
drawn TreeMapleblasted05's L1 copy, shape [4] 0/75/5, on top of its full tree,
shape [8] 141/0/42). Applied to the PRE-LANE backup, so the result does not
depend on whether the first form was applied. Exact-once anchors, LF-only.
--check writes nothing."""
import sys
S = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshrub1_20260923/nifskope_ui.cpp.pre'
P = 'E:/Projects/NifskopeWildWastelandEdition/src/nifskope_ui.cpp'
T7, T8, T9, T10 = '\t' * 7, '\t' * 8, '\t' * 9, '\t' * 10

A_OLD = (
"\t\t\t\t\tif ( NifModel * mnif = skope->getNifModel() ) {\n"
"\t\t\t\t\t\tfor ( int b = 0; b < mnif->getBlockCount(); b++ ) {\n")
A_NEW = (
"\t\t\t\t\tif ( NifModel * mnif = skope->getNifModel() ) {\n"
"\t\t\t\t\t\t/* THE MESH-LOD LEVEL THE BAKE PHOTOGRAPHS, one for the whole\n"
"\t\t\t\t\t\t * model (lane IMPOSTORSHRUB1, 2026-09-23): the lowest slot that\n"
"\t\t\t\t\t\t * ANY BSMeshLODTriShape fills. See the slot rule below. */\n"
"\t\t\t\t\t\tint keptLevel = -1;\n"
"\t\t\t\t\t\tfor ( int b = 0; b < mnif->getBlockCount(); b++ ) {\n"
"\t\t\t\t\t\t\tconst QModelIndex iB = mnif->getBlockIndex( b );\n"
"\t\t\t\t\t\t\tif ( !mnif->isNiBlock( iB, \"BSMeshLODTriShape\" ) )\n"
"\t\t\t\t\t\t\t\tcontinue;\n"
"\t\t\t\t\t\t\tfor ( int k = 0; k < 3; k++ ) {\n"
"\t\t\t\t\t\t\t\tif ( mnif->get<uint>( iB, QStringLiteral( \"LOD%1 Size\" ).arg( k ) ) ) {\n"
"\t\t\t\t\t\t\t\t\tkeptLevel = keptLevel < 0 ? k : std::min( keptLevel, k );\n"
"\t\t\t\t\t\t\t\t\tbreak;\n"
"\t\t\t\t\t\t\t\t}\n"
"\t\t\t\t\t\t\t}\n"
"\t\t\t\t\t\t}\n"
"\t\t\t\t\t\tif ( keptLevel > 0 )\n"
"\t\t\t\t\t\t\tms << \"rangekept LOD\" << keptLevel << \" (no shape fills the LOD0 slot)\\n\";\n"
"\t\t\t\t\t\tfor ( int b = 0; b < mnif->getBlockCount(); b++ ) {\n")

B_OLD = (
"\t\t\t\t\t\t\t\t * (the maple: 71 + 23 + 8), the engine draws one range and\n"
"\t\t\t\t\t\t\t\t * the viewer draws every range at its default level. */\n"
"\t\t\t\t\t\t\t\tconst uint l1 = mnif->get<uint>( iB, \"LOD1 Size\" ), l2 = mnif->get<uint>( iB, \"LOD2 Size\" );\n"
"\t\t\t\t\t\t\t\tif ( l1 || l2 ) {\n"
"\t\t\t\t\t\t\t\t\tmnif->set<uint>( iB, \"LOD1 Size\", 0 );\n")
B_NEW = (
"\t\t\t\t\t\t\t\t * (the maple: 71 + 23 + 8), the engine draws one range and\n"
"\t\t\t\t\t\t\t\t * the viewer draws every range at its default level\n"
"\t\t\t\t\t\t\t\t * (BSShape::updateLodLevel: the first LOD0+LOD1+LOD2 triangles).\n"
"\t\t\t\t\t\t\t\t *\n"
"\t\t\t\t\t\t\t\t * ONE LEVEL FOR THE WHOLE MODEL, the lowest slot any shape\n"
"\t\t\t\t\t\t\t\t * fills (lane IMPOSTORSHRUB1, 2026-09-23). The shape names say\n"
"\t\t\t\t\t\t\t\t * what the slots hold: the exporter merges a full part (no\n"
"\t\t\t\t\t\t\t\t * prefix) into LOD0, its `L1_` copy into LOD1 and its `L2_`\n"
"\t\t\t\t\t\t\t\t * copy into LOD2 (HollyShrub01 `L1_HollyShrubSmall01` 0/302/0,\n"
"\t\t\t\t\t\t\t\t * DeadShrub04 `L2_DeadShrub04` 0/0/158, FoothillsShrubLarge01\n"
"\t\t\t\t\t\t\t\t * full + `L1_` 672/546/0). 27 of the 54 vanilla shrub, bush,\n"
"\t\t\t\t\t\t\t\t * sapling, hedge and undergrowth models ship NO full part at\n"
"\t\t\t\t\t\t\t\t * all - every shape's LOD0 slot is empty - so keeping LOD0\n"
"\t\t\t\t\t\t\t\t * left 0 triangles and every one of them baked an EMPTY card\n"
"\t\t\t\t\t\t\t\t * (halfW 1.077, 0 covered texels). Those now bake their\n"
"\t\t\t\t\t\t\t\t * lowest filled level (DeadShrub01 0/123/198 keeps the `L1_`\n"
"\t\t\t\t\t\t\t\t * 123, which has the larger box and 2.6x the triangle area of\n"
"\t\t\t\t\t\t\t\t * the `L2_` 198: scratchpad/impostorshrub1_20260923/ranges.py).\n"
"\t\t\t\t\t\t\t\t * The level is chosen per MODEL, not per shape: the maple\n"
"\t\t\t\t\t\t\t\t * TreeMapleblasted05 has its full tree in one shape (141/0/42)\n"
"\t\t\t\t\t\t\t\t * and its `L1_` copy alone in another (0/75/5); a per-shape\n"
"\t\t\t\t\t\t\t\t * rule would draw both, one over the other. Every slot below\n"
"\t\t\t\t\t\t\t\t * the kept level is empty in every shape, so the kept range\n"
"\t\t\t\t\t\t\t\t * starts at triangle 0 and `LOD0 Size` triangles draw exactly\n"
"\t\t\t\t\t\t\t\t * it; a shape that does not fill the kept level draws nothing,\n"
"\t\t\t\t\t\t\t\t * as the engine's level does. A model with any LOD0 part\n"
"\t\t\t\t\t\t\t\t * (keptLevel 0) bakes byte for byte as before. */\n"
"\t\t\t\t\t\t\t\tconst uint l1 = mnif->get<uint>( iB, \"LOD1 Size\" ), l2 = mnif->get<uint>( iB, \"LOD2 Size\" );\n"
"\t\t\t\t\t\t\t\tif ( l1 || l2 ) {\n"
"\t\t\t\t\t\t\t\t\tif ( keptLevel > 0 )\n"
"\t\t\t\t\t\t\t\t\t\tmnif->set<uint>( iB, \"LOD0 Size\", keptLevel == 1 ? l1 : l2 );\n"
"\t\t\t\t\t\t\t\t\tmnif->set<uint>( iB, \"LOD1 Size\", 0 );\n")

b = open(S, 'rb').read()
assert b.count(b'\r') == 0, 'backup is not LF-only'
for o, n in ((A_OLD, A_NEW), (B_OLD, B_NEW)):
    c = b.count(o.encode())
    print('anchor count', c)
    assert c == 1
    b = b.replace(o.encode(), n.encode())
if '--check' in sys.argv:
    print('check only'); sys.exit(0)
open(P, 'wb').write(b)
print('written; lines %d, CR %d' % (b.count(b'\n'), b.count(b'\r')))
