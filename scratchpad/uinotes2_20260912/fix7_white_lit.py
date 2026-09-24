# Ruling 07:3x (bungo, 2026-09-12, over icons_before_after.png):
#   "Why do these turn yellow when selected? the buttons, keep them white"
# The ON state of the two gizmo toggles stops being `accent` and becomes
# `textBright`, the palette's brightest ink token.  Refusing patch: every
# anchor must appear EXACTLY once, no file may carry a CR, and nothing is
# written unless every edit in the run resolved.
import io, os, sys

ROOT = "E:/Projects/NifskopeWWE_ui"

EDITS = []  # (relpath, old, new)


def E(path, old, new):
    EDITS.append((path, old, new))


# --------------------------------------------------------------- A. the icon
E("src/animworkspace.cpp",
  " *  off and in the palette's `accent` while it is on.\n",
  " *  off and in the palette's brightest ink, `textBright`, while it is on.\n")

E("src/animworkspace.cpp",
  " *  is on. `accent` is the palette's own name for \"active toggle\" (skinVars,\n"
  " *  nifskope_ui.cpp), and `accentDisabled` is its inert twin, so a toggle that\n"
  " *  is ON but cannot be clicked does not read as OFF.\n",
  " *  is on. The first cut lit them in `accent`, and bungo overruled that at\n"
  " *  07:3x over icons_before_after.png: \"Why do these turn yellow when\n"
  " *  selected? the buttons, keep them white\". So ON is `textBright` (#f2f3f5),\n"
  " *  the brightest ink token in skinVars (nifskope_ui.cpp), and OFF stays\n"
  " *  `text` (#e6e8eb). That is a step of 33 in summed RGB out of a possible\n"
  " *  765 -- deliberately small, because the state signal the eye reads is the\n"
  " *  QSS `:checked` plate under the glyph (`bgBtnDown`, wwBoxedButtonQss) and\n"
  " *  the ink only has to stop contradicting it. Disabled/On is `text`:\n"
  " *  brighter than the `textMuted` a disabled OFF toggle draws, so a toggle\n"
  " *  that is ON but cannot be clicked does not read as OFF, and dimmer than\n"
  " *  the enabled white, so it still reads as disabled.\n")

E("src/animworkspace.cpp",
  "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"accent\" ) ) ), QIcon::Normal, QIcon::On );\n",
  "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"textBright\" ) ) ), QIcon::Normal, QIcon::On );\n")

E("src/animworkspace.cpp",
  "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"accentDisabled\" ) ) ), QIcon::Disabled, QIcon::On );\n",
  "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"text\" ) ) ), QIcon::Disabled, QIcon::On );\n")

E("src/animworkspace.cpp",
  "\t   same 16x16 grid, lit in the accent while the mode is on. */\n",
  "\t   same 16x16 grid, lit in `textBright` while the mode is on (07:3x: the\n"
  "\t   lit ink was the accent for one build, and bungo asked for white). */\n")

# the loop note quotes numbers measured off transport_2x_on.png, which ruling
# 07:3x retakes; reword so the note states the fact and not the stale figures.
E("src/animworkspace.cpp",
  "\t\t   time that popup refreshes: in transport_2x_on.png (2026-09-12 06:35)\n"
  "\t\t   the checked loop button's ink measures #4d5761, plain ink over the\n"
  "\t\t   checked blue, while pose and auto-key measure #61584b and #665847, the\n"
  "\t\t   accent. Making loop light means changing the RENDER TOOLBAR's own loop\n",
  "\t\t   time that popup refreshes: in transport_2x_on.png the checked loop\n"
  "\t\t   button's ink is the PLAIN ink over the checked blue, unmoved from its\n"
  "\t\t   unchecked ink, while pose's and auto-key's does move. The harness\n"
  "\t\t   prints all three every run -- gate (q), the \"gizmo toggles' ink\" line\n"
  "\t\t   and the Loop say() beside it. Making loop light means changing the\n"
  "\t\t   RENDER TOOLBAR's own loop\n")

