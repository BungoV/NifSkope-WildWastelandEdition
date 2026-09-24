"""UINOTES2 step 5 -- pin the loop measurement in the gate, so the note beside
the code is a number the harness prints rather than a sentence someone has to
trust.

This is NOT asserted as a failure. Loop not lighting is a fact about who owns
the shared aAnimLoop icon (the render toolbar), not a defect, and the day
someone decides it SHOULD light, a check that says "it must not" would be
exactly the wrong thing to have written. So the harness measures it and says
it, next to the two toggles it does gate.

Refusing script: exact-once anchor, pure LF, all-or-nothing.
"""
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "src", "animworkspacetest.cpp")

with open(SRC, "rb") as f:
    orig = f.read()
assert orig.count(b"\r") == 0
text = orig.decode("utf-8")

anchor = (
	"					if ( auto * stop = widget<QToolButton>( ws, \"AnimWsStop\" ) ) {\n"
	"						const int stopMoved = dist( inkMean( stop->icon(), stop->iconSize(), QIcon::Off ),\n"
)
rep = (
	"					/* AND THE ONE THAT DOES NOT LIGHT, said out loud. The dock's loop\n"
	"					   button takes its icon from the SHARED aAnimLoop action, and the\n"
	"					   render toolbar re-skins that action on every refresh of its popup\n"
	"					   with a single-state icon, so nothing the dock puts on it survives.\n"
	"					   Measured, not asserted: a check that Loop must NOT light would be\n"
	"					   the wrong thing to own the day someone decides it should. */\n"
	"					if ( auto * lp = widget<QToolButton>( ws, \"AnimWsLoop\" ) ) {\n"
	"						const QColor lpOff = inkMean( lp->icon(), lp->iconSize(), QIcon::Off );\n"
	"						const QColor lpOn = inkMean( lp->icon(), lp->iconSize(), QIcon::On );\n"
	"						say( *st, QStringLiteral( \"  (q) Loop, whose icon the render toolbar owns: off=%1 on=%2 moved=%3 checked=%4\"\n"
	"								\" (not a gate -- a lit Loop would mean changing the render toolbar's own glyph)\" )\n"
	"							.arg( lpOff.name(), lpOn.name() ).arg( dist( lpOff, lpOn ) ).arg( lp->isChecked() ? 1 : 0 ) );\n"
	"					}\n"
) + anchor

n = text.count(anchor)
if n != 1:
	sys.exit("refused: anchor x%d (want 1); nothing written" % n)

text = text.replace(anchor, rep, 1)
out = text.encode("utf-8")
assert out.count(b"\r") == 0
with open(SRC, "wb") as f:
	f.write(out)
print("written %s: CR %d LF %d bytes %d (was %d)" % (SRC, out.count(b"\r"), out.count(b"\n"), len(out), len(orig)))
