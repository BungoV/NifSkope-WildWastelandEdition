"""UINOTES2 step 1 -- the two transport-row toggles become icons in ruling 4's
set, with a lit (accent) state when the mode is on.

bungo, 2026-09-12 06:0x: "what is the "pose" button and the round dot that's
not centered button?" -- offered both drawn as icons in the same set with a lit
state when on -- "Both icons".

Refusing script: every anchor must appear exactly once, the file must stay pure
LF, and nothing is written unless every substitution succeeded.
"""
import io, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "src", "animworkspace.cpp")

with open(SRC, "rb") as f:
    orig = f.read()
cr0 = orig.count(b"\r")
assert cr0 == 0, "animworkspace.cpp is meant to be pure LF, found %d CR" % cr0
text = orig.decode("utf-8")

subs = []


def sub(anchor, replacement):
    subs.append((anchor, replacement))


# ---- 1. the new glyph joins the enum
sub(
    "\tGlyphToStart, GlyphPrevKey, GlyphPlayBack, GlyphPlay, GlyphPause,\n"
    "\tGlyphStop, GlyphNextKey, GlyphToEnd, GlyphLoop, GlyphRecord\n",
    "\tGlyphToStart, GlyphPrevKey, GlyphPlayBack, GlyphPlay, GlyphPause,\n"
    "\tGlyphStop, GlyphNextKey, GlyphToEnd, GlyphLoop, GlyphRecord, GlyphPose\n",
)

# ---- 2. the record dot gets the set's own mass, and the pose glyph is drawn
sub(
    "\tcase GlyphRecord:\n"
    "\t\tp.setBrush( ink );\n"
    "\t\tp.setPen( Qt::NoPen );\n"
    "\t\tp.drawEllipse( QPointF( 8.0, 8.0 ), 4.0, 4.0 );\n"
    "\t\tbreak;\n",
    "\tcase GlyphRecord:\n"
    "\t\t/* Blender's auto-key record dot. Radius 5, not 4: every other glyph\n"
    "\t\t   in this set is ten units tall (the triangle's back, the bar), and a\n"
    "\t\t   dot of diameter 8 beside them read as a speck -- which is how bungo\n"
    "\t\t   described it (2026-09-12 06:0x, \"the round dot\"). */\n"
    "\t\tp.setBrush( ink );\n"
    "\t\tp.setPen( Qt::NoPen );\n"
    "\t\tp.drawEllipse( QPointF( 8.0, 8.0 ), 5.0, 5.0 );\n"
    "\t\tbreak;\n"
    "\tcase GlyphPose: {\n"
    "\t\t/* POSE WITH THE GIZMO. Blender has no single icon for this: its\n"
    "\t\t   armature glyph is a bone, and the gizmo is a ring drawn in the\n"
    "\t\t   viewport, never on a button. So this glyph is the two put together,\n"
    "\t\t   and it is OURS, not Blender's -- said plainly because the house rule\n"
    "\t\t   is to follow Blender and name every divergence.\n"
    "\n"
    "\t\t   The bone is Blender's OCTAHEDRAL bone, the shape an armature draws:\n"
    "\t\t   a head at the top, two shoulders a third of the way down, a long\n"
    "\t\t   tail. Around it are the two side arcs of the rotate gizmo's ring,\n"
    "\t\t   open at the top and the bottom so the bone's two tips read as tips\n"
    "\t\t   instead of merging into the ring. Same 2 units of stroke and the\n"
    "\t\t   same 16x16 grid as every other glyph here. */\n"
    "\t\tconst QRectF ring( 1.6, 1.6, 12.8, 12.8 );\n"
    "\t\tp.setPen( QPen( ink, 2.0, Qt::SolidLine, Qt::FlatCap ) );\n"
    "\t\tp.setBrush( Qt::NoBrush );\n"
    "\t\tp.drawArc( ring, int( -50 * 16 ), int( 100 * 16 ) );\n"
    "\t\tp.drawArc( ring, int( 130 * 16 ), int( 100 * 16 ) );\n"
    "\t\tp.setPen( Qt::NoPen );\n"
    "\t\tQPainterPath bone;\n"
    "\t\tbone.moveTo( 8.0, 1.8 );\n"
    "\t\tbone.lineTo( 10.4, 6.0 );\n"
    "\t\tbone.lineTo( 8.0, 14.2 );\n"
    "\t\tbone.lineTo( 5.6, 6.0 );\n"
    "\t\tbone.closeSubpath();\n"
    "\t\tp.fillPath( bone, ink );\n"
    "\t\tbreak;\n"
    "\t}\n",
)

