"""FARRING1 step 2: src/lodgen.cpp -- BC1 alpha through the mip chain, the
atlas's bc1 flag and its corrected comment, and the far-ring proxy pass."""

P = 'src/lodgen.cpp'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0, 'lodgen.cpp must be LF-only'
s = b.decode('utf-8')


def once(hay, needle):
    n = hay.count(needle)
    assert n == 1, 'anchor matched %d times, want 1:\n%s' % (n, needle[:240])


# ---- A. lodgenWriteDds forward declaration ---------------------------------
A = ("bool lodgenWriteDds( const QString & path, int w, int h,\n"
     "\tconst std::vector<quint32> & bgra, bool bc3 = false, int maxMips = 0 );\n")
once(s, A)
s = s.replace(A, "bool lodgenWriteDds( const QString & path, int w, int h,\n"
                 "\tconst std::vector<quint32> & bgra, bool bc3 = false, int maxMips = 0,\n"
                 "\tbool bc1Alpha = false );\n")

# ---- B. definition ---------------------------------------------------------
B = ("bool lodgenWriteDds( const QString & path, int w, int h,\n"
     "\tconst std::vector<quint32> & bgra, bool bc3, int maxMips )\n")
once(s, B)
s = s.replace(B, "bool lodgenWriteDds( const QString & path, int w, int h,\n"
                 "\tconst std::vector<quint32> & bgra, bool bc3, int maxMips, bool bc1Alpha )\n")

# ---- C. alpha through the mip chain ----------------------------------------
C = ("\t// mip chain by box filter, down to 4x4 (block floor); BC3 carries alpha\n"
     "\t// through the chain, BC1 stays opaque. maxMips > 0 stops the chain early:\n")
once(s, C)
s = s.replace(C, "\t// mip chain by box filter, down to 4x4 (block floor); BC3 carries alpha\n"
                 "\t// through the chain, and so does BC1 when bc1Alpha -- a BC1 sheet whose\n"
                 "\t// alpha is a CUT-OUT mask must keep it, or every mip past the top turns\n"
                 "\t// a tree's leaves back into a solid square, which is precisely the\n"
                 "\t// distance an atlas is looked at. Without it BC1 stays opaque, which is\n"
                 "\t// what every all-opaque caller (the terrain bakes, the emissive sheets)\n"
                 "\t// already wrote, byte for byte. maxMips > 0 stops the chain early:\n")

# the same two lines appear in lodgenEncodeArrayLayer, so the replace is
# scoped to lodgenWriteDds's own body
C2 = "\t\t\t\t\t( ( bc3 ? ( acc[3] / 4 ) : 0xFFU ) << 24 )\n"
lo = s.index("bool lodgenWriteDds( const QString & path, int w, int h,\n"
             "\tconst std::vector<quint32> & bgra, bool bc3, int maxMips, bool bc1Alpha )\n")
hi = s.index("\n//! Cached loader for source landscape textures", lo)
body = s[lo:hi]
once(body, C2)
s = s[:lo] + body.replace(C2, "\t\t\t\t\t( ( ( bc3 || bc1Alpha ) ? ( acc[3] / 4 ) : 0xFFU ) << 24 )\n") + s[hi:]

# ---- D. the atlas gains a bc1 flag -----------------------------------------
D = ("bool lodgenBuildAtlas( const QStringList & btoPaths, const QString & dataRoot,\n"
     "\tconst QString & atlasFileBase, const QString & atlasGameBase,\n"
     "\tconst QString & looseRoot, QString * error )\n")
once(s, D)
s = s.replace(D, "bool lodgenBuildAtlas( const QStringList & btoPaths, const QString & dataRoot,\n"
                 "\tconst QString & atlasFileBase, const QString & atlasGameBase,\n"
                 "\tconst QString & looseRoot, bool bc1, QString * error )\n")

# ---- E. the sheet write, and the comment that had vanilla's format wrong ----
E = ("\t// BC3, like vanilla's sheet: 8-bit alpha (soft card edges survive) and\n"
     "\t// RGB kept under transparent texels\n"
     "\tif ( !lodgenWriteDds( atlasFileBase + QStringLiteral( \".DDS\" ), AW, AH, sheet, true ) )\n")
once(s, E)
s = s.replace(E,
    "\t/* Vanilla's diffuse sheet is DXT1, not BC3 -- measured on the shipped\n"
    "\t * `Commonwealth.Objects.DDS`: 4096x2048, 13 mips, fourCC DXT1,\n"
    "\t * 5,592,552 bytes. (Its `_n` and `_s` are both BC5U.) The comment here\n"
    "\t * claimed BC3 for a month and the claim was the argument for ours being\n"
    "\t * BC3 too, which is twice the memory per sheet.\n"
    "\t *\n"
    "\t * bc1 writes DXT1 with BC1's one-bit punch-through for the cut-outs and\n"
    "\t * carries that alpha down the mip chain; BC3 keeps eight-bit alpha, which\n"
    "\t * only a consumer that soft-blends card edges can spend. So the stock\n"
    "\t * target takes BC1 (vanilla parity, half the memory) and FO4CS keeps BC3.\n"
    "\t * RGB survives under transparent texels either way -- the dilation above\n"
    "\t * is what puts it there. */\n"
    "\tif ( !lodgenWriteDds( atlasFileBase + QStringLiteral( \".DDS\" ), AW, AH, sheet, !bc1, 0, bc1 ) )\n")

# ---- F. the far-ring pass, after the merge ---------------------------------
F = ("\tif ( report )\n"
     "\t\t*report = QString( \"%1 shapes -> %2 across %3 chunks\" ).arg( before ).arg( after ).arg( chunks );\n"
     "\tif ( error )\n"
     "\t\terror->clear();\n"
     "\treturn true;\n"
     "}\n"
     "\n"
     "/*! Card sheet arrays:")
once(s, F)
snip = open('scratchpad/snip_simplify.cpp', encoding='utf-8').read()
assert snip.count('\r') == 0
s = s.replace(F, F[:F.index('\n\n/*! Card sheet arrays:')] + '\n' + snip + '\n/*! Card sheet arrays:')

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('lodgen.cpp: %d -> %d bytes, CR %d, LF %d' % (len(b), len(out), out.count(b'\r'), out.count(b'\n')))
