# Ruling 08:2x (bungo, 2026-09-12), verbatim:
#   "also, grey out diamond keyframes out of animations start and end range,
#    to indicate they're not being taking into consideration anymore"
# Every key diamond whose frame is outside rangeStart()..rangeEnd() is painted
# in the dimmed form of the colour it would otherwise have -- plain, selected
# and active alike.  The dim is the darkened band's own arithmetic applied to
# the ink instead of the ground: `animOutOfRange` at Blender's alpha 155/255.
# Refusing patch: anchors exactly once, no CR anywhere, all-or-nothing write.
import io, os, sys

ROOT = "E:/Projects/NifskopeWWE_ui"
EDITS = []


def E(path, old, new):
    EDITS.append((path, old, new))


# =========================================================== the header
E("src/animdopesheet.h",
  "#include <QSet>\n",
  "#include <QColor>\n#include <QSet>\n")

E("src/animdopesheet.h",
  "\tint rangeStart() const;\n"
  "\tint rangeEnd() const;\n"
  "\tvoid setRange( int first, int last );\n",
  "\tint rangeStart() const;\n"
  "\tint rangeEnd() const;\n"
  "\tvoid setRange( int first, int last );\n"
  "\t/* bungo, 2026-09-12, ruling 08:2x, verbatim: \"also, grey out diamond\n"
  "\t * keyframes out of animations start and end range, to indicate they're not\n"
  "\t * being taking into consideration anymore\". A key outside\n"
  "\t * rangeStart()..rangeEnd() is painted in THIS dimmed form of whatever\n"
  "\t * colour it would otherwise have had -- plain, selected or active alike,\n"
  "\t * so a selected key that has fallen out of the range still reads as\n"
  "\t * selected while it says \"ignored\".\n"
  "\t *\n"
  "\t * The dim is the darkened band's own arithmetic applied to the INK instead\n"
  "\t * of the ground: the `animOutOfRange` token composited over the colour at\n"
  "\t * Blender's alpha (155/255, ANIM_draw_framerange), so a dimmed diamond\n"
  "\t * looks exactly as though the out-of-range wash had been painted across\n"
  "\t * it. Public and static because the harness measures the painted pixels\n"
  "\t * against it rather than re-deriving the constant. */\n"
  "\tstatic QColor dimOutOfRange( const QColor & c );\n")

# =========================================================== the painter
E("src/animdopesheet.cpp",
  "void AnimDopeSheet::setRange( int first, int last )\n"
  "{\n"
  "\trngStart = std::max( 0, std::min( frames - 1, first ) );\n"
  "\trngEnd = std::max( rngStart, std::min( frames - 1, last ) );\n"
  "\tupdate();\n"
  "}\n",
  "void AnimDopeSheet::setRange( int first, int last )\n"
  "{\n"
  "\trngStart = std::max( 0, std::min( frames - 1, first ) );\n"
  "\trngEnd = std::max( rngStart, std::min( frames - 1, last ) );\n"
  "\tupdate();\n"
  "}\n"
  "\n"
  "QColor AnimDopeSheet::dimOutOfRange( const QColor & c )\n"
  "{\n"
  "\t/* The band outside the range is `animOutOfRange` filled at alpha 155 over\n"
  "\t   the row's ground. This is that same composite, with the key's own colour\n"
  "\t   as the ground, so the diamond ends up the colour it would have been had\n"
  "\t   the wash been painted over it instead of under it. One arithmetic, one\n"
  "\t   token, and the harness calls this very function for its expectations. */\n"
  "\tconst QColor out = wwC( \"animOutOfRange\" );\n"
  "\tconst int a = 155;\n"
  "\tauto mix = []( int ink, int wash, int alpha ) {\n"
  "\t\treturn ( ink * ( 255 - alpha ) + wash * alpha + 127 ) / 255;\n"
  "\t};\n"
  "\treturn QColor( mix( c.red(), out.red(), a ), mix( c.green(), out.green(), a ), mix( c.blue(), out.blue(), a ) );\n"
  "}\n")

