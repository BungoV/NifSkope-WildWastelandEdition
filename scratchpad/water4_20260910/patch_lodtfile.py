# -*- coding: utf-8 -*-
"""patch_lodtfile.py -- the dye plane in the .lodl reader (src/lodtfile.{h,cpp}).
Each anchor must match exactly once; the CR count must be unchanged."""
import sys

def splice(path, edits):
    b = open(path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for anchor, new, where in edits:
        n = s.count(anchor)
        if n != 1:
            print('REFUSED: anchor matches %d times in %s: %r' % (n, path, anchor[:60]))
            sys.exit(1)
        if where == 'after':
            s = s.replace(anchor, anchor + new)
        elif where == 'replace':
            s = s.replace(anchor, new)
        else:
            s = s.replace(anchor, new + anchor)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr, 'CR count moved'
    open(path, 'wb').write(out)
    print('%s: %d -> %d bytes, CR %d' % (path, len(b), len(out), cr))

H = 'src/lodtfile.h'
C = 'src/lodtfile.cpp'

splice(H, [
    ('constexpr quint32 LODL_SECT_STROKE      = 1u << 7;   //!< stroke store\n',
     '/*! The DYE plane (lane WATER4): where one body\'s water is carried into\n'
     ' *  another\'s -- a river\'s tint past its mouth, a dye pin\'s plume.  It lives\n'
     ' *  in the version-3 header\'s reserved word at 0xF4 as a 32-bit offset, so\n'
     ' *  the version stays 3 and no existing offset moves; a reader checks this\n'
     ' *  bit, never the word. */\n'
     'constexpr quint32 LODL_SECT_DYE         = 1u << 8;   //!< dye plane\n', 'after'),
    ('\tquint32 flowEncoding() const { return flowEnc; }\n',
     '\t/*! The dye plane (LODL_SECT_DYE): samples per cell edge, 0 when absent.\n'
     '\t *  One uint32 a sample: bits 0..15 the SOURCE -- 1..32767 a body id, the\n'
     '\t *  body whose water this is; 0x8000 | n the n-th dye pin of the stroke\n'
     '\t *  store; 0 no dye -- and bits 16..23 the weight 0..255.  The plane is\n'
     '\t *  written at the flow plane\'s rate by the marking tool (watermark.cpp);\n'
     '\t *  the generator never writes one. */\n'
     '\tint dyePlaneSamples() const { return dyeS; }\n'
     '\t//! Where the dye plane store sits, 0 when absent (the word at 0xF4).\n'
     '\tquint64 dyePlaneOffset() const { return dyeAt; }\n', 'after'),
    ('\tquint8 shoreAt( int sx, int sy ) const;\n',
     '\t//! Dye word at a sample of the DYE plane\'s own grid; 0 = no dye here.\n'
     '\tquint32 dyeWordAt( int dx, int dy ) const;\n', 'after'),
    ('\tint bodyS = 0, flowS = 0, shoreS = 0;\n',
     '\tint dyeS = 0;\n\tquint64 dyeAt = 0;\n', 'after'),
    ('\tPlaneStore idStore, flowStore, shoreStore;\n',
     '\tPlaneStore dyeStore;\n', 'after'),
])

splice(C, [
    ('\t\tnStrokes = int( rd<quint32>( strokes, 0 ) );\n\t}\n',
     '\tif ( ver >= 3 && ( sect & LODL_SECT_DYE ) ) {\n'
     '\t\t/* The dye plane\'s offset is the version-3 header\'s reserved word at\n'
     '\t\t * 0xF4, 32 bits wide; the container it points at carries its own\n'
     '\t\t * rate and sample size, which is why one word is enough. */\n'
     '\t\tdyeAt = quint64( rd<quint32>( buf, 0xF4 ) );\n'
     '\t\tif ( !dyeAt )\n'
     '\t\t\treturn fail( QStringLiteral( "section dye is declared present but its offset "\n'
     '\t\t\t\t"is empty" ) );\n'
     '\t\tif ( !readPlaneStore( dyeAt, 4, dyeStore, error ) )\n'
     '\t\t\treturn false;\n'
     '\t\tdyeS = dyeStore.tileEdge;\n'
     '\t\tif ( !dyeS )\n'
     '\t\t\treturn fail( QStringLiteral( "the dye plane declares 0 samples a cell" ) );\n'
     '\t}\n', 'after'),
    ('quint8 LodtFile::shoreAt( int sx, int sy ) const\n{\n\treturn quint8( planeSampleOf( shoreStore, sx, sy, 255 ) );\n}\n',
     '\nquint32 LodtFile::dyeWordAt( int dx, int dy ) const\n{\n'
     '\t// 0 = no dye, which is also what an absent plane answers\n'
     '\treturn planeSampleOf( dyeStore, dx, dy, 0 );\n}\n', 'after'),
])
