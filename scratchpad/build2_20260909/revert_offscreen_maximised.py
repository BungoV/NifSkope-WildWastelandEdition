#!/usr/bin/env python3
# BUILD2: REVERT of fix_offscreen_maximised.py.
#
# The un-maximise line WORKED -- release/ww_render_shot/*.winlog went from
# "2 of 4 records onscreen=1" to "0 of 4" on every headless run, so the window
# really did leave every screen. And then NOTHING RENDERED:
#
#   WW_RENDER_SHOT  exited 0 in 5 s and wrote NO PNG at all (not a black one)
#   WW_IMPOSTOR_BAKE exited 0 in 3 s, wrote its sidecar, and wrote NO card image
#
# A window that is entirely outside every screen is never exposed, so
# QOpenGLWidget never creates its context, grabFramebuffer() returns a null
# QImage and QImage::save() writes nothing. An off-screen bake would therefore
# have produced EMPTY card sets while exiting 0 -- and the driver caches by form
# id, so the emptiness would have been cached.
#
# That is a worse failure than the hazard it fixes, so the tree goes back to the
# state lane OFFSCREEN left it in (the off-screen branch present but inert on
# this machine) and the finding goes to the director. The real fix is the one
# that lane already named: render into an FBO instead of the window
# (GLView::grabSupersampled, src/glview.cpp, which short-circuits shift == 0 to
# grabFramebuffer()). An untested second candidate: leave the window on screen
# and set window opacity 0, which stays exposed and composited and so still
# renders. Neither is a build lane's call.

import sys

PATH = "E:/Projects/NifskopeWildWastelandEdition/src/nifskope_ui.cpp"

NEW = (b'\t\tif ( headlessOffscreen ) {\n'
       b'\t\t\tskope->setAttribute( Qt::WA_ShowWithoutActivating, true );\n'
       b'\t\t\t/* UN-MAXIMISE FIRST, or the move is a no-op (measured\n'
       b'\t\t\t * 2026-09-09, lane BUILD2, on the first build that carried\n'
       b'\t\t\t * this branch). restoreUi() above restores the window state\n'
       b'\t\t\t * the person last left, which is MAXIMISED, and on Windows\n'
       b'\t\t\t * move() on a maximised window changes nothing except which\n'
       b'\t\t\t * monitor it is maximised onto -- and an off-screen point is\n'
       b'\t\t\t * on no monitor, so every headless run still came up\n'
       b'\t\t\t * maximised on the primary screen and the gate read\n'
       b'\t\t\t * onscreen=1. The instrument is what caught it:\n'
       b'\t\t\t * release/ww_headless_windows.log said geom=0,23,1920x1017\n'
       b'\t\t\t * where the off-screen origin was asked for. */\n'
       b'\t\t\tskope->setWindowState( skope->windowState()\n'
       b'\t\t\t\t& ~( Qt::WindowMaximized | Qt::WindowFullScreen ) );\n'
       b'\t\t\tskope->move( wwOffscreenWindowOrigin() );\n'
       b'\t\t\tskope->show();\n')

OLD = (b'\t\tif ( headlessOffscreen ) {\n'
       b'\t\t\tskope->setAttribute( Qt::WA_ShowWithoutActivating, true );\n'
       b'\t\t\tskope->move( wwOffscreenWindowOrigin() );\n'
       b'\t\t\tskope->show();\n')

with open(PATH, "rb") as fh:
    b = fh.read()
cr0, lf0 = b.count(b"\r"), b.count(b"\n")
if b.count(NEW) != 1:
    print("ABORT: anchor count %d" % b.count(NEW))
    sys.exit(2)
b = b.replace(NEW, OLD)
cr1, lf1 = b.count(b"\r"), b.count(b"\n")
if cr1 != cr0:
    print("ABORT: CR moved")
    sys.exit(2)
with open(PATH, "wb") as fh:
    fh.write(b)
print("OK reverted; CR %d (unchanged) LF %d -> %d (%d)" % (cr1, lf0, lf1, lf1 - lf0))