E("src/animdopesheet.cpp",
  "\t   It is painted band by band, each time right after that band's own\n"
  "\t   background and BEFORE its keys, rather than once over the finished\n"
  "\t   picture. Two reasons: a key outside the range then stays as bright as one\n"
  "\t   inside, which is what Blender does (ANIM_draw_framerange runs in the\n"
  "\t   background pass, not over the channels), and the label column keeps its\n"
  "\t   full contrast. */\n",
  "\t   It is painted band by band, each time right after that band's own\n"
  "\t   background and BEFORE its keys, rather than once over the finished\n"
  "\t   picture, so the label column keeps its full contrast.\n"
  "\n"
  "\t   THE KEYS ARE DIMMED SEPARATELY, and that is new. Painting the band under\n"
  "\t   the keys used to mean a key outside the range stayed as bright as one\n"
  "\t   inside, which is what Blender itself does (ANIM_draw_framerange runs in\n"
  "\t   the background pass, not over the channels). bungo overturned that on\n"
  "\t   2026-09-12, ruling 08:2x, verbatim: \"also, grey out diamond keyframes out\n"
  "\t   of animations start and end range, to indicate they're not being taking\n"
  "\t   into consideration anymore\". So the diamond painter below runs every\n"
  "\t   out-of-range key's colour through dimOutOfRange(), which applies THIS\n"
  "\t   band's arithmetic to the ink instead of to the ground. The band is\n"
  "\t   unchanged; only the diamonds are. */\n")

E("src/animdopesheet.cpp",
  "\t\t// keys as diamonds\n"
  "\t\tauto diamond = [&]( int x, bool selected, bool active, const QColor & c ) {\n"
  "\t\t\tQPainterPath d;\n"
  "\t\t\tconst int cy = y + rowH / 2;\n"
  "\t\t\tconst int s = selected ? 5 : 4;\n"
  "\t\t\td.moveTo( x, cy - s );\n"
  "\t\t\td.lineTo( x + s, cy );\n"
  "\t\t\td.lineTo( x, cy + s );\n"
  "\t\t\td.lineTo( x - s, cy );\n"
  "\t\t\td.closeSubpath();\n"
  "\t\t\tp.fillPath( d, active ? keyAct : ( selected ? keyOther : c ) );\n"
  "\t\t};\n",
  "\t\t/* keys as diamonds. RULING 08:2x: a key whose frame is outside\n"
  "\t\t   rangeStart()..rangeEnd() is filled with the dimmed form of the colour\n"
  "\t\t   it would otherwise have -- plain, selected and active alike, so a\n"
  "\t\t   selected key that has fallen out of the range still reads as selected\n"
  "\t\t   while it says \"ignored\". Nothing else about it changes: same size,\n"
  "\t\t   same shape, still hit-tested, still draggable. It follows the ruler\n"
  "\t\t   grips for free, because dragging one calls setRange(), which calls\n"
  "\t\t   update(). */\n"
  "\t\tauto diamond = [&]( int x, int frame, bool selected, bool active, const QColor & c ) {\n"
  "\t\t\tQPainterPath d;\n"
  "\t\t\tconst int cy = y + rowH / 2;\n"
  "\t\t\tconst int s = selected ? 5 : 4;\n"
  "\t\t\td.moveTo( x, cy - s );\n"
  "\t\t\td.lineTo( x + s, cy );\n"
  "\t\t\td.lineTo( x, cy + s );\n"
  "\t\t\td.lineTo( x - s, cy );\n"
  "\t\t\td.closeSubpath();\n"
  "\t\t\tconst QColor fill = active ? keyAct : ( selected ? keyOther : c );\n"
  "\t\t\tp.fillPath( d, ( frame < rs || frame > re ) ? dimOutOfRange( fill ) : fill );\n"
  "\t\t};\n")

E("src/animdopesheet.cpp",
  "\t\t\t\t\tdiamond( x, false, false, muted );\n",
  "\t\t\t\t\tdiamond( x, fr, false, false, muted );\n")