# ---- 3. the toggle icon: the same glyph, lit when the mode is on
sub(
    "//! The same glyph in the palette's ink and, for Disabled, in its muted ink.\n"
    "QIcon wwTransportIcon( int glyph, int px = 16 )\n"
    "{\n"
    "\tQIcon ic;\n"
    "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"text\" ) ) ), QIcon::Normal );\n"
    "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"textMuted\" ) ) ), QIcon::Disabled );\n"
    "\treturn ic;\n"
    "}\n",
    "//! The same glyph in the palette's ink and, for Disabled, in its muted ink.\n"
    "QIcon wwTransportIcon( int glyph, int px = 16 )\n"
    "{\n"
    "\tQIcon ic;\n"
    "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"text\" ) ) ), QIcon::Normal );\n"
    "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"textMuted\" ) ) ), QIcon::Disabled );\n"
    "\treturn ic;\n"
    "}\n"
    "\n"
    "/*! A MODE toggle's icon: the same drawing, in the plain ink while the mode is\n"
    " *  off and in the palette's `accent` while it is on.\n"
    " *\n"
    " *  bungo, 2026-09-12 06:1x, asked for \"Both icons\" after \"what is the \\\"pose\\\"\n"
    " *  button and the round dot\" -- i.e. the two gizmo switches drawn like the\n"
    " *  rest of the row, with a LIT state so one can see at a glance that the mode\n"
    " *  is on. `accent` is the palette's own name for \"active toggle\" (skinVars,\n"
    " *  nifskope_ui.cpp), and `accentDisabled` is its inert twin, so a toggle that\n"
    " *  is ON but cannot be clicked does not read as OFF.\n"
    " *\n"
    " *  Qt picks the On pixmap for a checked QToolButton (QStyle sets State_On)\n"
    " *  and for a checked QAction in a menu, so the Animation menu's own Loop\n"
    " *  entry lights with the same ink as the button -- which is the point of\n"
    " *  having one set. Play and Play backwards do NOT use this: they say they\n"
    " *  are running by swapping the glyph to the pause bars, and two signals for\n"
    " *  one state is one more than the button needs. */\n"
    "QIcon wwTransportToggleIcon( int glyph, int px = 16 )\n"
    "{\n"
    "\tQIcon ic;\n"
    "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"text\" ) ) ), QIcon::Normal, QIcon::Off );\n"
    "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"accent\" ) ) ), QIcon::Normal, QIcon::On );\n"
    "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"textMuted\" ) ) ), QIcon::Disabled, QIcon::Off );\n"
    "\tic.addPixmap( wwTransportPixmap( glyph, px, QColor( wwSkinColor( \"accentDisabled\" ) ) ), QIcon::Disabled, QIcon::On );\n"
    "\treturn ic;\n"
    "}\n",
)

# ---- 4. the loop action lights too (one set, one rule)
sub(
    "\t\tloop->setIcon( wwTransportIcon( GlyphLoop, WW_TRANSPORT_ICON_PX ) );\n",
    "\t\tloop->setIcon( wwTransportToggleIcon( GlyphLoop, WW_TRANSPORT_ICON_PX ) );\n",
)

# ---- 5. the two toggles: icons, and the lit ink
sub(
    "\tbtnLoop = mk( \"AnimWsLoop\", tr( \"Loop\" ), tr( \"Loop the animation (the render toolbar's own switch)\" ), true, GlyphLoop );\n",
    "\tbtnLoop = mk( \"AnimWsLoop\", tr( \"Loop\" ), tr( \"Loop the animation (the render toolbar's own switch)\" ), true, GlyphLoop );\n"
    "\tbtnLoop->setIcon( wwTransportToggleIcon( GlyphLoop, WW_TRANSPORT_ICON_PX ) );\n",
)

sub(
    "\tposeCheck = mk( \"AnimWsPose\", tr( \"pose\" ),\n"
    "\t\ttr( \"Pose with the gizmo: hold the selected bone out of the clip's pose so the viewport shows what the transform gizmo (G/R/S) does to it; Insert key then keys that pose at the playhead\" ), true );\n"
    "\tautoKeyCheck = mk( \"AnimWsAutoKey\", tr( \"Auto-key\" ),\n"
    "\t\ttr( \"Auto-key gizmo transforms: after every committed gizmo transform, key the bone at the playhead\" ), true, GlyphRecord );\n",
    "\t/* Both of these were the odd ones out until 2026-09-12 06:1x: \"pose\" was\n"
    "\t   a WORD in a row of drawings, and its text sat on the font's baseline,\n"
    "\t   nine 2:1 pixels below the line every glyph beside it is centred on\n"
    "\t   (measured off transport_2x.png: glyph ink centre y 27.5, the word's\n"
    "\t   36.5). That is what read as \"not centered\". Both are glyphs now, on the\n"
    "\t   same 16x16 grid, lit in the accent while the mode is on. */\n"
    "\tposeCheck = mk( \"AnimWsPose\", tr( \"pose\" ),\n"
    "\t\ttr( \"Pose with the gizmo: hold the selected bone out of the clip's pose so the viewport shows what the transform gizmo (G/R/S) does to it; Insert key then keys that pose at the playhead\" ), true, GlyphPose );\n"
    "\tposeCheck->setIcon( wwTransportToggleIcon( GlyphPose, WW_TRANSPORT_ICON_PX ) );\n"
    "\tautoKeyCheck = mk( \"AnimWsAutoKey\", tr( \"Auto-key\" ),\n"
    "\t\ttr( \"Auto-key gizmo transforms: after every committed gizmo transform, key the bone at the playhead\" ), true, GlyphRecord );\n"
    "\tautoKeyCheck->setIcon( wwTransportToggleIcon( GlyphRecord, WW_TRANSPORT_ICON_PX ) );\n",
)

bad = 0
for anchor, _ in subs:
    n = text.count(anchor)
    if n != 1:
        bad += 1
        print("ANCHOR x%d (want 1): %s" % (n, anchor.splitlines()[0][:90]))
if bad:
    sys.exit("refused: %d anchor(s) did not match exactly once; nothing written" % bad)

for anchor, rep in subs:
    text = text.replace(anchor, rep, 1)

out = text.encode("utf-8")
assert out.count(b"\r") == 0, "the edit introduced CR bytes"
with open(SRC, "wb") as f:
    f.write(out)
print("written %s: CR %d LF %d bytes %d (was %d)" % (SRC, out.count(b"\r"), out.count(b"\n"), len(out), len(orig)))
