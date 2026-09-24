#!/usr/bin/env python3
"""Lane WATER6 (run by BUILD10) -- the solver consumes per-point weights,
one-point pins and the imported raster layer; the flow PNG speaks DirectX.

Every anchor must match EXACTLY ONCE in the file's real bytes; every file is
LF-only and its CR count is asserted unchanged.  Run with --check first.
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HERE = os.path.dirname(os.path.abspath(__file__))
GATES = open(os.path.join(HERE, "w6_gates.cpp.txt"), "r", encoding="utf-8").read()

E = []


def after(path, anchor, text):
    E.append((path, "after", anchor, text))


def repl(path, anchor, text):
    E.append((path, "replace", anchor, text))


# ============================================================ src/watermark.h
after("src/watermark.h",
      '#define WATERMARK_STROKE_EXTRA 1\n',
      '\n'
      '/*! Lane WATER6: the solver consumes the window\'s per-point weights, a\n'
      ' *  ONE-POINT curve, and an imported raster layer as the authority where\n'
      ' *  painted (scratchpad/water5_20260910/CHANGE_NEEDED.md C1-C3). */\n'
      '#define WATERMARK_RASTER_AUTHORITY 1\n'
      '\n'
      '#include "watercurves.h"\n')

after("src/watermark.h",
      '\tQVector<WaterStroke> marks;\n',
      '\t/*! Lane WATER6 (C3): the kind-10 raster layers, decoded ONCE from the\n'
      '\t *  marks with lane WATER5\'s own decoder (never a second one) and asked\n'
      '\t *  by flowWordOf BEFORE the solved field.  `rasterSrc` is the payload of\n'
      '\t *  each cached layer: a read pass (sweep, packFlowPlane, flowWordAt)\n'
      '\t *  compares it with the marks and rebuilds only when it differs, so a\n'
      '\t *  layer that is added or removed between passes is seen and a fingerprint\n'
      '\t *  cannot collide. */\n'
      '\tmutable QVector<WaterRasterLayer> rasterCache;\n'
      '\tmutable QVector<QByteArray> rasterSrc;\n'
      '\tvoid syncRasters() const;\n')

# ========================================================== src/watermark.cpp
after("src/watermark.cpp",
      '#include "watermark.h"\n',
      '\n'
      '#include "watercurves.h"        // lane WATER6: WaterRasterLayer, the PNG codec\n')

after("src/watermark.cpp",
      '#include <QFileInfo>\n',
      '#include <QCoreApplication>\n'
      '#include <QImage>\n')

# ---- the raster cache -------------------------------------------------------
after("src/watermark.cpp",
      'quint16 WaterMarkDoc::flowWordOf( int px, int py, quint16 id ) const\n'
      '{\n'
      '\tif ( !id )\n'
      '\t\treturn 0;\n',
      '\t/* Lane WATER6 (C3): an imported raster layer is the AUTHORITY where it is\n'
      '\t * painted -- the last one painted wins.  The solve is untouched: this is a\n'
      '\t * question about the word the document WRITES, not about the field it\n'
      '\t * solved.  The cache is refreshed at the top of every read pass. */\n'
      '\tfor ( int i = rasterCache.size() - 1; i >= 0; i-- ) {\n'
      '\t\tquint16 w = 0;\n'
      '\t\tif ( rasterCache[i].wordAt( px, py, w ) )\n'
      '\t\t\treturn w;\n'
      '\t}\n')

after("src/watermark.cpp",
      'quint16 WaterMarkDoc::flowWordOf( int px, int py, quint16 id ) const\n',
      '')   # placeholder replaced below (kept so the ordering is explicit)
E.pop()

# syncRasters(), placed just before flowWordOf
after("src/watermark.cpp",
      'quint16 WaterMarkDoc::automaticWord( quint16 id ) const\n',
      '')
E.pop()

repl("src/watermark.cpp",
     'bool WaterMarkDoc::sweep( const std::function<void( int, int, quint16, quint16, quint16 )> & cb,\n'
     '\tQString * error ) const\n'
     '{\n',
     '/*! Lane WATER6: the kind-10 raster layers, decoded from the marks when they\n'
     ' *  differ from what is cached.  Called at the top of every read pass. */\n'
     'void WaterMarkDoc::syncRasters() const\n'
     '{\n'
     '\tQVector<QByteArray> now;\n'
     '\tfor ( const WaterStroke & s : marks )\n'
     '\t\tif ( s.kind == 10 && s.enabled() )\n'
     '\t\t\tnow.append( s.extra );\n'
     '\tif ( now == rasterSrc )\n'
     '\t\treturn;\n'
     '\trasterSrc = now;\n'
     '\trasterCache.clear();\n'
     '\tfor ( const QByteArray & p : now ) {\n'
     '\t\tWaterRasterLayer r;\n'
     '\t\tQString why;\n'
     '\t\tif ( r.fromPayload( p, &why ) )\n'
     '\t\t\trasterCache.append( r );\n'
     '\t}\n'
     '}\n'
     '\n'
     'bool WaterMarkDoc::sweep( const std::function<void( int, int, quint16, quint16, quint16 )> & cb,\n'
     '\tQString * error ) const\n'
     '{\n'
     '\tsyncRasters();\n')

repl("src/watermark.cpp",
     'QByteArray WaterMarkDoc::packFlowPlane( quint64 base, QString * error ) const\n'
     '{\n',
     'QByteArray WaterMarkDoc::packFlowPlane( quint64 base, QString * error ) const\n'
     '{\n'
     '\tsyncRasters();\n')

repl("src/watermark.cpp",
     'quint16 WaterMarkDoc::flowWordAt( int px, int py ) const\n'
     '{\n',
     'quint16 WaterMarkDoc::flowWordAt( int px, int py ) const\n'
     '{\n'
     '\tsyncRasters();\n')

# ---- C1: the per-point weights ---------------------------------------------
repl("src/watermark.cpp",
     '\tstruct Seg { double ax, ay, bx, by, halfW; };\n',
     '\t/* lane WATER6 (C1): `wa` / `wb` are the per-point speed weights of this\n'
     '\t * segment\'s two endpoints; 1 is "the curve\'s own speed". */\n'
     '\tstruct Seg { double ax, ay, bx, by, halfW; double wa = 1.0, wb = 1.0; };\n')

repl("src/watermark.cpp",
     '\t\tif ( s.kind == WaterStroke::Stroke || s.kind == WaterStroke::Pin ) {\n'
     '\t\t\tif ( s.pts.size() < 2 )\n'
     '\t\t\t\tcontinue;\n'
     '\t\t\tmaxWidth = std::max( maxWidth, double( s.width ) );\n'
     '\t\t\tfor ( int k = 0; k + 1 < s.pts.size(); k++ ) {\n'
     '\t\t\t\tsegs.append( Seg{ double( s.pts[k].x ), double( s.pts[k].y ),\n'
     '\t\t\t\t\tdouble( s.pts[k + 1].x ), double( s.pts[k + 1].y ), halfW } );\n',

     '\t\tif ( s.kind == WaterStroke::Stroke || s.kind == WaterStroke::Pin ) {\n'
     '\t\t\t/* lane WATER6 (C1): one float a point in the record\'s trailing bytes\n'
     '\t\t\t * (hook-up H2, lane WATER5) is that point\'s speed weight.  A record\n'
     '\t\t\t * with none, or with a value that is not a finite 0..64, reads 1 --\n'
     '\t\t\t * which is the unweighted solve, byte for byte (gate X1a). */\n'
     '\t\t\tauto weightAt = [&s]( int k ) -> double {\n'
     '\t\t\t\tif ( s.extra.size() < qsizetype( k + 1 ) * 4 )\n'
     '\t\t\t\t\treturn 1.0;\n'
     '\t\t\t\tfloat v = 1.0f;\n'
     '\t\t\t\tmemcpy( &v, s.extra.constData() + qsizetype( k ) * 4, 4 );\n'
     '\t\t\t\treturn ( std::isfinite( v ) && v >= 0.0f && v <= 64.0f ) ? double( v ) : 1.0;\n'
     '\t\t\t};\n'
     '\t\t\tif ( s.pts.size() < 2 ) {\n'
     '\t\t\t\t/* lane WATER6 (C2), bungo: "a one-point curve = a pin".  It carries\n'
     '\t\t\t\t * no direction, so it enters as a SOURCE disc of its own width --\n'
     '\t\t\t\t * the same treatment a stroke\'s FIRST point already gets.  A sink is\n'
     '\t\t\t\t * what the store\'s OutletPin (kind 5) already means. */\n'
     '\t\t\t\tmaxWidth = std::max( maxWidth, double( s.width ) );\n'
     '\t\t\t\tsrcPins.append( QPointF( s.pts.first().x, s.pts.first().y ) );\n'
     '\t\t\t\tgrow( s.pts.first().x, s.pts.first().y, halfW );\n'
     '\t\t\t\tif ( s.flags & WaterStroke::SetsSpeed ) {\n'
     '\t\t\t\t\tsumSpeed += double( s.speed );\n'
     '\t\t\t\t\tnSpeed++;\n'
     '\t\t\t\t}\n'
     '\t\t\t\tst.strokes++;\n'
     '\t\t\t\tcontinue;\n'
     '\t\t\t}\n'
     '\t\t\tmaxWidth = std::max( maxWidth, double( s.width ) );\n'
     '\t\t\tfor ( int k = 0; k + 1 < s.pts.size(); k++ ) {\n'
     '\t\t\t\tsegs.append( Seg{ double( s.pts[k].x ), double( s.pts[k].y ),\n'
     '\t\t\t\t\tdouble( s.pts[k + 1].x ), double( s.pts[k + 1].y ), halfW,\n'
     '\t\t\t\t\tweightAt( k ), weightAt( k + 1 ) } );\n')

repl("src/watermark.cpp",
     '\tstd::vector<quint8> held( n, 0 );\n'
     '\tstd::vector<float> boost( n, 1.0f );\n',
     '\tstd::vector<quint8> held( n, 0 );\n'
     '\tstd::vector<float> boost( n, 1.0f );\n'
     '\t/* lane WATER6 (C1): the weight of the NEAREST segment over this texel, 1\n'
     '\t * where no segment reaches.  It multiplies the speed before the body\'s\n'
     '\t * mean is taken, so a weighted reach reads faster without the direction\n'
     '\t * being asked to change. */\n'
     '\tstd::vector<float> wgt( n, 1.0f );\n'
     '\tstd::vector<float> wgtDist( n, 3.4e38f );\n')

repl("src/watermark.cpp",
     '\t\t\t\theld[at] = 1;\n'
     '\t\t\t\t/* a SMOOTH preference: a step in k refracts the flow at its edge,\n'
     '\t\t\t\t * and that edge was a seam in the prototype\'s picture */\n'
     '\t\t\t\tconst double q = 1.0 - ( dd / seg.halfW ) * ( dd / seg.halfW );\n'
     '\t\t\t\tboost[at] = float( std::max( double( boost[at] ), 1.0 + ( kStrokeBoost - 1.0 ) * q * q ) );\n',

     '\t\t\t\theld[at] = 1;\n'
     '\t\t\t\t/* a SMOOTH preference: a step in k refracts the flow at its edge,\n'
     '\t\t\t\t * and that edge was a seam in the prototype\'s picture */\n'
     '\t\t\t\tconst double q = 1.0 - ( dd / seg.halfW ) * ( dd / seg.halfW );\n'
     '\t\t\t\t/* lane WATER6 (C1): the weight interpolated along THIS segment.  At\n'
     '\t\t\t\t * w = 1 the expression below is the old one character for character,\n'
     '\t\t\t\t * which is what makes gate X1a a byte gate. */\n'
     '\t\t\t\tconst double segLen2 = ( seg.bx - seg.ax ) * ( seg.bx - seg.ax )\n'
     '\t\t\t\t\t+ ( seg.by - seg.ay ) * ( seg.by - seg.ay );\n'
     '\t\t\t\tconst double tt = segLen2 > 0.0\n'
     '\t\t\t\t\t? std::max( 0.0, std::min( 1.0, ( ( tx - seg.ax ) * ( seg.bx - seg.ax )\n'
     '\t\t\t\t\t\t+ ( ty - seg.ay ) * ( seg.by - seg.ay ) ) / segLen2 ) ) : 0.0;\n'
     '\t\t\t\tconst double sw = seg.wa + tt * ( seg.wb - seg.wa );\n'
     '\t\t\t\tif ( float( dd ) < wgtDist[at] ) {\n'
     '\t\t\t\t\twgtDist[at] = float( dd );\n'
     '\t\t\t\t\twgt[at] = float( sw );\n'
     '\t\t\t\t}\n'
     '\t\t\t\tboost[at] = float( std::max( double( boost[at] ),\n'
     '\t\t\t\t\t1.0 + ( kStrokeBoost * sw - 1.0 ) * q * q ) );\n')

repl("src/watermark.cpp",
     '\tdouble meanSpeed = 0.0;\n'
     '\tqint64 wet = 0;\n'
     '\tfor ( int i = 0; i < G.n; i++ ) {\n'
     '\t\tmeanSpeed += std::hypot( ux[size_t( i )], uy[size_t( i )] );\n'
     '\t\twet++;\n'
     '\t}\n',
     '\tdouble meanSpeed = 0.0;\n'
     '\tqint64 wet = 0;\n'
     '\tfor ( int i = 0; i < G.n; i++ ) {\n'
     '\t\tconst size_t wat = size_t( G.cy[size_t( i )] ) * size_t( W ) + size_t( G.cx[size_t( i )] );\n'
     '\t\tmeanSpeed += std::hypot( ux[size_t( i )], uy[size_t( i )] ) * double( wgt[wat] );\n'
     '\t\twet++;\n'
     '\t}\n')

repl("src/watermark.cpp",
     '\t\tconst size_t at = size_t( G.cy[size_t( i )] ) * size_t( W ) + size_t( G.cx[size_t( i )] );\n'
     '\t\tconst double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );\n'
     '\t\tmx += F->vx[at];\n',
     '\t\tconst size_t at = size_t( G.cy[size_t( i )] ) * size_t( W ) + size_t( G.cx[size_t( i )] );\n'
     '\t\tconst double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] ) * double( wgt[at] );\n'
     '\t\tmx += F->vx[at];\n')

# ---- the gates --------------------------------------------------------------
DIRS = ", ".join(str(i * 16) for i in range(16))
repl("src/watermark.cpp",
     '} // namespace\n'
     '\n'
     'bool lodtWaterMarkSelfTest( const QString & path, QString * text, QString * error )\n',

     '/*! The 16 directions of `tests/fixtures/flowmap_directx_4x4.png`, in image\n'
     ' *  reading order (row 0 is the image\'s TOP row).  The PNG was written from\n'
     ' *  the CONVENTION by `scratchpad/build10_20260910/make_directx_fixture.py`,\n'
     ' *  not by this program\'s encoder, so gate X5c can fail even when the codec\n'
     ' *  round-trips itself perfectly. */\n'
     'const int kDirectXFixtureDirs[16] = { ' + DIRS + ' };\n'
     '\n'
     + GATES +
     '\n'
     '} // namespace\n'
     '\n'
     'bool lodtWaterMarkSelfTest( const QString & path, QString * text, QString * error )\n')

# call the WATER6 gates LAST: they mutate the in-memory document and never save,
# so the file the P3 undo gate has just restored is left exactly as it is.
repl("src/watermark.cpp",
     '\tif ( text )\n'
     '\t\t*text = rep + QStringLiteral( "%1 checks, %2 failures\\n%3\\n" )\n',
     '\t/* ---- lane WATER6 -------------------------------------------------------\n'
     '\t * LAST, because these gates solve the river six times and clear the\n'
     '\t * strokes between; they never save, so the file the P3 undo gate has just\n'
     '\t * reproduced byte for byte is left exactly as it is. */\n'
     '\t{\n'
     '\t\tWaterStroke axis = axisStroke( rb );\n'
     '\t\tif ( axis.pts.size() >= 6 )\n'
     '\t\t\twaterWeightGates( doc, river, axis, check, say );\n'
     '\t\telse\n'
     '\t\t\tsay( QStringLiteral( "lane WATER6\'s gates need a centreline of at least six "\n'
     '\t\t\t\t"points; body %1 gave %2, so X1..X5 did not run" ).arg( river ).arg( axis.pts.size() ) );\n'
     '\t}\n'
     '\n'
     '\tif ( text )\n'
     '\t\t*text = rep + QStringLiteral( "%1 checks, %2 failures\\n%3\\n" )\n')


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "--check"
    state = {}
    ok = True
    for path, kind, anchor, text in E:
        full = os.path.join(ROOT, path)
        b = state.get(full)
        if b is None:
            b = open(full, "rb").read()
            state[full] = b
        a = anchor.encode("utf-8")
        c = b.count(a)
        print("%-8s %-22s count=%d CR=%d  %r" % (kind, path, c, b.count(b"\r"), anchor[:44]))
        if c != 1:
            ok = False
    if not ok:
        print("REFUSED: an anchor does not match exactly once")
        return 1
    if mode != "--apply":
        print("check ok (nothing written)")
        return 0
    for path, kind, anchor, text in E:
        full = os.path.join(ROOT, path)
        b = state[full]
        a = anchor.encode("utf-8")
        assert b.count(a) == 1, path
        t = text.encode("utf-8")
        state[full] = b.replace(a, (a + t) if kind == "after" else t)
    for full, b in state.items():
        old = open(full, "rb").read()
        assert b.count(b"\r") == old.count(b"\r") == 0, full
        open(full, "wb").write(b)
        print("wrote %-24s %d -> %d bytes, CR 0" % (os.path.relpath(full, ROOT), len(old), len(b)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
