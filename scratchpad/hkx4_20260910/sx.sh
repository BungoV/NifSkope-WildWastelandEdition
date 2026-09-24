#!/bin/bash
# Lane HKX4b. Syntax-check one or more sources with the REAL Makefile.Release
# flags -- the only compile evidence available while NifSkope.exe may not be
# built (skill nifskope-ww-build-verify, "When you CANNOT build"). It writes
# no object and touches nothing in release/ or GeneratedFiles/.
#
# Run inside MSYS2 UCRT64:
#   MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
#     'cd /e/Projects/NifskopeWildWastelandEdition && bash scratchpad/hkx4_20260910/sx.sh src/gltfexportnif.cpp'
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
FLAGS="-march=nocona -msahf -mtune=generic -Wa,-mbig-obj -Ilib/qhull/src -isystem lib/gli/gli -isystem lib/gli/external -Ilib/libfo76utils/src -std=gnu++2a -Wall -Wextra -fexceptions -mthreads -DUNICODE -D_UNICODE -DWIN32 -DMINGW_HAS_SECURE_API=1 -DQT_NO_DEBUG -DQT_DISABLE_DEPRECATED_BEFORE=0x060400 -DQT_NO_DEBUG_OUTPUT -D_USE_MATH_DEFINES -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING -DEDIT_ON_ACTIVATE -DNIFSKOPE_VERSION='\"x\"' -DNIFSKOPE_REVISION='\"x\"' -DWW_EDITION_VERSION='\"x\"' -DQT_OPENGLWIDGETS_LIB -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_XML_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB -DQT_NEEDS_QMAIN -I. -Isrc -Ilib -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtOpenGLWidgets -IC:/msys64/ucrt64/include/qt6/QtOpenGL -IC:/msys64/ucrt64/include/qt6/QtWidgets -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtXml -IC:/msys64/ucrt64/include/qt6/QtNetwork -IC:/msys64/ucrt64/include/qt6/QtCore -IGeneratedFiles/.moc -IGeneratedFiles/.ui -IC:/msys64/ucrt64/share/qt6/mkspecs/win32-g++"
rc=0
for f in "$@"; do
	echo "== syntax $f"
	eval g++ -fsyntax-only $FLAGS "$f" 2>&1 | grep -v "sfinae-incomplete\|qchar.h" | head -40
	r=${PIPESTATUS[0]}
	echo "SYNTAX-RC=$r  $f"
	[ "$r" = 0 ] || rc=1
done
exit $rc