# --------------------------------------------------------------- B. the gate
E("src/animworkspacetest.cpp",
  "\t\t\t\t * \"Lit\" is measured, not asserted: the mean colour of the icon's\n"
  "\t\t\t\t * own ink (the pixels with alpha over half) is taken in both\n"
  "\t\t\t\t * states and compared, and the ON mean is compared to the palette's\n"
  "\t\t\t\t * `accent`, so a button that merely changed its plate colour\n"
  "\t\t\t\t * cannot pass. THE FLOOR is on the same line: Stop has no On\n",
  "\t\t\t\t * \"Lit\" is measured, not asserted: the mean colour of the icon's\n"
  "\t\t\t\t * own ink (the pixels with alpha over half) is taken in both\n"
  "\t\t\t\t * states, and each mean is compared to the token it is meant to\n"
  "\t\t\t\t * be -- OFF to `text`, ON to `textBright`, since bungo's 07:3x\n"
  "\t\t\t\t * ruling (\"keep them white\") replaced the accent -- so a button\n"
  "\t\t\t\t * that merely changed its plate colour cannot pass. The two tokens\n"
  "\t\t\t\t * are only 33 apart in summed RGB, so the \"it moved\" floor is 20\n"
  "\t\t\t\t * and not the 60 the accent allowed; what keeps the pair honest is\n"
  "\t\t\t\t * that BOTH means must land within 2 of their own token.\n"
  "\t\t\t\t * THE FLOOR is on the same line: Stop has no On\n")

E("src/animworkspacetest.cpp",
  "\t\t\t\t\tconst QColor accent( wwSkinColor( \"accent\" ) );\n"
  "\t\t\t\t\tconst char * toggles[2] = { \"AnimWsPose\", \"AnimWsAutoKey\" };\n"
  "\t\t\t\t\tint notIcon = 0, wrongSize = 0, notLit = 0, notAccent = 0;\n",
  "\t\t\t\t\tconst QColor litInk( wwSkinColor( \"textBright\" ) ), plainInk( wwSkinColor( \"text\" ) );\n"
  "\t\t\t\t\tconst char * toggles[2] = { \"AnimWsPose\", \"AnimWsAutoKey\" };\n"
  "\t\t\t\t\tint notIcon = 0, wrongSize = 0, notLit = 0, notWhite = 0, notPlain = 0;\n")

E("src/animworkspacetest.cpp",
  "\t\t\t\t\t\tconst int moved = dist( off, on ), toAccent = dist( on, accent );\n"
  "\t\t\t\t\t\tlitLine += QStringLiteral( \" %1 off=%2 on=%3 moved=%4 fromAccent=%5\" )\n"
  "\t\t\t\t\t\t\t.arg( QString::fromLatin1( nm ).mid( 6 ), off.name(), on.name() ).arg( moved ).arg( toAccent );\n"
  "\t\t\t\t\t\tif ( moved < 60 )\n"
  "\t\t\t\t\t\t\tnotLit++;\n"
  "\t\t\t\t\t\tif ( toAccent < 0 || toAccent > 40 )\n"
  "\t\t\t\t\t\t\tnotAccent++;\n"
  "\t\t\t\t\t}\n"
  "\t\t\t\t\tsay( *st, QStringLiteral( \"  (q) the gizmo toggles' ink:%1 (accent %2)\" ).arg( litLine, accent.name() ) );\n",
  "\t\t\t\t\t\tconst int moved = dist( off, on ), toWhite = dist( on, litInk ), toPlain = dist( off, plainInk );\n"
  "\t\t\t\t\t\tlitLine += QStringLiteral( \" %1 off=%2 on=%3 moved=%4 fromWhite=%5 fromPlain=%6\" )\n"
  "\t\t\t\t\t\t\t.arg( QString::fromLatin1( nm ).mid( 6 ), off.name(), on.name() ).arg( moved ).arg( toWhite ).arg( toPlain );\n"
  "\t\t\t\t\t\tif ( moved < 20 )\n"
  "\t\t\t\t\t\t\tnotLit++;\n"
  "\t\t\t\t\t\tif ( toWhite < 0 || toWhite > 2 )\n"
  "\t\t\t\t\t\t\tnotWhite++;\n"
  "\t\t\t\t\t\tif ( toPlain < 0 || toPlain > 2 )\n"
  "\t\t\t\t\t\t\tnotPlain++;\n"
  "\t\t\t\t\t}\n"
  "\t\t\t\t\tsay( *st, QStringLiteral( \"  (q) the gizmo toggles' ink:%1 (textBright %2, text %3)\" ).arg( litLine, litInk.name(), plainInk.name() ) );\n")

