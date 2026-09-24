# Two corrections measured on the 08:34 exe's own run (animws_done2.log):
#
#  1. gate (q) failed: the pose glyph's ON mean came back #f1f2f4 against
#     textBright #f2f3f5 -- one short on every channel, summed distance 3,
#     over the tolerance of 2.  The auto-key dot, which is solid, came back
#     exact.  The mean is taken over pixels with alpha only over half, and
#     un-premultiplying such a pixel costs up to 1 per channel, so "the mean
#     IS the token" has to be measured per channel, not summed.
#  2. gate (n) started SKIPping ("the COM row is scrolled out of the sheet")
#     on the same run.  A gate forces the state it measures: scroll the row
#     in the way gate (k) already does for the thigh row, and say so.
import io, os, sys

ROOT = "E:/Projects/NifskopeWWE_ui"
EDITS = []


def E(path, old, new):
    EDITS.append((path, old, new))


# ---------------------------------------------------------- 1. per channel
E("src/animworkspacetest.cpp",
  "\t\t\t\t\tauto dist = []( const QColor & a, const QColor & b ) {\n"
  "\t\t\t\t\t\tif ( !a.isValid() || !b.isValid() )\n"
  "\t\t\t\t\t\t\treturn -1;\n"
  "\t\t\t\t\t\treturn std::abs( a.red() - b.red() ) + std::abs( a.green() - b.green() ) + std::abs( a.blue() - b.blue() );\n"
  "\t\t\t\t\t};\n",
  "\t\t\t\t\tauto dist = []( const QColor & a, const QColor & b ) {\n"
  "\t\t\t\t\t\tif ( !a.isValid() || !b.isValid() )\n"
  "\t\t\t\t\t\t\treturn -1;\n"
  "\t\t\t\t\t\treturn std::abs( a.red() - b.red() ) + std::abs( a.green() - b.green() ) + std::abs( a.blue() - b.blue() );\n"
  "\t\t\t\t\t};\n"
  "\t\t\t\t\t/* The per-CHANNEL difference, which is what \"the mean ink IS the\n"
  "\t\t\t\t\t   token\" has to be measured with. The mean is taken over every pixel\n"
  "\t\t\t\t\t   whose alpha is over half, and un-premultiplying a half-transparent\n"
  "\t\t\t\t\t   pixel costs up to 1 per channel, so two identical inks can come back\n"
  "\t\t\t\t\t   3 apart in SUMMED RGB. Measured, on the 08:34 exe: the pose glyph is\n"
  "\t\t\t\t\t   a ring and is nearly all antialiased edge, and its ON mean read\n"
  "\t\t\t\t\t   #f1f2f4 against textBright #f2f3f5 -- one short on each channel --\n"
  "\t\t\t\t\t   while the auto-key dot, which is solid, read the token exactly. Per\n"
  "\t\t\t\t\t   channel the tolerance the ruling asks for (1-2) is a real gate; a\n"
  "\t\t\t\t\t   summed one would have to be loosened to 6 to say the same thing. */\n"
  "\t\t\t\t\tauto chMax = []( const QColor & a, const QColor & b ) {\n"
  "\t\t\t\t\t\tif ( !a.isValid() || !b.isValid() )\n"
  "\t\t\t\t\t\t\treturn 99;\n"
  "\t\t\t\t\t\treturn std::max( std::abs( a.red() - b.red() ),\n"
  "\t\t\t\t\t\t\t\t\t\t std::max( std::abs( a.green() - b.green() ), std::abs( a.blue() - b.blue() ) ) );\n"
  "\t\t\t\t\t};\n")

E("src/animworkspacetest.cpp",
  "\t\t\t\t\t\tconst int moved = dist( off, on ), toWhite = dist( on, litInk ), toPlain = dist( off, plainInk );\n"
  "\t\t\t\t\t\tlitLine += QStringLiteral( \" %1 off=%2 on=%3 moved=%4 fromWhite=%5 fromPlain=%6\" )\n"
  "\t\t\t\t\t\t\t.arg( QString::fromLatin1( nm ).mid( 6 ), off.name(), on.name() ).arg( moved ).arg( toWhite ).arg( toPlain );\n"
  "\t\t\t\t\t\tif ( moved < 20 )\n"
  "\t\t\t\t\t\t\tnotLit++;\n"
  "\t\t\t\t\t\tif ( toWhite < 0 || toWhite > 2 )\n"
  "\t\t\t\t\t\t\tnotWhite++;\n"
  "\t\t\t\t\t\tif ( toPlain < 0 || toPlain > 2 )\n"
  "\t\t\t\t\t\t\tnotPlain++;\n",
  "\t\t\t\t\t\tconst int moved = dist( off, on ), toWhite = chMax( on, litInk ), toPlain = chMax( off, plainInk );\n"
  "\t\t\t\t\t\tlitLine += QStringLiteral( \" %1 off=%2 on=%3 moved=%4 offWhiteMaxCh=%5 offPlainMaxCh=%6\" )\n"
  "\t\t\t\t\t\t\t.arg( QString::fromLatin1( nm ).mid( 6 ), off.name(), on.name() ).arg( moved ).arg( toWhite ).arg( toPlain );\n"
  "\t\t\t\t\t\tif ( moved < 20 )\n"
  "\t\t\t\t\t\t\tnotLit++;\n"
  "\t\t\t\t\t\tif ( toWhite > 2 )\n"
  "\t\t\t\t\t\t\tnotWhite++;\n"
  "\t\t\t\t\t\tif ( toPlain > 2 )\n"
  "\t\t\t\t\t\t\tnotPlain++;\n")