E("src/animdopesheet.cpp",
  "\t\t\t\tdiamond( x, selKeys.contains( ref ), actKey == ref,\n"
  "\t\t\t\t\t\t r.kind == AnimWsRow::Unbound ? muted : keyPlain );\n",
  "\t\t\t\tdiamond( x, k.frame, selKeys.contains( ref ), actKey == ref,\n"
  "\t\t\t\t\t\t r.kind == AnimWsRow::Unbound ? muted : keyPlain );\n")

E("src/animdopesheet.cpp",
  "\t\t\t\tdiamond( x, selKeys.contains( ref ), actKey == ref, keyPlain );\n",
  "\t\t\t\tdiamond( x, k.frame, selKeys.contains( ref ), actKey == ref, keyPlain );\n")

# =========================================================== the gate
E("src/animworkspacetest.cpp",
  "\t\t\t\t\t\tconst QColor gotAct = px( im, x20, cy );\n"
  "\t\t\t\t\t\tconst QColor gotOth = px( im, x30, cy );\n"
  "\t\t\t\t\t\tconst QColor gotKey = px( im, x40, cy );\n",
  "\t\t\t\t\t\tconst QColor gotAct = px( im, x20, cy );\n"
  "\t\t\t\t\t\tconst QColor gotOth = px( im, x30, cy );\n"
  "\t\t\t\t\t\tconst QColor gotKey = px( im, x40, cy );\n"
  "\t\t\t\t\t\t/* RULING 08:2x: a key outside the range is painted in the dimmed\n"
  "\t\t\t\t\t\t   form of its colour, so what a sample must equal depends on where\n"
  "\t\t\t\t\t\t   the pass put the range. Pass 2's range is 25..40, which leaves the\n"
  "\t\t\t\t\t\t   ACTIVE key at frame 20 outside it -- that is the case this\n"
  "\t\t\t\t\t\t   expectation exists for, and it is the painter's own function that\n"
  "\t\t\t\t\t\t   says what the colour should be. */\n"
  "\t\t\t\t\t\tauto outOf = [&]( int frame ) { return frame < ps.r0 || frame > ps.r1; };\n"
  "\t\t\t\t\t\tauto want = [&]( int frame, const QColor & base ) {\n"
  "\t\t\t\t\t\t\treturn outOf( frame ) ? AnimDopeSheet::dimOutOfRange( base ) : base;\n"
  "\t\t\t\t\t\t};\n"
  "\t\t\t\t\t\tauto how = [&]( int frame, const char * token ) {\n"
  "\t\t\t\t\t\t\treturn outOf( frame ) ? QStringLiteral( \"%1 GREYED (frame %2 is out of range)\" ).arg( QString::fromLatin1( token ) ).arg( frame )\n"
  "\t\t\t\t\t\t\t\t\t\t\t\t : QString::fromLatin1( token );\n"
  "\t\t\t\t\t\t};\n"
  "\t\t\t\t\t\tconst QColor wAct = want( 20, cAct ), wOth = want( 30, cOth ), wKey = want( 40, cKey );\n")

E("src/animworkspacetest.cpp",
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k2) %1: the ACTIVE selected key is animKeySel %2, got %3\" ).arg( pn, cAct.name(), gotAct.name() ),\n"
  "\t\t\t\t\t\t\t   gotAct == cAct );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k7b) %1: the OTHER selected key is animKeySelOther %2, got %3\" ).arg( pn, cOth.name(), gotOth.name() ),\n"
  "\t\t\t\t\t\t\t   gotOth == cOth );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k2) %1: an unselected key is plain animKey %2, got %3\" ).arg( pn, cKey.name(), gotKey.name() ),\n"
  "\t\t\t\t\t\t\t   gotKey == cKey );\n",
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k2) %1: the ACTIVE selected key is %2 %3, got %4\" ).arg( pn, how( 20, \"animKeySel\" ), wAct.name(), gotAct.name() ),\n"
  "\t\t\t\t\t\t\t   gotAct == wAct );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k7b) %1: the OTHER selected key is %2 %3, got %4\" ).arg( pn, how( 30, \"animKeySelOther\" ), wOth.name(), gotOth.name() ),\n"
  "\t\t\t\t\t\t\t   gotOth == wOth );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k2) %1: an unselected key is %2 %3, got %4\" ).arg( pn, how( 40, \"animKey\" ), wKey.name(), gotKey.name() ),\n"
  "\t\t\t\t\t\t\t   gotKey == wKey );\n")

