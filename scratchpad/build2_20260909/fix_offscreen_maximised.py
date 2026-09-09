#!/usr/bin/env python3
# BUILD2: the off-screen branch was INERT because the window is MAXIMISED.
#
# MEASURED, from the gate's own instrument (release/ww_render_shot/*.winlog,
# build 18:29:24):
#   headless run   shown QWidgetWindow geom=0,23,1920x1017   onscreen=1
#   control run    shown QWidgetWindow geom=1920,-42,1920x1017 onscreen=1
# The control asked for move(1960,40) and the window's client origin came out
# 1920,-42 -- the MAXIMISED client origin of the monitor that contains 1960,40,
# not the requested point. A normal window would have landed at 1968,71 (frame
# 1960,40 plus the 8 px border and 31 px title bar), which is exactly where the
# same window IS at the later `grab` record, after the render hook calls
# showNormal(). So restoreUi()'s restoreGeometry() brings the window up
# maximised, and on Windows move() on a maximised window changes nothing except,
# when the target point is on another monitor, which monitor it maximises onto.
# An off-screen point is on no monitor, so the move was a no-op and every
# headless run came up maximised on the primary screen.
#
# The fix is one line: clear the maximised/full-screen bits BEFORE the move.

import sys

PATH = "E:/Projects/NifskopeWildWastelandEdition/src/nifskope_ui.cpp"

OLD = (b'\t\tif ( headlessOffscreen ) {\n'
       b'\t\t\tskope->setAttribute( Qt::WA_ShowWithoutActivating, true );\n'
       b'\t\t\tskope->move( wwOffscreenWindowOrigin() );\n'
       b'\t\t\tskope->show();\n')

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

with open(PATH, "rb") as fh:
    b = fh.read()
cr0, lf0 = b.count(b"\r"), b.count(b"\n")
if b.count(OLD) != 1:
    print("ABORT: anchor count %d" % b.count(OLD))
    sys.exit(2)
b = b.replace(OLD, NEW)
cr1, lf1 = b.count(b"\r"), b.count(b"\n")
if cr1 != cr0:
    print("ABORT: CR moved %d -> %d" % (cr0, cr1))
    sys.exit(2)
with open(PATH, "wb") as fh:
    fh.write(b)
print("OK CR %d (unchanged) LF %d -> %d (+%d)" % (cr1, lf0, lf1, lf1 - lf0))
