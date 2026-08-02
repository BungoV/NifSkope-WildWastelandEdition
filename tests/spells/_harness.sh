# Shared setup for the harnesses that open a REAL NifSkope window.
# Sourced, not executed:  . "$(dirname "$0")/_harness.sh"
#
# WHY THIS EXISTS
#
# These harnesses are not headless. They start the application, drive real
# widgets and quit — which on the primary monitor means a window appearing over
# whatever the user is working on and taking the keyboard with it. Running the
# suite then costs someone else their focus, so it stops being something you can
# do freely, which is the whole value of having it.
#
# WW_WINDOW_AT=x,y is read in src/nifskope_ui.cpp: it moves the window BEFORE
# show() and skips raise(). Both halves matter. Moving it after show() makes the
# window appear on the primary monitor for a frame and then jump, which is the
# same interruption with extra steps; and raise() takes focus even once the
# window is out of the way.
#
# CHANGE THE GEOMETRY HERE, not in twelve scripts. Current layout on this
# machine (System.Windows.Forms.Screen):
#
#     DISPLAY1   primary   0,0      1920x1080
#     DISPLAY5             1920,0   1920x1080     <- harnesses go here
#     DISPLAY10            -1280,122 1280x800
#
# Override for a one-off run:  WW_WINDOW_AT=0,0 bash tests/spells/top_bar.sh
# Anything that is not "x,y" falls back to the normal show()+raise() path, so an
# empty value is the way to get the old behaviour deliberately.

export WW_WINDOW_AT="${WW_WINDOW_AT:-1960,40}"