E("src/animworkspacetest.cpp",
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8 floor) range 0..%1: frame 5 and frame 40 are the SAME colour %2 / %3 -- nothing darkened\" ).arg( sheet->numFrames() - 1 ).arg( a.name(), bIn.name() ),\n"
  "\t\t\t\t\t\t\t   a == bIn && a == cRowSel );\n"
  "\t\t\t\t\t}\n",
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8 floor) range 0..%1: frame 5 and frame 40 are the SAME colour %2 / %3 -- nothing darkened\" ).arg( sheet->numFrames() - 1 ).arg( a.name(), bIn.name() ),\n"
  "\t\t\t\t\t\t\t   a == bIn && a == cRowSel );\n"
  "\t\t\t\t\t}\n"
  "\t\t\t\t\t/* ---- (k8b) RULING 08:2x, bungo verbatim: \"also, grey out diamond\n"
  "\t\t\t\t\t * keyframes out of animations start and end range, to indicate\n"
  "\t\t\t\t\t * they're not being taking into consideration anymore\".\n"
  "\t\t\t\t\t *\n"
  "\t\t\t\t\t * The fixture keys every frame of 93, so frames 5, 30 and 80 are all\n"
  "\t\t\t\t\t * real diamonds. With the range at 10..50, 5 and 80 are outside it and\n"
  "\t\t\t\t\t * 30 is inside; every sample is compared to the EXACT colour the\n"
  "\t\t\t\t\t * painter's own dimOutOfRange() gives, not to \"darker than\", so a wash\n"
  "\t\t\t\t\t * of the wrong strength fails too. Then End is moved out to 90 and\n"
  "\t\t\t\t\t * frame 80 must come back plain with no reload -- the \"the moment the\n"
  "\t\t\t\t\t * grips move\" half of the ruling. Selection is checked in the same\n"
  "\t\t\t\t\t * place: a selected key that falls out of the range keeps its own\n"
  "\t\t\t\t\t * colour, dimmed the same way.\n"
  "\t\t\t\t\t *\n"
  "\t\t\t\t\t * THE FLOOR: with the range back over the whole clip the very same\n"
  "\t\t\t\t\t * three samples must all be plain, so the comparison is shown able to\n"
  "\t\t\t\t\t * go the other way on the same run. */\n"
  "\t\t\t\t\t{\n"
  "\t\t\t\t\t\tconst QColor dimKey = AnimDopeSheet::dimOutOfRange( cKey );\n"
  "\t\t\t\t\t\tsheet->frameAll();\n"
  "\t\t\t\t\t\tsheet->clearSelection();\n"
  "\t\t\t\t\t\tsheet->setRange( 10, 50 );\n"
  "\t\t\t\t\t\tsheet->setCurrentFrame( 60 );   // the playhead is a 2 px line: park it off every sample\n"
  "\t\t\t\t\t\tqApp->processEvents();\n"
  "\t\t\t\t\t\tauto keyAt = [&]( float frame ) {\n"
  "\t\t\t\t\t\t\tconst QImage im2 = sheet->grab().toImage();\n"
  "\t\t\t\t\t\t\treturn px( im2, int( sheet->frameToX( frame ) ), cy );\n"
  "\t\t\t\t\t\t};\n"
  "\t\t\t\t\t\tconst QColor before5 = keyAt( 5.0f ), in30 = keyAt( 30.0f ), before80 = keyAt( 80.0f );\n"
  "\t\t\t\t\t\tsay( *st, QStringLiteral( \"  (k8b) range 10..50: frame 5 %1, frame 30 %2, frame 80 %3 (plain animKey %4, greyed %5)\" )\n"
  "\t\t\t\t\t\t\t\t .arg( before5.name(), in30.name(), before80.name(), cKey.name(), dimKey.name() ) );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8b) the key at frame 5, BEFORE the range, is greyed: %1 == %2\" ).arg( before5.name(), dimKey.name() ),\n"
  "\t\t\t\t\t\t\t   before5 == dimKey );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8b) the key at frame 80, AFTER the range, is greyed: %1 == %2\" ).arg( before80.name(), dimKey.name() ),\n"
  "\t\t\t\t\t\t\t   before80 == dimKey );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8b) the key at frame 30, INSIDE it, is untouched plain animKey: %1 == %2\" ).arg( in30.name(), cKey.name() ),\n"
  "\t\t\t\t\t\t\t   in30 == cKey );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8b) greyed is darker than plain, and %1 apart in R+G+B (>= 60)\" ).arg( chanDist( dimKey, cKey ) ),\n"
  "\t\t\t\t\t\t\t   dimKey.lightness() < cKey.lightness() && chanDist( dimKey, cKey ) >= 60 );\n"
  "\t\t\t\t\t\t// the grips move -> the diamonds follow, with no reload\n"
  "\t\t\t\t\t\tsheet->setRange( 10, 90 );\n"
  "\t\t\t\t\t\tqApp->processEvents();\n"
  "\t\t\t\t\t\tconst QColor after80 = keyAt( 80.0f ), after5 = keyAt( 5.0f );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8b) End moved to 90: frame 80 is plain again, %1 -> %2\" ).arg( before80.name(), after80.name() ),\n"
  "\t\t\t\t\t\t\t   after80 == cKey && before80 != after80 );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8b) ... and frame 5, still before the start, is still greyed: %1\" ).arg( after5.name() ),\n"
  "\t\t\t\t\t\t\t   after5 == dimKey );\n"
  "\t\t\t\t\t\t// a SELECTED key that has fallen out of the range keeps its colour, dimmed\n"
  "\t\t\t\t\t\tsheet->setRange( 10, 50 );\n"
  "\t\t\t\t\t\tsheet->selectKeys( { HkxKeyRef{ thigh, 5 }, HkxKeyRef{ thigh, 80 } }, HkxKeyRef{ thigh, 5 } );\n"
  "\t\t\t\t\t\tqApp->processEvents();\n"
  "\t\t\t\t\t\tconst QColor selOut = keyAt( 5.0f ), othOut = keyAt( 80.0f );\n"
  "\t\t\t\t\t\tconst QColor wantSel = AnimDopeSheet::dimOutOfRange( cAct ), wantOth = AnimDopeSheet::dimOutOfRange( cOth );\n"
  "\t\t\t\t\t\tsay( *st, QStringLiteral( \"  (k8b) selected AND out of range: active %1 (want %2), other %3 (want %4)\" )\n"
  "\t\t\t\t\t\t\t\t .arg( selOut.name(), wantSel.name(), othOut.name(), wantOth.name() ) );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8b) the ACTIVE selected key keeps animKeySel, greyed the same way: %1 == %2\" ).arg( selOut.name(), wantSel.name() ),\n"
  "\t\t\t\t\t\t\t   selOut == wantSel );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8b) the other selected key keeps animKeySelOther, greyed the same way: %1 == %2\" ).arg( othOut.name(), wantOth.name() ),\n"
  "\t\t\t\t\t\t\t   othOut == wantOth );\n"
  "\t\t\t\t\t\tcheck( *st, \"(k8b) and greyed does not flatten the three states into one\",\n"
  "\t\t\t\t\t\t\t   selOut != othOut && othOut != dimKey && selOut != dimKey );\n"
  "\t\t\t\t\t\tif ( !st->outDir.isEmpty() ) {\n"
  "\t\t\t\t\t\t\tconst QPixmap pm = sheet->grab();\n"
  "\t\t\t\t\t\t\tconst QString shot = st->outDir + QStringLiteral( \"/sheet_range_dim.png\" );\n"
  "\t\t\t\t\t\t\tcheck( *st, QStringLiteral( \"picture: (k8b) the sheet at 1:1, range 10..50, greyed keys either side, %1x%2 -> %3\" ).arg( pm.width() ).arg( pm.height() ).arg( shot ),\n"
  "\t\t\t\t\t\t\t\t   pm.save( shot ) );\n"
  "\t\t\t\t\t\t}\n"
  "\t\t\t\t\t\t// THE FLOOR\n"
  "\t\t\t\t\t\tsheet->clearSelection();\n"
  "\t\t\t\t\t\tsheet->setRange( 0, sheet->numFrames() - 1 );\n"
  "\t\t\t\t\t\tqApp->processEvents();\n"
  "\t\t\t\t\t\tconst QColor all5 = keyAt( 5.0f ), all30 = keyAt( 30.0f ), all80 = keyAt( 80.0f );\n"
  "\t\t\t\t\t\tcheck( *st, QStringLiteral( \"(k8b floor) range 0..%1: the SAME three samples are all plain animKey -- %2 / %3 / %4\" ).arg( sheet->numFrames() - 1 ).arg( all5.name(), all30.name(), all80.name() ),\n"
  "\t\t\t\t\t\t\t   all5 == cKey && all30 == cKey && all80 == cKey );\n"
  "\t\t\t\t\t}\n")

