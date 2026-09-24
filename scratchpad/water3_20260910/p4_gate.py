# -*- coding: utf-8 -*-
"""p4_gate.py -- lane BUILD5: wire gate P4 into the marking tool's self-test.

PENDING.md step 6. `WaterMarkDoc::setFlowRate` shipped with lane WATER3 and
re-derives the flow plane at another sample rate from the SAME world-coordinate
strokes, but nothing measured it, so P4 was "mechanism shipped, gate not
written".

Two edits, both LF-only files (CR must stay 0):

  src/watermark.h    one public method -- `meanFileFlow`, the instrument: the
                     mean flow direction READ BACK OUT OF THE FILE over one
                     body, at the file's own rate.
  src/watermark.cpp  that method, and the P4 case in lodtWaterMarkSelfTest.

Why read the FILE and not the solver: the solve field lives at the body-ID
plane's rate and does not move when the flow rate does, so a check written
against the solver could not fail on its input -- the exact defect this lane's
predecessor already recorded once (CONSTITUTION rule 4). The plane written at
8 samples a cell is a different, coarser object, and it is what a consumer
reads.

    python scratchpad/water3_20260910/p4_gate.py --check
    python scratchpad/water3_20260910/p4_gate.py
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
check_only = '--check' in sys.argv

H_ANCHOR = b'\tbool setFlowRate( int samplesPerCell, QString * error );\n'
H_NEW = H_ANCHOR + (
    b'\t/*! P4\'s instrument: the mean direction of the flow plane AS IT SITS IN\n'
    b'\t *  THE FILE, over body `id`, in degrees, with the number of samples it\n'
    b'\t *  averaged. It reads the file\'s own flow plane at the file\'s own rate\n'
    b'\t *  through the body plane, so a re-derivation at a coarser rate is\n'
    b'\t *  measured the way a CONSUMER sees it and not the way the solver\n'
    b'\t *  remembers it -- the solve field lives at the body plane\'s rate and\n'
    b'\t *  does not move when the flow rate does.\n'
    b'\t *\n'
    b'\t *  `onlyMarked` counts only the samples that differ from the body\'s\n'
    b'\t *  AUTOMATIC word, which is the half a stroke actually moved. Over a\n'
    b'\t *  whole body the untouched majority pins the mean to that constant and\n'
    b'\t *  the number cannot move, which would make a gate written on it unable\n'
    b'\t *  to fail. */\n'
    b'\tbool meanFileFlow( int id, bool onlyMarked, double & degrees, qint64 & samples,\n'
    b'\t\tQString * error ) const;\n'
)

CPP_ANCHOR = (
    b'\tif ( int( flowRate ) != samplesPerCell ) {\n'
    b'\t\tflowRate = quint32( samplesPerCell );\n'
    b'\t\tdirty = true;\n'
    b'\t}\n'
    b'\treturn true;\n'
    b'}\n'
)
CPP_NEW = CPP_ANCHOR + (
    b'\n'
    b'/*! The mean flow direction of body `id`, read back OUT OF THE FILE.\n'
    b' *\n'
    b' *  The flow plane is tiled per CELL exactly as the body plane is, so tile\n'
    b' *  (tx,ty) of one is tile (tx,ty) of the other and both are inflated once\n'
    b' *  per tile. The sample -> body-texel mapping is packFlowPlane\'s own, on\n'
    b' *  purpose: an instrument that mapped differently from the packer would\n'
    b' *  measure a plane nobody wrote. */\n'
    b'bool WaterMarkDoc::meanFileFlow( int id, bool onlyMarked, double & degrees,\n'
    b'\tqint64 & samples, QString * error ) const\n'
    b'{\n'
    b'\tdegrees = 0.0;\n'
    b'\tsamples = 0;\n'
    b'\tif ( !opened || !flowPlane.ok || !idPlane.ok ) {\n'
    b'\t\tif ( error )\n'
    b'\t\t\t*error = QStringLiteral( "no landscape file is open" );\n'
    b'\t\treturn false;\n'
    b'\t}\n'
    b'\tconst int spc = lodl->samplesPerCell();\n'
    b'\tconst int fe = flowPlane.tileEdge;\n'
    b'\tconst int ie = idPlane.tileEdge;\n'
    b'\tif ( fe <= 0 || ie <= 0 || spc % fe || spc % ie ) {\n'
    b'\t\tif ( error )\n'
    b'\t\t\t*error = QStringLiteral( "plane rates %1 / %2 do not divide %3 samples a cell" )\n'
    b'\t\t\t\t.arg( fe ).arg( ie ).arg( spc );\n'
    b'\t\treturn false;\n'
    b'\t}\n'
    b'\tconst int flowStep = spc / fe, idStep = spc / ie;\n'
    b'\tQFile f( filePath );\n'
    b'\tif ( !f.open( QIODevice::ReadOnly ) ) {\n'
    b'\t\tif ( error )\n'
    b'\t\t\t*error = QStringLiteral( "could not open %1" ).arg( filePath );\n'
    b'\t\treturn false;\n'
    b'\t}\n'
    b'\tQByteArray fRaw, iRaw;\n'
    b'\tquint32 fUni = 0, iUni = 0;\n'
    b'\tbool fIsUni = false, iIsUni = false;\n'
    b'\tdouble sx = 0.0, sy = 0.0;\n'
    b'\tconst int tilesX = qMin( flowPlane.tilesX, idPlane.tilesX );\n'
    b'\tconst int tilesY = qMin( flowPlane.tilesY, idPlane.tilesY );\n'
    b'\tfor ( int ty = 0; ty < tilesY; ty++ ) {\n'
    b'\t\tfor ( int tx = 0; tx < tilesX; tx++ ) {\n'
    b'\t\t\tif ( !tileOf( f, flowPlane, tx, ty, fRaw, fUni, fIsUni )\n'
    b'\t\t\t\t|| !tileOf( f, idPlane, tx, ty, iRaw, iUni, iIsUni ) ) {\n'
    b'\t\t\t\tif ( error )\n'
    b'\t\t\t\t\t*error = QStringLiteral( "tile %1,%2 would not inflate" ).arg( tx ).arg( ty );\n'
    b'\t\t\t\treturn false;\n'
    b'\t\t\t}\n'
    b'\t\t\tfor ( int j = 0; j < fe; j++ ) {\n'
    b'\t\t\t\tconst int lj = ( j * flowStep ) / idStep;\n'
    b'\t\t\t\tfor ( int i = 0; i < fe; i++ ) {\n'
    b'\t\t\t\t\tconst int li = ( i * flowStep ) / idStep;\n'
    b'\t\t\t\t\tconst quint16 bid = iIsUni ? quint16( iUni )\n'
    b'\t\t\t\t\t\t: rd16( iRaw.constData() + ( qsizetype( lj ) * ie + li ) * 2 );\n'
    b'\t\t\t\t\tif ( int( bid ) != id )\n'
    b'\t\t\t\t\t\tcontinue;\n'
    b'\t\t\t\t\tconst quint16 word = fIsUni ? quint16( fUni )\n'
    b'\t\t\t\t\t\t: rd16( fRaw.constData() + ( qsizetype( j ) * fe + i ) * 2 );\n'
    b'\t\t\t\t\tif ( onlyMarked && word == automaticWord( bid ) )\n'
    b'\t\t\t\t\t\tcontinue;\n'
    b'\t\t\t\t\tconst double ang = double( word & 0xFF ) / 256.0 * kTwoPi;\n'
    b'\t\t\t\t\tsx += std::cos( ang );\n'
    b'\t\t\t\t\tsy += std::sin( ang );\n'
    b'\t\t\t\t\tsamples++;\n'
    b'\t\t\t\t}\n'
    b'\t\t\t}\n'
    b'\t\t}\n'
    b'\t}\n'
    b'\tdegrees = std::atan2( sy, sx ) * 180.0 / 3.14159265358979;\n'
    b'\treturn samples > 0;\n'
    b'}\n'
)

TEST_ANCHOR = b'\t// ---- 6. a stroke on dry land is refused in words ------------------------\n'
TEST_NEW = (
    b'\t/* ---- 5b. P4: the stroke survives a re-derivation at another rate ------\n'
    b'\t * The strokes are WORLD coordinates, so the plane may be written at any\n'
    b'\t * rate the file\'s samples per cell divides. Measured as a CONSUMER sees\n'
    b'\t * it: the file is saved at each rate and the mean direction is read back\n'
    b'\t * OUT OF THE FILE. The file\'s own rate is restored afterwards, because\n'
    b'\t * the round-trip and undo gates below compare against the writer\'s\n'
    b'\t * bytes. */\n'
    b'\t{\n'
    b'\t\tconst int rate0 = doc.flowSamples();\n'
    b'\t\tdouble a0 = 0.0, a1 = 0.0, w0 = 0.0, w1 = 0.0;\n'
    b'\t\tqint64 n0 = 0, n1 = 0, m0 = 0, m1 = 0;\n'
    b'\t\tint rateBack = 0;\n'
    b'\t\tbool got = doc.save( &err );\n'
    b'\t\tif ( got ) {\n'
    b'\t\t\tWaterMarkDoc d0;\n'
    b'\t\t\tgot = d0.open( path, &err )\n'
    b'\t\t\t\t&& d0.meanFileFlow( river, true, a0, n0, &err );\n'
    b'\t\t\tif ( got )\n'
    b'\t\t\t\td0.meanFileFlow( river, false, w0, m0, &err );\n'
    b'\t\t}\n'
    b'\t\tif ( got )\n'
    b'\t\t\tgot = doc.setFlowRate( 8, &err );\n'
    b'\t\tif ( got ) {\n'
    b'\t\t\tWaterMarkSolve st8;\n'
    b'\t\t\tdoc.solve( &st8, &err );\n'
    b'\t\t\tgot = doc.save( &err );\n'
    b'\t\t}\n'
    b'\t\tif ( got ) {\n'
    b'\t\t\tWaterMarkDoc d1;\n'
    b'\t\t\tgot = d1.open( path, &err )\n'
    b'\t\t\t\t&& d1.meanFileFlow( river, true, a1, n1, &err );\n'
    b'\t\t\tif ( got ) {\n'
    b'\t\t\t\td1.meanFileFlow( river, false, w1, m1, &err );\n'
    b'\t\t\t\trateBack = d1.flowSamples();\n'
    b'\t\t\t}\n'
    b'\t\t}\n'
    b'\t\tsay( QStringLiteral( "P4: body %1 carries %2 of %3 flow samples at %4, and %5 of "\n'
    b'\t\t\t"%6 at 8; the whole-body mean is %7 then %8 degrees" ).arg( river ).arg( n0 )\n'
    b'\t\t\t.arg( m0 ).arg( rate0 ).arg( n1 ).arg( m1 ).arg( w0, 0, \'f\', 2 )\n'
    b'\t\t\t.arg( w1, 0, \'f\', 2 ) );\n'
    b'\t\t/* The floor, on the other side of the gate: the re-bake has to have\n'
    b'\t\t * HAPPENED. A plane still written at rate0, or one with no samples of\n'
    b'\t\t * this body in it, would agree with itself perfectly. */\n'
    b'\t\tcheck( QStringLiteral( "P4 floor: the file was really re-written at 8 samples a "\n'
    b'\t\t\t"cell (header says %1, was %2) and body %3 still has samples in it "\n'
    b'\t\t\t"(%4 at %2, %5 at 8)" ).arg( rateBack ).arg( rate0 ).arg( river )\n'
    b'\t\t\t.arg( n0 ).arg( n1 ),\n'
    b'\t\t\tgot && rateBack == 8 && rate0 != 8 && n0 > 0 && n1 > 0 );\n'
    b'\t\tdouble moved = 999.0;\n'
    b'\t\tif ( got ) {\n'
    b'\t\t\tmoved = std::fabs( a1 - a0 );\n'
    b'\t\t\tif ( moved > 180.0 )\n'
    b'\t\t\t\tmoved = 360.0 - moved;\n'
    b'\t\t}\n'
    b'\t\tcheck( QStringLiteral( "P4 re-bake: body %1 marked at %2 samples a cell and "\n'
    b'\t\t\t"re-written at 8 still points the same way (mean %3 -> %4 degrees, moved "\n'
    b'\t\t\t"%5, tolerance 5)" ).arg( river ).arg( rate0 ).arg( a0, 0, \'f\', 2 )\n'
    b'\t\t\t.arg( a1, 0, \'f\', 2 ).arg( moved, 0, \'f\', 2 ), got && moved <= 5.0 );\n'
    b'\t\tQString e2;\n'
    b'\t\tif ( !doc.setFlowRate( rate0, &e2 ) )\n'
    b'\t\t\tsay( QStringLiteral( "could not restore the file\'s own rate: %1" ).arg( e2 ) );\n'
    b'\t\tWaterMarkSolve stBack;\n'
    b'\t\tdoc.solve( &stBack, &e2 );\n'
    b'\t}\n'
    b'\n'
) + TEST_ANCHOR

EDITS = [
    ('src/watermark.h', [(H_ANCHOR, H_NEW)]),
    ('src/watermark.cpp', [(CPP_ANCHOR, CPP_NEW), (TEST_ANCHOR, TEST_NEW)]),
]


def main():
    failed = 0
    for rel, subs in EDITS:
        path = os.path.join(ROOT, rel)
        b = open(path, 'rb').read()
        cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
        print('%-20s %s %8d bytes %6d lines CR=%d'
              % (rel, hashlib.sha256(b).hexdigest()[:16], n0, lf0, cr0))
        out = b
        for anchor, repl in subs:
            hits = out.count(anchor)
            if hits != 1:
                print('  REFUSED: %d matches for %r' % (hits, anchor[:70]))
                failed += 1
                out = None
                break
            out = out.replace(anchor, repl, 1)
        if out is None:
            continue
        cr1, lf1 = out.count(b'\r'), out.count(b'\n')
        if cr1 != cr0:
            print('  REFUSED: CR moved %d -> %d (both files are LF-only)' % (cr0, cr1))
            failed += 1
            continue
        print('  ok: %d -> %d bytes, %d -> %d lines, CR %d'
              % (n0, len(out), lf0, lf1, cr1))
        if not check_only:
            open(path, 'wb').write(out)
    if check_only:
        print('--check: nothing written')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
