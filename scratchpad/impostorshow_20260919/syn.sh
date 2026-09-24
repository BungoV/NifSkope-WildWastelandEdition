#!/bin/sh
# ---------------------------------------------------------------------------
# syn.sh -- lane IMPOSTORSHOW's syntax gate. 2026-09-19.
#
# PHASE A owns no build slot: lane GLTFEXPORT1 has it. So nothing here runs
# `make` and nothing links. `g++ -fsyntax-only` with the REAL flags out of
# Makefile.Release is the most a lane can honestly claim without the slot, and
# it is claimed as exactly that: these files PARSE and TYPE-CHECK against the
# real Qt 6 headers and the real defines. It does not say they link, it does
# not say they run, and no picture may be quoted off it.
#
# The flags are copied from Makefile.Release lines 16..19 (DEFINES, CXXFLAGS,
# INCPATH), minus the ones only a real compile needs (-O3, -Wa,-mbig-obj).
# If that Makefile is regenerated with different Qt paths, this refuses on a
# missing header rather than quietly checking against nothing.
#
#   sh scratchpad/impostorshow_20260919/syn.sh            # every lane file
#   sh scratchpad/impostorshow_20260919/syn.sh src/x.cpp  # just that one
# ---------------------------------------------------------------------------

set -u

root=$( cd "$( dirname "$0" )/../.." && pwd )
cd "$root" || exit 1

# The compiler is MSYS2's UCRT64 g++, the one that builds the application.
# Git Bash has no g++ at all, so a lane driving this from the wrong shell must
# be told which shell it is in rather than reading "command not found" twice.
if ! command -v g++ >/dev/null 2>&1; then
	# NOTE the POSIX spelling: PATH is colon-separated, so a "C:/..." entry is
	# read as two directories named "C" and "/msys64/..." and finds nothing.
	# That mistake costs a confusing "command not found" with the right
	# directory apparently on PATH, so it is spelled out here.
	if [ -x /c/msys64/ucrt64/bin/g++.exe ]; then
		PATH="/c/msys64/ucrt64/bin:$PATH"
		export PATH
	else
		echo "REFUSED: no g++ on PATH and none at C:/msys64/ucrt64/bin"
		exit 1
	fi
fi

QT=C:/msys64/ucrt64/include/qt6
if [ ! -d "$QT" ]; then
	echo "REFUSED: no Qt 6 headers at $QT -- Makefile.Release's INCPATH has moved"
	exit 1
fi

DEFS="-DUNICODE -D_UNICODE -DWIN32 -DMINGW_HAS_SECURE_API=1 \
-DQT_DISABLE_DEPRECATED_BEFORE=0x060400 -DQT_NO_DEBUG_OUTPUT -D_USE_MATH_DEFINES \
-DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING \
-DNIFSKOPE_VERSION=\"2.0.dev11\" -DNIFSKOPE_REVISION=\"720762a\" \
-DWW_EDITION_VERSION=\"0.3.3\" -DWW_HKXANIM_UI -DWW_HKXCLIP_CANON -DWW_ANIMWS_HKXMODEL \
-DEDIT_ON_ACTIVATE -DQT_NO_DEBUG -DQT_OPENGLWIDGETS_LIB -DQT_OPENGL_LIB \
-DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_XML_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB \
-DQT_NEEDS_QMAIN"

INC="-I. -Isrc -Ilib -I$QT -I$QT/QtOpenGLWidgets -I$QT/QtOpenGL -I$QT/QtWidgets \
-I$QT/QtGui -I$QT/QtXml -I$QT/QtNetwork -I$QT/QtCore \
-IGeneratedFiles/.moc -IGeneratedFiles/.ui \
-Ilib/qhull/src -isystem lib/gli/gli -isystem lib/gli/external -Ilib/libfo76utils/src \
-IC:/msys64/ucrt64/share/qt6/mkspecs/win32-g++"

FLAGS="-fsyntax-only -std=gnu++2a -Wall -Wextra -fexceptions -mthreads"

if [ $# -gt 0 ]; then
	files="$*"
else
	# Every file this lane wrote that is meant to end up in the application.
	# impostoroct.cpp is here too although the gate compiles it standalone:
	# it must ALSO be clean inside the application's warning set.
	files="src/impostoroct.cpp src/impostorcard.cpp src/gl/impostordraw.cpp src/impostorpreviewtest.cpp"
fi

fails=0
for f in $files; do
	if [ ! -f "$f" ]; then
		echo "SKIP  $f (not written yet)"
		continue
	fi
	# shellcheck disable=SC2086
	if g++ $FLAGS $INC $DEFS "$f"; then
		echo "OK    $f"
	else
		echo "FAIL  $f"
		fails=$(( fails + 1 ))
	fi
done

echo "syn.sh: $fails failures"
[ "$fails" -eq 0 ] || exit 1
exit 0