E("src/animworkspacetest.cpp",
  "\t\t\t\t\tcheck( *st, QStringLiteral( \"(q) and the lit ink is the palette's accent, not some other colour (%1 off it)\" ).arg( notAccent ), notAccent == 0 );\n",
  "\t\t\t\t\tcheck( *st, QStringLiteral( \"(q) and the lit ink is the palette's brightest white, textBright, not the accent (%1 off it by more than 2)\" ).arg( notWhite ), notWhite == 0 );\n"
  "\t\t\t\t\tcheck( *st, QStringLiteral( \"(q) while the unlit ink is still the plain text ink (%1 off it by more than 2)\" ).arg( notPlain ), notPlain == 0 );\n")

# ------------------------------------------------------------ C. the spell doc
E("tests/spells/animws.sh",
  "#       mean colour of the icon's own ink (alpha over half) is taken with the\n"
  "#       toggle off and on, the two must be at least 60 apart in summed RGB, and\n"
  "#       the ON mean must be within 40 of the palette's `accent`, so a button that\n"
  "#       only changed its plate cannot pass. Pose's tooltip joins the exact-text\n",
  "#       mean colour of the icon's own ink (alpha over half) is taken with the\n"
  "#       toggle off and on, and each mean must land within 2 of the token it is\n"
  "#       meant to be: OFF on `text`, ON on `textBright`, the palette's brightest\n"
  "#       ink. AMENDED 2026-09-12 07:3x, bungo over icons_before_after.png: \"Why\n"
  "#       do these turn yellow when selected? the buttons, keep them white\" --\n"
  "#       the lit ink was `accent` for one build. The two tokens are 33 apart in\n"
  "#       summed RGB, so \"it moved\" is floored at 20 and not the 60 the accent\n"
  "#       allowed; a button that only changed its plate still cannot pass, because\n"
  "#       both means are pinned to their own token. Pose's tooltip joins the exact-text\n")

# --------------------------------------------------------------- D. the skill
SK = ".claude/skills/ww-toggle-lit-gate/SKILL.md"

E(SK,
  "a QIcon On/Off pair in the palette's accent, the mean ink measured off the icon",
  "a QIcon On/Off pair in the palette's brightest ink, the mean ink measured off the icon")

E(SK,
  "One function states the rule for every mode toggle, so the accent lives in one\nplace:\n",
  "One function states the rule for every mode toggle, so the lit ink lives in one\nplace:\n")

E(SK,
  "\tic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( \"text\" ) ) ),            QIcon::Normal,   QIcon::Off );\n"
  "\tic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( \"accent\" ) ) ),          QIcon::Normal,   QIcon::On  );\n"
  "\tic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( \"textMuted\" ) ) ),       QIcon::Disabled, QIcon::Off );\n"
  "\tic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( \"accentDisabled\" ) ) ),  QIcon::Disabled, QIcon::On  );\n",
  "\tic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( \"text\" ) ) ),        QIcon::Normal,   QIcon::Off );\n"
  "\tic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( \"textBright\" ) ) ),  QIcon::Normal,   QIcon::On  );\n"
  "\tic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( \"textMuted\" ) ) ),   QIcon::Disabled, QIcon::Off );\n"
  "\tic.addPixmap( pixmap( glyph, px, QColor( wwSkinColor( \"text\" ) ) ),        QIcon::Disabled, QIcon::On  );\n")