# =========================================================== the spell doc
E("tests/spells/animws.sh",
  "#       darkened and inside untouched -- at two zoom levels, with the old\n"
  "#       colour pair shown failing the same contrast test\n",
  "#       darkened and inside untouched -- at two zoom levels, with the old\n"
  "#       colour pair shown failing the same contrast test\n"
  "#  (k8b) KEYS OUTSIDE THE RANGE ARE GREYED (ruling 08:2x, bungo verbatim:\n"
  "#       \"also, grey out diamond keyframes out of animations start and end\n"
  "#       range, to indicate they're not being taking into consideration\n"
  "#       anymore\"). With the range at 10..50 on the 93-frame fixture the\n"
  "#       diamonds at frames 5 and 80 must equal AnimDopeSheet::dimOutOfRange()\n"
  "#       of the plain ink EXACTLY -- the `animOutOfRange` token at Blender's\n"
  "#       own alpha 155/255 laid over the ink, the same arithmetic the darkened\n"
  "#       band does to the ground -- while frame 30 stays plain. End is then\n"
  "#       moved to 90 and frame 80 must be plain again with no reload (the \"the\n"
  "#       moment the grips move\" half), and a SELECTED key out of range must\n"
  "#       keep its orange / orange-red, greyed the same way. The two zoom passes\n"
  "#       of (k) carry the same expectation, which is why pass 2's range of\n"
  "#       25..40 leaves its ACTIVE key at frame 20 greyed. PICTURE:\n"
  "#       sheet_range_dim.png, the sheet at 1:1 with that range. FLOOR: with the\n"
  "#       range over the whole clip the same three samples are all plain again\n")


def main():
    byfile = {}
    for rel, old, new in EDITS:
        byfile.setdefault(rel, []).append((old, new))

    out, bad = {}, []
    for rel, edits in byfile.items():
        p = os.path.join(ROOT, rel)
        with io.open(p, "rb") as f:
            raw = f.read()
        if raw.count(b"\r"):
            bad.append("%s carries %d CR bytes" % (rel, raw.count(b"\r")))
            continue
        s = raw.decode("utf-8")
        for old, new in edits:
            n = s.count(old)
            if n != 1:
                bad.append("%s: anchor found %d times (want 1): %r" % (rel, n, old[:70]))
                continue
            s = s.replace(old, new, 1)
        out[rel] = s.encode("utf-8")

    if bad:
        for b in bad:
            sys.stderr.write("REFUSED: " + b + "\n")
        sys.exit(1)

    for rel, data in out.items():
        assert b"\r" not in data, rel
        with io.open(os.path.join(ROOT, rel), "wb") as f:
            f.write(data)
        print("%-44s %8d B  LF %d" % (rel, len(data), data.count(b"\n")))


main()
