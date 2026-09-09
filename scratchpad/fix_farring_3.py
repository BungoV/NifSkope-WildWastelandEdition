"""FARRING1 step 3: src/nifcli.cpp -- the far-ring knobs, --slot-fallback,
--atlas-bc1, --dump-geometry, and the pass in the region pipeline."""

P = 'src/nifcli.cpp'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0, 'nifcli.cpp must be LF-only'
s = b.decode('utf-8')


def once(hay, needle):
    n = hay.count(needle)
    assert n == 1, 'anchor matched %d times, want 1:\n%s' % (n, needle[:240])


# ---- 1. options ------------------------------------------------------------
A = ("\tbool lgAtlas = false;\n"
     "\tbool lgArrays = false;\n"
     "\tbool lgMerge = true;\n")
once(s, A)
s = s.replace(A, A +
    "\t/* Vanilla's own diffuse sheet is DXT1 (measured: Commonwealth.Objects.DDS,\n"
    "\t * 4096x2048, 13 mips, 5,592,552 bytes), so BC1 is parity AND half the\n"
    "\t * memory. Ours has been BC3 since the atlas shipped; the panel picks it\n"
    "\t * off the Target, the CLI off this flag. */\n"
    "\tbool lgAtlasBc1 = false;\n"
    "\t/* The panel has had this toggle since the chunk builder shipped; the CLI\n"
    "\t * had no way to reach it, which made every far ring over Sanctuary empty\n"
    "\t * (measured: 0 of 19,507 refs in chunk (-32,16) fill MNAM slot 2). */\n"
    "\tbool lgSlotFallback = false;\n"
    "\t/* Far-ring proxies (lodgen.h). --no-simplify turns the pass off; the\n"
    "\t * three ratios and the error bound are per ring. Ring 0 is never cut. */\n"
    "\tLodgenSimplifyOptions lgSimplify;\n"
    "\tQString lgDumpGeometry;\n")

# ---- 2. flag parsing -------------------------------------------------------
B = ("\t\telse if ( t == QLatin1String( \"--merge\" ) ) lgMerge = true;\n"
     "\t\telse if ( t == QLatin1String( \"--no-merge\" ) ) lgMerge = false;\n")
once(s, B)
s = s.replace(B, B +
    "\t\telse if ( t == QLatin1String( \"--atlas-bc1\" ) ) lgAtlasBc1 = true;\n"
    "\t\telse if ( t == QLatin1String( \"--slot-fallback\" ) ) lgSlotFallback = true;\n"
    "\t\telse if ( t == QLatin1String( \"--no-simplify\" ) ) lgSimplify.enabled = false;\n"
    "\t\telse if ( t == QLatin1String( \"--simplify8\" ) ) lgSimplify.ratio8 = next().toFloat();\n"
    "\t\telse if ( t == QLatin1String( \"--simplify16\" ) ) lgSimplify.ratio16 = next().toFloat();\n"
    "\t\telse if ( t == QLatin1String( \"--simplify32\" ) ) lgSimplify.ratio32 = next().toFloat();\n"
    "\t\telse if ( t == QLatin1String( \"--simplify-error\" ) ) lgSimplify.errorWorld = next().toFloat();\n"
    "\t\telse if ( t == QLatin1String( \"--dump-geometry\" ) ) lgDumpGeometry = next();\n")

# ---- 2b. cmdLodgen takes the three new settings ----------------------------
S = ("\tconst QString & btdPath, bool btdProbe, bool verifyOnly,\n"
     "\tconst QString & dumpLand, bool refreshAo )\n")
once(s, S)
s = s.replace(S, ("\tconst QString & btdPath, bool btdProbe, bool verifyOnly,\n"
                  "\tconst QString & dumpLand, bool refreshAo,\n"
                  "\tbool slotFallback, bool atlasBc1, const LodgenSimplifyOptions & simplify )\n"))

T = ("\t\t\tlgLodtVerify, lgDumpLand, lgRefreshAo );\n")
once(s, T)
s = s.replace(T, ("\t\t\tlgLodtVerify, lgDumpLand, lgRefreshAo,\n"
                  "\t\t\tlgSlotFallback, lgAtlasBc1, lgSimplify );\n"))

# ---- 3. the chunk builder gets the fallback --------------------------------
C = ("\t\t\t\t\toopts.impostorDir = impostors;\n"
     "\t\t\t\t\toopts.impostorFromLevel = impostorFromLevel;\n")
once(s, C)
s = s.replace(C, C + "\t\t\t\t\toopts.slotFallback = slotFallback;\n")

# ---- 4. the atlas call takes the format ------------------------------------
D = ("\t\t\t\tatlasDir + \"/\" + ws + QStringLiteral( \".LodgenObjects\" ),\n"
     "\t\t\t\tQString( \"data\\\\Textures\\\\Terrain\\\\%1\\\\Objects\\\\%1.LodgenObjects\" ).arg( ws ),\n"
     "\t\t\t\tlooseRoot, &aerr ) ) {\n")
once(s, D)
s = s.replace(D, ("\t\t\t\tatlasDir + \"/\" + ws + QStringLiteral( \".LodgenObjects\" ),\n"
                  "\t\t\t\tQString( \"data\\\\Textures\\\\Terrain\\\\%1\\\\Objects\\\\%1.LodgenObjects\" ).arg( ws ),\n"
                  "\t\t\t\tlooseRoot, atlasBc1, &aerr ) ) {\n"))

