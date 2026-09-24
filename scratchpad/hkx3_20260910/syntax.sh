#!/bin/bash
# Syntax-check lane HKX3's files with the REAL Makefile.Release flags (the same
# list scratchpad/hkx1_20260910/build_dump.sh uses, which was lifted from the
# generated makefile). No linking, no exe, so this is safe while bungo's bake
# holds the GUI.
#   MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWildWastelandEdition && bash scratchpad/hkx3_20260910/syntax.sh'
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
FLAGS="-march=nocona -msahf -mtune=generic -Wa,-mbig-obj -Ilib/qhull/src -isystem lib/gli/gli -isystem lib/gli/external -Ilib/libfo76utils/src -std=gnu++2a -Wall -Wextra -fexceptions -mthreads -DUNICODE -D_UNICODE -DWIN32 -DMINGW_HAS_SECURE_API=1 -DQT_NO_DEBUG -DQT_DISABLE_DEPRECATED_BEFORE=0x060400 -DQT_NO_DEBUG_OUTPUT -D_USE_MATH_DEFINES -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING -DEDIT_ON_ACTIVATE -DWW_HKXANIM_UI -DNIFSKOPE_VERSION='\"x\"' -DNIFSKOPE_REVISION='\"x\"' -DWW_EDITION_VERSION='\"x\"' -DQT_OPENGLWIDGETS_LIB -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_XML_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB -DQT_NEEDS_QMAIN -I. -Isrc -Ilib -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtOpenGLWidgets -IC:/msys64/ucrt64/include/qt6/QtOpenGL -IC:/msys64/ucrt64/include/qt6/QtWidgets -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtXml -IC:/msys64/ucrt64/include/qt6/QtNetwork -IC:/msys64/ucrt64/include/qt6/QtCore -IGeneratedFiles/.moc -IGeneratedFiles/.ui -IC:/msys64/ucrt64/share/qt6/mkspecs/win32-g++"
RC=0
for f in "$@"; do
	echo "== syntax $f"
	eval g++ -fsyntax-only $FLAGS "$f" 2>&1 | grep -v "sfinae-incomplete\|qchar.h" | head -60
	r=${PIPESTATUS[0]}
	echo "SYNTAX-RC[$f]=$r"
	[ "$r" = "0" ] || RC=1
done
echo "ALL-RC=$RC"
exit $RC
