# CARDFIX1 step 7 (IMPOSTORPBRM1) job 3, the lodgen card region: a pbr set's `<id>_oct_s.png` (RGB sqrt(F0'),
# A specular weight; written by the bake only when a shape departs from the default specular) becomes
# `<id>_oct_s.DDS`, BC7 at the aux size (the `_n` sheet's codec and weights), dilated like the other aux sheets,
# and the set's .lodm names it under the new texture key `specular`. No `_oct_s.png` = no key = F0 0.04, weight 1.
# (The strings below carry one tab too many; U() strips it -- measured depths 4/5/3.)
# The card ARRAYS writer (~14724) does not carry it: stated in the docs. LF-only file.
P = 'E:/Projects/NifskopeWWE-cardfix1/src/lodgen.cpp'

LOAD_OLD = ("\t\t\t\t\tif ( emi.size() != alb.size() )\n"
            "\t\t\t\t\t\temi = QImage();\n"
            "\t\t\t\t}\n")
LOAD_NEW = LOAD_OLD + (
    "\t\t\t\t/* The fifth sheet, a pbr set's SPECULAR (IMPOSTORPBRM1): `_s`, RGB sqrt(F0')\n"
    "\t\t\t\t * and A the specular weight. Optional twice over: the bake writes it only\n"
    "\t\t\t\t * when a shape departs from the default specular, and never on a legacy set. */\n"
    "\t\t\t\tQImage spc;\n"
    "\t\t\t\tif ( card.octPbr ) {\n"
    "\t\t\t\t\tconst QString octS = dir + \"/\" + id + QStringLiteral( \"_oct_s.png\" );\n"
    "\t\t\t\t\tif ( QFile::exists( octS ) ) {\n"
    "\t\t\t\t\t\tspc = QImage( octS ).convertToFormat( QImage::Format_ARGB32 );\n"
    "\t\t\t\t\t\tif ( spc.size() != alb.size() )\n"
    "\t\t\t\t\t\t\tspc = QImage();\n"
    "\t\t\t\t\t}\n"
    "\t\t\t\t}\n")
DIL_OLD = "\t\t\t\t\t\tlodgenDilateFrames( emi, alb, card.octTileW, card.octTileH, deep );\n"
DIL_NEW = DIL_OLD + ("\t\t\t\t\tif ( !spc.isNull() )\n"
                     "\t\t\t\t\t\tlodgenDilateFrames( spc, alb, card.octTileW, card.octTileH, deep );\n")
WR_OLD = ("\t\t\t\t\t\tconst QImage emiA = down( emi );\n"
          "\t\t\t\t\t\tok = lodgenWriteDds( base + emSfx, aw, ah, pixels( emiA ), false, auxMips ) && ok;\n"
          "\t\t\t\t\t}\n")
WR_NEW = WR_OLD + ("\t\t\t\t\t// the specular sheet is BC7 with its alpha (the weight), the `_n` sheet's codec\n"
                   "\t\t\t\t\tif ( !spc.isNull() && !QFile::exists( base + QStringLiteral( \"_s.DDS\" ) ) ) {\n"
                   "\t\t\t\t\t\tconst QImage spcA = down( spc );\n"
                   "\t\t\t\t\t\tok = lodgenWriteDds( base + QStringLiteral( \"_s.DDS\" ), aw, ah, pixels( spcA ), true, auxMips,\n"
                   "\t\t\t\t\t\t\t\tfalse, 0, 0, false, true ) && ok;\n"
                   "\t\t\t\t\t}\n")
KEY_OLD = ("\t\t\t\t\t\tif ( !emi.isNull() )\n"
           "\t\t\t\t\t\t\ttex.insert( QStringLiteral( \"emissive\" ), game + emSfx );\n")
KEY_NEW = KEY_OLD + ("\t\t\t\t\t\t// a new key, no version bump: a reader ignores keys it does not know (LODGEN_LODM_FORMAT 2)\n"
                     "\t\t\t\t\t\tif ( !spc.isNull() )\n"
                     "\t\t\t\t\t\t\ttex.insert( QStringLiteral( \"specular\" ), game + QStringLiteral( \"_s.DDS\" ) );\n")

b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')
def U(t):
    return '\n'.join(l[1:] if l.startswith('\t') else l for l in t.split('\n'))


for o, n in [(U(a), U(c)) for a, c in [(LOAD_OLD, LOAD_NEW), (DIL_OLD, DIL_NEW), (WR_OLD, WR_NEW), (KEY_OLD, KEY_NEW)]]:
    assert s.count(o) == 1, (o[:70], s.count(o))
    s = s.replace(o, n)
out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('patched lodgen.cpp: +%d bytes' % (len(out) - len(b)))
