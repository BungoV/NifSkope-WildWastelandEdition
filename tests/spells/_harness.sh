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

# Pure bash, deliberately no sed: when this script is launched from a Git-Bash
# parent, the MSYS2 shell inherits Git's sed, and BRE groups passed across the
# two runtimes silently stop matching — winpath then no-ops. Single argv/env
# paths still get rescued by MSYS2's automatic conversion, but a
# semicolon-joined LIST (WW_SAMPOSE_WEAPON) is not, so the weapon step quietly
# skips. Parameter expansion cannot be PATH-poisoned.
winpath() {
	case "$1" in
		/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
		*) printf '%s' "$1" ;;
	esac
}

# A FLOORED WINDOW IS A REFUSAL, NOT A MEASUREMENT.  Lane HARNESSWIN1, 2026-09-19.
#
# native_open.sh row (c) sat red at 0.8978 for a day because the window it
# measured was bungo's last window -- maximized, restored out of QSettings,
# 1822 px wide instead of the 1024 it asked for, so `upp` halved and a sub-pixel
# disagreement resolved into whole pixels.  Nothing in the run said so.  The
# number looked like a fact about the scene and was a fact about the machine.
#
# The restore no longer happens on a harness run, but a request can still be
# floored by the layout: the main window has a minimum width of about 1024 px
# (already written down at native_open.sh:117-119, "480x480 came back
# 1024x445"), and Qt will not go under a layout minimum for anybody.  So the
# size obtained is written to release/ww_harness_window.log with the size asked
# for beside it, and this is how a spell reads it back.
#
#   ww_window_refusal   -> prints the REFUSED sentence, or nothing
#   ww_window_line      -> the settled harness-window line, or nothing
#
# Use it the moment before the number is trusted:
#
#   if r="$(ww_window_refusal)" && [ -n "$r" ]; then echo "$r"; exit 1; fi
#
# It prints nothing when no such log exists, so a spell that has not run the
# application yet, or an older exe, is not turned red by adding the call.
ww_window_log() {
	printf '%s' "${WW_HARNESS_WINDOW_LOG:-$(dirname "${BASH_SOURCE[0]}")/../../release/ww_harness_window.log}"
}

ww_window_line() {
	local f; f="$(ww_window_log)"
	[ -f "$f" ] || return 0
	grep '^harness-window ' "$f" | tail -1
}

ww_window_refusal() {
	local f; f="$(ww_window_log)"
	[ -f "$f" ] || return 0
	grep '^REFUSED: the window size asked for was ' "$f" | tail -1
}