# ---- 5. the far-ring pass, right after the merge ---------------------------
E = ("\t\tif ( merge && !writtenBto.isEmpty() ) {\n"
     "\t\t\t// last: one shape per material the engine can tell apart (after the atlas and the arrays)\n"
     "\t\t\tQString rep, merr;\n"
     "\t\t\tif ( lodgenMergeChunkShapes( writtenBto, &rep, &merr ) )\n"
     "\t\t\t\tout() << \"merged: \" << rep << Qt::endl;\n"
     "\t\t\telse\n"
     "\t\t\t\terr() << \"merge: \" << merr << Qt::endl;\n"
     "\t\t}\n")
once(s, E)
s = s.replace(E, E +
    "\t\tif ( simplify.enabled && !writtenBto.isEmpty() ) {\n"
    "\t\t\t/* After the merge, because the proxy is the MERGED shape: the merge\n"
    "\t\t\t * has already made one shape per material, which is the cluster a\n"
    "\t\t\t * far ring wants one simplified mesh of. Ring 0 is never cut. */\n"
    "\t\t\tQString rep, serr;\n"
    "\t\t\tif ( lodgenSimplifyFarRings( writtenBto, simplify, &rep, &serr ) )\n"
    "\t\t\t\tout() << \"far rings: \" << rep << Qt::endl;\n"
    "\t\t\telse\n"
    "\t\t\t\terr() << \"far rings: \" << serr << Qt::endl;\n"
    "\t\t}\n")

# ---- 6. --dump-geometry, beside --dump-shapes ------------------------------
F = ("\t\t// --print-source and --list-files are QUESTIONS about the stack: they\n"
     "\t\t// answer and stop, so neither can be mistaken for a bake\n")
once(s, F)
snip = open('scratchpad/snip_dumpgeom.cpp', encoding='utf-8').read()
assert snip.count('\r') == 0
s = s.replace(F, snip + F)

# ---- 7. usage --------------------------------------------------------------
G = ("\t\t  << \"  lodgen <file.esm> --worldspace HEX --terrain X Y [--dim 4] -o OUT.btr\\n\"\n")
once(s, G)
s = s.replace(G,
    "\t\t  << \"  lodgen --dump-geometry FILE.BTO          one line per shape: what it\\n\"\n"
    "\t\t  << \"                                          weighs (vertices, triangles,\\n\"\n"
    "\t\t  << \"                                          segments) and the counts its\\n\"\n"
    "\t\t  << \"                                          own invariants stand on -\\n\"\n"
    "\t\t  << \"                                          triangles in the wrong segment\\n\"\n"
    "\t\t  << \"                                          cell, centroids outside the\\n\"\n"
    "\t\t  << \"                                          chunk, vertices outside the\\n\"\n"
    "\t\t  << \"                                          bounding sphere or the node\\n\"\n"
    "\t\t  << \"                                          AABB - plus an `i` line with\\n\"\n"
    "\t\t  << \"                                          its object identity indices\\n\"\n"
    "\t\t  << \"  lodgen ... --terrain-region ... [--slot-fallback]\\n\"\n"
    "\t\t  << \"                                          keep an object at a ring whose\\n\"\n"
    "\t\t  << \"                                          MNAM slot is empty by using the\\n\"\n"
    "\t\t  << \"                                          nearest filled one. OFF matches\\n\"\n"
    "\t\t  << \"                                          vanilla, where it drops out;\\n\"\n"
    "\t\t  << \"                                          measured, 0 of 19,507 refs in\\n\"\n"
    "\t\t  << \"                                          chunk (-32,16) fill slot 2, so\\n\"\n"
    "\t\t  << \"                                          Sanctuary's ring 2 is empty\\n\"\n"
    "\t\t  << \"                                          without this or --impostors\\n\"\n"
    "\t\t  << \"  lodgen ... --terrain-region ... [--atlas [--atlas-bc1]]\\n\"\n"
    "\t\t  << \"                                          --atlas-bc1 writes the diffuse\\n\"\n"
    "\t\t  << \"                                          sheet as DXT1 with one-bit alpha,\\n\"\n"
    "\t\t  << \"                                          which is what vanilla's own sheet\\n\"\n"
    "\t\t  << \"                                          is (measured) and half the\\n\"\n"
    "\t\t  << \"                                          memory; BC3 is the default and\\n\"\n"
    "\t\t  << \"                                          keeps eight-bit alpha for FO4CS\\n\"\n"
    "\t\t  << \"  lodgen ... --terrain-region ... [--no-simplify]\\n\"\n"
    "\t\t  << \"         [--simplify8 R] [--simplify16 R] [--simplify32 R]\\n\"\n"
    "\t\t  << \"         [--simplify-error UNITS]         far-ring proxies: after the\\n\"\n"
    "\t\t  << \"                                          merge, each ring's merged shapes\\n\"\n"
    "\t\t  << \"                                          keep R of their triangles\\n\"\n"
    "\t\t  << \"                                          (default 1 / 0.35 / 0.2 at dim\\n\"\n"
    "\t\t  << \"                                          8 / 16 / 32; ring 0 is never\\n\"\n"
    "\t\t  << \"                                          touched). Alpha-tested shapes\\n\"\n"
    "\t\t  << \"                                          and impostor cards keep every\\n\"\n"
    "\t\t  << \"                                          triangle; the error is world\\n\"\n"
    "\t\t  << \"                                          units at ring 0, scaled by the\\n\"\n"
    "\t\t  << \"                                          ring's dim (default 32)\\n\"\n"
    + G)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('nifcli.cpp: %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\r')))
