"""UINOTES2 step 4 -- take back a claim the picture refuted.

Step 1 wired the loop button and the shared `aAnimLoop` QAction to
`wwTransportToggleIcon`, and the report said "Loop lights too ... the Animation
menu's own Loop entry lights with the same ink". THE BUILT EXE SAYS OTHERWISE.

Measured on transport_2x_on.png (06:35, the gate's own 2:1 grab), by the ink
inside each checked button's blue (#355f86) box:

    loop     x 528..589   ink mean #4d5761   <- plain ink over the blue
    pose     x 602..663   ink mean #61584b   <- the accent
    auto-key x 676..737   ink mean #665847   <- the accent

Cause, found by reading rather than guessing: the render toolbar re-skins the
SHARED action on every refresh --

    src/nifskope_ui.cpp:27726
        ui->aAnimLoop->setIcon( tlMakeIcon( QStringLiteral( "loop" ),
                                            have ? icoCol : icoColOff ) );

-- with a single-state icon, and the dock's button takes its icon from that
action (setDefaultAction). Whatever the dock puts on the action is replaced the
next time that popup refreshes.

So the lit loop is NOT this lane's to give: it would mean changing the RENDER
TOOLBAR's own loop glyph, which bungo did not ask for (he named the pose button
and the dot). This patch puts the loop icon back to what it was and writes the
measurement and the file:line where the decision lives, so the next lane does
not have to find it twice.

Refusing script: exact-once anchors, pure LF, all-or-nothing.
"""
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "src", "animworkspace.cpp")

with open(SRC, "rb") as f:
    orig = f.read()
assert orig.count(b"\r") == 0
text = orig.decode("utf-8")

NOTE = (
    "\t/* NOT the toggle icon, and this is measured, not assumed. The loop button\n"
    "\t   takes its icon from the SHARED aAnimLoop action, and the render\n"
    "\t   toolbar re-skins that action on every refresh of its popup\n"
    "\t   (src/nifskope_ui.cpp, `ui->aAnimLoop->setIcon( tlMakeIcon( \"loop\", ... ) )`)\n"
    "\t   with a single-state icon. A lit state put here is overwritten the next\n"
    "\t   time that popup refreshes: in transport_2x_on.png (2026-09-12 06:35)\n"
    "\t   the checked loop button's ink measures #4d5761, plain ink over the\n"
    "\t   checked blue, while pose and auto-key measure #61584b and #665847, the\n"
    "\t   accent. Making loop light means changing the RENDER TOOLBAR's own loop\n"
    "\t   glyph, which is not what bungo asked for (06:0x named the pose button\n"
    "\t   and the dot), so it stays his call. */\n"
)

subs = [
    (
        "\t\tloop->setIcon( wwTransportToggleIcon( GlyphLoop, WW_TRANSPORT_ICON_PX ) );\n",
        NOTE.replace("\t/*", "\t\t/*").replace("\n\t   ", "\n\t\t   ").replace("\n\t*/", "\n\t\t*/")
        + "\t\tloop->setIcon( wwTransportIcon( GlyphLoop, WW_TRANSPORT_ICON_PX ) );\n",
    ),
    (
        "\tbtnLoop->setIcon( wwTransportToggleIcon( GlyphLoop, WW_TRANSPORT_ICON_PX ) );\n",
        "",
    ),
    (
        " *  Qt picks the On pixmap for a checked QToolButton (QStyle sets State_On)\n"
        " *  and for a checked QAction in a menu, so the Animation menu's own Loop\n"
        " *  entry lights with the same ink as the button -- which is the point of\n"
        " *  having one set. Play and Play backwards do NOT use this: they say they\n"
        " *  are running by swapping the glyph to the pause bars, and two signals for\n"
        " *  one state is one more than the button needs. */\n",
        " *  Qt picks the On pixmap for a checked QToolButton (QStyle sets State_On)\n"
        " *  and for a checked QAction in a menu.\n"
        " *\n"
        " *  WHO USES IT, and who does not. The two gizmo toggles do. Play and Play\n"
        " *  backwards do NOT: they already say they are running by swapping the\n"
        " *  glyph to the pause bars, and two signals for one state is one more than\n"
        " *  the button needs. Loop does NOT either, and that is a measurement, not\n"
        " *  a preference -- see the note in addAnimActions: the render toolbar owns\n"
        " *  that action's icon and re-skins it on every refresh. */\n",
    ),
]

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
assert out.count(b"\r") == 0
assert "wwTransportToggleIcon( GlyphLoop" not in text, "a lit loop is still wired somewhere"
with open(SRC, "wb") as f:
    f.write(out)
print("written %s: CR %d LF %d bytes %d (was %d)" % (SRC, out.count(b"\r"), out.count(b"\n"), len(out), len(orig)))
