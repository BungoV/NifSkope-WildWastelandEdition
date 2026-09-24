#!/bin/bash
# Syntax + semantics pass over lane FILESTAB's new translation units, with the
# REAL flags out of Makefile.Release (nifskope-ww-build-verify, "When you CANNOT
# build"). Writes nothing, needs no build slot, does not touch release/.
#
# Run twice: once as the tree stands (the hook-up NOT applied, so the two seam
# calls are inside #else and the harness prints a named SKIP), and once with
# -DWW_FILESTAB_HOOKUP=1 forced, which is what the applied hook-up produces.
#
#   bash scratchpad/filestab_20260910/syntax.sh
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 2

FLAGS='-march=nocona -msahf -mtune=generic -Wa,-mbig-obj -Ilib/qhull/src
 -isystem lib/gli/gli -isystem lib/gli/external -Ilib/libfo76utils/src
 -std=gnu++2a -Wall -Wextra -fexceptions -mthreads -DUNICODE -D_UNICODE -DWIN32
 -DMINGW_HAS_SECURE_API=1 -DQT_NO_DEBUG -DQT_DISABLE_DEPRECATED_BEFORE=0x060400
 -DQT_NO_DEBUG_OUTPUT -D_USE_MATH_DEFINES -DQT_NO_CAST_FROM_BYTEARRAY
 -DQT_NO_URL_CAST_FROM_STRING -DEDIT_ON_ACTIVATE -DNIFSKOPE_VERSION=\"x\"
 -DNIFSKOPE_REVISION=\"x\" -DWW_EDITION_VERSION=\"x\" -DQT_OPENGLWIDGETS_LIB
 -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_XML_LIB -DQT_NETWORK_LIB
 -DQT_CORE_LIB -DQT_NEEDS_QMAIN -I. -Isrc -Ilib -IC:/msys64/ucrt64/include/qt6
 -IC:/msys64/ucrt64/include/qt6/QtOpenGLWidgets
 -IC:/msys64/ucrt64/include/qt6/QtOpenGL -IC:/msys64/ucrt64/include/qt6/QtWidgets
 -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtXml
 -IC:/msys64/ucrt64/include/qt6/QtNetwork -IC:/msys64/ucrt64/include/qt6/QtCore
 -IGeneratedFiles/.moc -IGeneratedFiles/.ui
 -IC:/msys64/ucrt64/share/qt6/mkspecs/win32-g++'

# The -DWW_FILESTAB_HOOKUP pass is only a REAL configuration once the hook-up
# has been applied: the macro and the two method declarations it needs are the
# same edit to src/nifskope.h. Forcing the macro on an un-hooked tree fails with
# "class NifSkope has no member named wwFilesTabOpenRow", which is the guard
# working, not a defect -- so the second pass runs only when the marker is there.
PASSES=""
grep -q "WW_FILESTAB_HOOKUP" src/nifskope.h 2>/dev/null && PASSES="-DWW_FILESTAB_HOOKUP=1"

ALLRC=0
for extra in "" $PASSES; do
	echo "===== extra flags: '${extra:-none}'"
	for f in src/filestab.cpp src/filestabtest.cpp; do
		echo "== $f"
		# shellcheck disable=SC2086
		g++ -fsyntax-only $FLAGS $extra "$f" 2>&1 | grep -v 'sfinae-incomplete' | head -30
		rc=${PIPESTATUS[0]}
		echo "RC=$rc"
		[ "$rc" = 0 ] || ALLRC=1
	done
done
echo "ALL-RC=$ALLRC"
