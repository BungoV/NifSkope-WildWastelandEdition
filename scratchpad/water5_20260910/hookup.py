#!/usr/bin/env python3
"""Lane WATER5's hook-ups, NOT APPLIED by the lane (the brief's file rule).

    python scratchpad/water5_20260910/hookup.py --check     # count every anchor, write nothing
    python scratchpad/water5_20260910/hookup.py --apply     # apply all, refusing unless every anchor matches once

Every edit is an insertion after (or a replacement of) an anchor that must
match EXACTLY ONCE in the file's real bytes; the CR count of every file must
be 0 before and after (src/ and NifSkope.pro are LF-only, measured by byte
count).  Run --check first; it says which anchors would refuse.

H1  NifSkope.pro         the four new paths (two headers, two sources)
H2  src/watermark.h      WaterStroke::extra + #define WATERMARK_STROKE_EXTRA
    src/watermark.cpp    the codec keeps a record's trailing bytes; addStroke accepts kind 10
H3  src/watermarkpanel.cpp  #include, the canvas hidden, the "Water window" button, waterWindowInstall()
"""
import sys, os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

EDITS = [
    # ---- H1: the .pro -------------------------------------------------------
    ("NifSkope.pro", "after",
     "\tsrc/watermarkpanel.h \\\n",
     "\tsrc/watercurves.h \\\n\tsrc/waterwindow.h \\\n"),
    ("NifSkope.pro", "after",
     "\tsrc/watermarkpanel.cpp \\\n",
     "\tsrc/watercurves.cpp \\\n\tsrc/waterwindow.cpp \\\n"),
    # ---- H2: the store keeps a record's trailing bytes ---------------------
    ("src/watermark.h", "after",
     "#include \"lodtfile.h\"\n",
     "\n/*! Lane WATER5 (hook-up H2): WaterStroke carries a record's trailing bytes.\n"
     " *  watercurves.cpp and waterwindow.cpp compile with or without this. */\n"
     "#define WATERMARK_STROKE_EXTRA 1\n"),
    ("src/watermark.h", "after",
     "\tquint8 colour[4] = { 0, 0, 0, 0 };   //!< DyePin only: RGBA of the dye\n"
     "\tQVector<WaterStrokePoint> pts;\n",
     "\t/*! Lane WATER5 (hook-up H2): the bytes of the record AFTER its points (and\n"
     "\t *  after a DyePin's colour), kept verbatim through open and save.  A curve\n"
     "\t *  (kind 0/1) carries one float a point here, the per-point speed weight;\n"
     "\t *  a raster layer (kind 10) carries its payload (watercurves.h).  Empty for\n"
     "\t *  every kind that has none, so an old file re-encodes to its own bytes. */\n"
     "\tQByteArray extra;\n"),
    ("src/watermark.cpp", "replace",
     "\t\tif ( s.kind == WaterStroke::DyePin && 20 + qsizetype( n ) * 8 + 4 <= qsizetype( recBytes ) )\n"
     "\t\t\tfor ( int c = 0; c < 4; c++ )\n"
     "\t\t\t\ts.colour[c] = quint8( p[20 + n * 8 + c] );\n"
     "\t\tmarks.append( s );\n",
     "\t\tif ( s.kind == WaterStroke::DyePin && 20 + qsizetype( n ) * 8 + 4 <= qsizetype( recBytes ) )\n"
     "\t\t\tfor ( int c = 0; c < 4; c++ )\n"
     "\t\t\t\ts.colour[c] = quint8( p[20 + n * 8 + c] );\n"
     "\t\t{\n"
     "\t\t\t// lane WATER5: whatever follows is kept verbatim (weights, a raster payload)\n"
     "\t\t\tconst qsizetype base = 20 + qsizetype( n ) * 8 + ( s.kind == WaterStroke::DyePin ? 4 : 0 );\n"
     "\t\t\tif ( qsizetype( recBytes ) > base )\n"
     "\t\t\t\ts.extra = QByteArray( p + base, int( qsizetype( recBytes ) - base ) );\n"
     "\t\t}\n"
     "\t\tmarks.append( s );\n"),
    ("src/watermark.cpp", "replace",
     "\t\tconst quint32 recBytes = quint32( 20 + m.pts.size() * 8\n"
     "\t\t\t+ ( m.kind == WaterStroke::DyePin ? 4 : 0 ) );\n",
     "\t\tconst quint32 recBytes = quint32( 20 + m.pts.size() * 8\n"
     "\t\t\t+ ( m.kind == WaterStroke::DyePin ? 4 : 0 ) + m.extra.size() );\n"),
    ("src/watermark.cpp", "replace",
     "\t\tif ( m.kind == WaterStroke::DyePin )\n"
     "\t\t\tfor ( int c = 0; c < 4; c++ )\n"
     "\t\t\t\tr.u8( m.colour[c] );\n"
     "\t\ts.b.append( r.b );\n",
     "\t\tif ( m.kind == WaterStroke::DyePin )\n"
     "\t\t\tfor ( int c = 0; c < 4; c++ )\n"
     "\t\t\t\tr.u8( m.colour[c] );\n"
     "\t\tr.b.append( m.extra );          // lane WATER5: the trailing bytes, verbatim\n"
     "\t\ts.b.append( r.b );\n"),
    ("src/watermark.cpp", "replace",
     "\tif ( in.pts.isEmpty() ) {\n"
     "\t\tsay( QStringLiteral( \"that stroke has no points\" ) );\n"
     "\t\treturn false;\n"
     "\t}\n"
     "\tWaterStroke s = in;\n",
     "\tif ( in.kind == 10 ) {\n"
     "\t\t/* A raster layer (lane WATER5, watercurves.h): no points and no body,\n"
     "\t\t * the payload is the mark.  The solve skips it (no points) until\n"
     "\t\t * CHANGE_NEEDED H3 teaches it authority where painted. */\n"
     "\t\tmarks.append( in );\n"
     "\t\tdirty = true;\n"
     "\t\tsay( QStringLiteral( \"raster layer stored (%1 bytes)\" ).arg( in.extra.size() ) );\n"
     "\t\treturn true;\n"
     "\t}\n"
     "\tif ( in.pts.isEmpty() ) {\n"
     "\t\tsay( QStringLiteral( \"that stroke has no points\" ) );\n"
     "\t\treturn false;\n"
     "\t}\n"
     "\tWaterStroke s = in;\n"),
    # ---- H3: the dock keeps a button; its canvas goes away ------------------
    ("src/watermarkpanel.cpp", "after",
     "#include \"watermark.h\"\n",
     "#include \"waterwindow.h\"\n"),
    ("src/watermarkpanel.cpp", "after",
     "\t\tcanvas = new WaterMarkCanvas( this, splitter );\n"
     "\t\tsplitter->addWidget( canvas );\n",
     "\t\t/* bungo, 2026-09-10: \"do you draw it on that tiny map?\" -- the tiny map\n"
     "\t\t * goes away.  The canvas stays as the model's hands for the harness (its\n"
     "\t\t * layStroke) and is never shown; the map lives in the water window. */\n"
     "\t\tcanvas->hide();\n"),
    ("src/watermarkpanel.cpp", "after",
     "\t\tbh->addStretch( 1 );\n",
     "\t\tauto * windowButton = new QPushButton( tr( \"Water window\" ), bar );\n"
     "\t\twindowButton->setObjectName( QStringLiteral( \"WaterMarkWindowButton\" ) );\n"
     "\t\twindowButton->setToolTip( tr( \"Open the water window: the whole flow map, the curves and every\\n\"\n"
     "\t\t\t\"marking tool, draggable, resizable and full-screen on F11\" ) );\n"
     "\t\tconnect( windowButton, &QPushButton::clicked, this, [this]() {\n"
     "\t\t\tQMainWindow * mw = nullptr;\n"
     "\t\t\tfor ( QWidget * w = parentWidget(); w; w = w->parentWidget() )\n"
     "\t\t\t\tif ( ( mw = qobject_cast<QMainWindow *>( w ) ) )\n"
     "\t\t\t\t\tbreak;\n"
     "\t\t\twaterWindowOpen( mw, doc ? doc->path() : fileEdit->text() );\n"
     "\t\t} );\n"
     "\t\tbh->addWidget( windowButton, 0 );\n"),
    ("src/watermarkpanel.cpp", "after",
     "\tmw->addDockWidget( Qt::RightDockWidgetArea, dock );\n"
     "\tdock->hide();\n",
     "\twaterWindowInstall( mw );      // lane WATER5: the Workspaces entry and WW_WATER_WINDOW_TEST\n"),
]


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "--check"
    files = {}
    ok = True
    for path, kind, anchor, text in EDITS:
        full = os.path.join(ROOT, path)
        if full not in files:
            files[full] = open(full, "rb").read()
        b = files[full]
        n = b.count(anchor.encode("utf-8"))
        cr = b.count(b"\r")
        print("%-8s %-26s anchor %r... count=%d CR=%d" % (kind, path, anchor[:40], n, cr))
        if n != 1 or cr != 0:
            ok = False
    # applied already?  The "after" anchors still match once after the edit, so the
    # answer comes from the marker every inserted text carries, never the anchor.
    for full, b in files.items():
        marks = b.count(b"lane WATER5")
        print("%-26s marker 'lane WATER5' x%d %s" % (os.path.relpath(full, ROOT), marks,
              "(APPLIED ALREADY)" if marks else "(not applied)"))
        if marks:
            ok = False
    if not ok:
        print("REFUSED: an anchor does not match exactly once, a file carries CR, or the edits are applied already")
        return 1
    if mode != "--apply":
        print("check ok (nothing written)")
        return 0
    for path, kind, anchor, text in EDITS:
        full = os.path.join(ROOT, path)
        b = files[full]
        a = anchor.encode("utf-8")
        t = text.encode("utf-8")
        assert b.count(a) == 1, path
        if kind == "after":
            b = b.replace(a, a + t)
        else:
            b = b.replace(a, t)
        files[full] = b
    for full, b in files.items():
        assert b.count(b"\r") == 0, full
        open(full, "wb").write(b)
        print("wrote %s (%d bytes, CR=0)" % (os.path.relpath(full, ROOT), len(b)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
