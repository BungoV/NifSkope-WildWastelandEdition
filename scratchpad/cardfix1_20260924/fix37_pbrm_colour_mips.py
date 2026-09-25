# CARDFIX1 step 7 (IMPOSTORPBRM1): the retargeted COLOUR source's mip chain is the .pbrm law evaluated on the
# SOURCE MAP'S OWN mip of each level, not a box filter of our level 0. Found by the gate's non-aa arm
# (gates/impostor_pbrm.run1.out): the box-filtered alpha lost coverage at the coarse mips a 1:1 render reads
# (coverage -1.8 %, silhouette halfW 370 vs the legacy card's 416 on the same arm), where the vanilla map's
# own mips keep it. law(mip(map)) is also what the viewport does: it samples the map, then applies the law.
# The other sources (RMAOS, specular, emissive) keep the box: their data is linear and carries no alpha test.
# LF-only file.
P = 'E:/Projects/NifskopeWWE-cardfix1/src/nifskope_ui.cpp'

W_OLD = ("//! An uncompressed B8G8R8A8 DDS (the legacy header) with a box-filtered mip chain down to 1 x 1.\n"
         "bool wwWriteDdsBgra( const QString & path, const QImage & top )\n"
         "{\n")
W_NEW = ("/*! An uncompressed B8G8R8A8 DDS (the legacy header) with a mip chain down to 1 x 1: box-filtered, or,\n"
         " *  with `level`, each level made by it at that level's size (the colour source: the law on the map's own mip). */\n"
         "bool wwWriteDdsBgra( const QString & path, const QImage & top,\n"
         "\tconst std::function<QImage( int, int )> & level = std::function<QImage( int, int )>() )\n"
         "{\n")
L_OLD = ("\t\tconst int w2 = qMax( 1, lv.width() / 2 ), h2 = qMax( 1, lv.height() / 2 );\n"
         "\t\tQImage nx( w2, h2, QImage::Format_ARGB32 );\n")
L_NEW = ("\t\tconst int w2 = qMax( 1, lv.width() / 2 ), h2 = qMax( 1, lv.height() / 2 );\n"
         "\t\tif ( level ) {\n"
         "\t\t\tlv = level( w2, h2 ).convertToFormat( QImage::Format_ARGB32 );\n"
         "\t\t\tcontinue;\n"
         "\t\t}\n"
         "\t\tQImage nx( w2, h2, QImage::Format_ARGB32 );\n")
WR_OLD = ("\tauto write = [&]( const QImage & img, const QString & suffix, QString & name ) {\n"
          "\t\tconst QString file = stem + \"_\" + suffix + \".dds\";\n"
          "\t\tif ( !wwWriteDdsBgra( texDir + \"/\" + file, img ) ) {\n")
WR_NEW = ("\tauto write = [&]( const QImage & img, const QString & suffix, QString & name,\n"
          "\t\t\tconst std::function<QImage( int, int )> & level = std::function<QImage( int, int )>() ) {\n"
          "\t\tconst QString file = stem + \"_\" + suffix + \".dds\";\n"
          "\t\tif ( !wwWriteDdsBgra( texDir + \"/\" + file, img, level ) ) {\n")
C_OLD = ("\t// slot 0: the colour, tinted\n"
         "\t{\n"
         "\t\tconst Grid g = gridOf( { base.get(), tint.get() } );\n"
         "\t\tQImage img( g.w, g.h, QImage::Format_ARGB32 );\n"
         "\t\tfor ( int y = 0; y < g.h; y++ ) {\n"
         "\t\t\tQRgb * d = reinterpret_cast<QRgb *>( img.scanLine( y ) );\n"
         "\t\t\tfor ( int x = 0; x < g.w; x++ ) {\n"
         "\t\t\t\tconst float u = ( float( x ) + 0.5f ) / float( g.w ), v = ( float( y ) + 0.5f ) / float( g.h );\n")
C_NEW = ("\t// slot 0: the colour, tinted -- every mip level evaluated on the maps' own mip of that size\n"
         "\t{\n"
         "\t\tconst Grid g = gridOf( { base.get(), tint.get() } );\n"
         "\t\tauto colourAt = [&]( int gw, int gh ) {\n"
         "\t\tQImage img( gw, gh, QImage::Format_ARGB32 );\n"
         "\t\tfor ( int y = 0; y < gh; y++ ) {\n"
         "\t\t\tQRgb * d = reinterpret_cast<QRgb *>( img.scanLine( y ) );\n"
         "\t\t\tfor ( int x = 0; x < gw; x++ ) {\n"
         "\t\t\t\tconst float u = ( float( x ) + 0.5f ) / float( gw ), v = ( float( y ) + 0.5f ) / float( gh );\n")
C2_OLD = ("\t\t\t\t\tconst FloatVector4 c = at( base, u, v, g.w );\n")
C2_NEW = ("\t\t\t\t\tconst FloatVector4 c = at( base, u, v, gw );\n")
C3_OLD = ("\t\t\t\t\tconst FloatVector4 tv = at( tint, u, v, g.w );\n")
C3_NEW = ("\t\t\t\t\tconst FloatVector4 tv = at( tint, u, v, gw );\n")
C4_OLD = ("\t\t\t\td[x] = qRgba( wwByte( wwLinearToSrgb( rgb[0] ) ), wwByte( wwLinearToSrgb( rgb[1] ) ),\n"
          "\t\t\t\t\twwByte( wwLinearToSrgb( rgb[2] ) ), wwByte( a ) );\n"
          "\t\t\t}\n"
          "\t\t}\n"
          "\t\tif ( !write( img, QStringLiteral( \"c\" ), s.colour ) )\n")
C4_NEW = ("\t\t\t\td[x] = qRgba( wwByte( wwLinearToSrgb( rgb[0] ) ), wwByte( wwLinearToSrgb( rgb[1] ) ),\n"
          "\t\t\t\t\twwByte( wwLinearToSrgb( rgb[2] ) ), wwByte( a ) );\n"
          "\t\t\t}\n"
          "\t\t}\n"
          "\t\treturn img;\n"
          "\t\t};\n"
          "\t\tif ( !write( colourAt( g.w, g.h ), QStringLiteral( \"c\" ), s.colour, colourAt ) )\n")
INC_OLD = "#include <QTemporaryDir>\n"
INC_NEW = "#include <QTemporaryDir>\n#include <functional>\n"

b = open(P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')
for o, n in [(W_OLD, W_NEW), (L_OLD, L_NEW), (WR_OLD, WR_NEW), (C_OLD, C_NEW), (C2_OLD, C2_NEW), (C3_OLD, C3_NEW),
             (C4_OLD, C4_NEW), (INC_OLD, INC_NEW)]:
    assert s.count(o) == 1, (o[:70], s.count(o))
    s = s.replace(o, n)
out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(P, 'wb').write(out)
print('patched nifskope_ui.cpp: %+d bytes, CR %d' % (len(out) - len(b), cr))