E("src/animworkspacetest.cpp",
  "\t\t\t\t * that merely changed its plate colour cannot pass. The two tokens\n"
  "\t\t\t\t * are only 33 apart in summed RGB, so the \"it moved\" floor is 20\n"
  "\t\t\t\t * and not the 60 the accent allowed; what keeps the pair honest is\n"
  "\t\t\t\t * that BOTH means must land within 2 of their own token.\n",
  "\t\t\t\t * that merely changed its plate colour cannot pass. The two tokens\n"
  "\t\t\t\t * are only 33 apart in summed RGB, so the \"it moved\" floor is 20\n"
  "\t\t\t\t * and not the 60 the accent allowed; what keeps the pair honest is\n"
  "\t\t\t\t * that BOTH means must land within 2 of their own token ON EVERY\n"
  "\t\t\t\t * CHANNEL -- see chMax below for why summing will not do here.\n")

E("src/animworkspacetest.cpp",
  "\"(q) and the lit ink is the palette's brightest white, textBright, not the accent (%1 off it by more than 2)\"",
  "\"(q) and the lit ink is the palette's brightest white, textBright, not the accent (%1 off it by more than 2 on a channel)\"")

E("src/animworkspacetest.cpp",
  "\"(q) while the unlit ink is still the plain text ink (%1 off it by more than 2)\"",
  "\"(q) while the unlit ink is still the plain text ink (%1 off it by more than 2 on a channel)\"")

# ------------------------------------------------- 2. (n) forces its state
E("src/animworkspacetest.cpp",
  "\t\t\t\t// the entry, in the place the ruling puts it: directly under \"Remove track\"\n"
  "\t\t\t\tQStringList nTexts;\n"
  "\t\t\t\tconst int cyN = sheet->rowCenterY( comRow );\n",
  "\t\t\t\t// the entry, in the place the ruling puts it: directly under \"Remove track\"\n"
  "\t\t\t\tQStringList nTexts;\n"
  "\t\t\t\t/* A gate FORCES the state it measures. The COM row can be scrolled out\n"
  "\t\t\t\t   of the sheet by whatever ran before it -- this started skipping the\n"
  "\t\t\t\t   day gate (k8b) was added, which grabs the sheet and pumps the event\n"
  "\t\t\t\t   loop several times over -- and a right-click that cannot reach the row\n"
  "\t\t\t\t   measures nothing. So scroll it in, exactly the way gate (k) already\n"
  "\t\t\t\t   does for the thigh row, and say whether that was needed. */\n"
  "\t\t\t\tif ( sheet->rowCenterY( comRow ) <= 0 ) {\n"
  "\t\t\t\t\tif ( auto * sbN = sheet->findChild<QScrollBar *>( QStringLiteral( \"AnimWsDopeSheetScroll\" ) ) ) {\n"
  "\t\t\t\t\t\tsbN->setValue( std::max( 0, int( sheet->visibleRows().indexOf( comRow ) ) ) );\n"
  "\t\t\t\t\t\tqApp->processEvents();\n"
  "\t\t\t\t\t}\n"
  "\t\t\t\t\tsay( *st, QStringLiteral( \"  (n) the COM row was scrolled out of the sheet; scrolled in to row index %1, centre y now %2\" )\n"
  "\t\t\t\t\t\t\t .arg( sheet->visibleRows().indexOf( comRow ) ).arg( sheet->rowCenterY( comRow ) ) );\n"
  "\t\t\t\t}\n"
  "\t\t\t\tconst int cyN = sheet->rowCenterY( comRow );\n")

# --------------------------------------------------------- 3. the spell doc
E("tests/spells/animws.sh",
  "#       toggle off and on, and each mean must land within 2 of the token it is\n"
  "#       meant to be: OFF on `text`, ON on `textBright`, the palette's brightest\n",
  "#       toggle off and on, and each mean must land within 2 ON EVERY CHANNEL of\n"
  "#       the token it is meant to be (per channel, because un-premultiplying an\n"
  "#       antialiased mean costs up to 1 a channel and the pose ring is nearly all\n"
  "#       edge): OFF on `text`, ON on `textBright`, the palette's brightest\n")

# ------------------------------------------------------------- 4. the skill
E(".claude/skills/ww-toggle-lit-gate/SKILL.md",
  "4. `dist( on, litToken ) <= 2` AND `dist( off, plainToken ) <= 2` -- both means\n"
  "   are pinned to the token they are meant to be, so neither an accent left\n"
  "   behind nor a plate colour bleeding into the mean can pass. Pinning both ends\n"
  "   is what lets check 3's floor be small;\n",
  "4. both means are pinned to the token they are meant to be -- within 2 ON EVERY\n"
  "   CHANNEL, `max(|dr|,|dg|,|db|) <= 2`, never a summed distance. The mean is\n"
  "   taken over pixels whose alpha is merely over half, and un-premultiplying one\n"
  "   of those costs up to 1 per channel, so a glyph that is nearly all antialiased\n"
  "   edge (a ring, a thin arrow) reads 3 away from its own token in summed RGB\n"
  "   while a solid one reads 0. Pinning both ends is what lets check 3's floor be\n"
  "   small; pinning them per channel is what lets the tolerance stay at 2;\n")


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