E(SK,
  "* `accent` is the palette's own name for \"selection accent, active toggles\";\n"
  "  `accentDisabled` exists so a toggle that is ON but cannot be clicked does not\n"
  "  read as OFF.\n",
  "* **ASK WHICH INK, do not assume the accent.** The obvious choice is the\n"
  "  palette's accent -- it is even named \"selection accent, active toggles\" --\n"
  "  and in this tree it was overruled the same day it shipped (bungo, 2026-09-12\n"
  "  07:3x, over the before/after picture: \"Why do these turn yellow when\n"
  "  selected? the buttons, keep them white\"). The lit ink here is `textBright`,\n"
  "  the brightest ink token, and the unlit one is `text`; Disabled/On is `text`,\n"
  "  brighter than the `textMuted` of a disabled OFF toggle so it does not read as\n"
  "  OFF, dimmer than the enabled white so it still reads as disabled.\n"
  "* **A small ink step is fine when the PLATE carries the state.** `text` and\n"
  "  `textBright` are 33 apart in summed RGB out of 765. The signal the eye reads\n"
  "  is the QSS `:checked` background (`bgBtnDown`); the ink only has to stop\n"
  "  contradicting it. Size the gate's thresholds to the tokens you chose -- never\n"
  "  leave a floor sized for a colour you no longer use.\n")

E(SK,
  "3. `dist( off, on ) >= 60` -- it actually moves;\n"
  "4. `dist( on, accent ) <= 40` -- it moves TO the accent, not to some other colour;\n",
  "3. `dist( off, on ) >= floor` -- it actually moves. The floor is a fraction of\n"
  "   the distance between the two tokens (20 for a 33-apart pair), NOT a constant\n"
  "   carried over from whatever colour the toggle used to light in;\n"
  "4. `dist( on, litToken ) <= 2` AND `dist( off, plainToken ) <= 2` -- both means\n"
  "   are pinned to the token they are meant to be, so neither an accent left\n"
  "   behind nor a plate colour bleeding into the mean can pass. Pinning both ends\n"
  "   is what lets check 3's floor be small;\n")

E(SK,
  "print(sub[ink].mean(axis=0))                      # -> #61584b for an accent glyph\n",
  "print(sub[ink].mean(axis=0))                      # the lit glyph's ink, over the blue plate\n")

E(SK,
  "That measurement is what caught a lit state that the gate had passed and the\n"
  "report had already claimed (2026-09-12, lane UINOTES2): the icon carried the\n"
  "accent, the button never asked for it.\n",
  "That measurement is what caught a lit state that the gate had passed and the\n"
  "report had already claimed (2026-09-12, lane UINOTES2): the icon carried the\n"
  "lit pixmap, the button never asked for it.\n")


# ------------------------------------------------------------------ the engine
def main():
    byfile = {}
    for rel, old, new in EDITS:
        byfile.setdefault(rel, []).append((old, new))

    out = {}
    bad = []
    for rel, edits in byfile.items():
        p = os.path.join(ROOT, rel)
        with io.open(p, "rb") as f:
            raw = f.read()
        if raw.count(b"\r"):
            bad.append("%s carries %d CR bytes; this script only writes LF files"
                       % (rel, raw.count(b"\r")))
            continue
        s = raw.decode("utf-8")
        for old, new in edits:
            n = s.count(old)
            if n != 1:
                bad.append("%s: anchor found %d times (want exactly 1): %r"
                           % (rel, n, old[:70]))
                continue
            if new in s:
                bad.append("%s: replacement already present: %r" % (rel, new[:70]))
                continue
            s = s.replace(old, new, 1)
        out[rel] = s.encode("utf-8")

    if bad:
        for b in bad:
            sys.stderr.write("REFUSED: " + b + "\n")
        sys.exit(1)

    for rel, data in out.items():
        assert b"\r" not in data, rel
        p = os.path.join(ROOT, rel)
        with io.open(p, "wb") as f:
            f.write(data)
        print("%-44s %8d B  LF %d" % (rel, len(data), data.count(b"\n")))


main()
