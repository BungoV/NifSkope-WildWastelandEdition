#!/bin/bash
# Syntax-only compile of a translation unit with the REAL Makefile.Release flags.
# Writes nothing, needs no build slot, and does not disturb lane BUILD4.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
DEFINES='-DUNICODE -D_UNICODE -DWIN32 -DMINGW_HAS_SECURE_API=1 -DQT_DISABLE_DEPRECATED_BEFORE=0x060400 -DQT_NO_DEBUG_OUTPUT -D_USE_MATH_DEFINES -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING -DNIFSKOPE_VERSION="2.0.dev11" -DNIFSKOPE_REVISION="720762a" -DWW_EDITION_VERSION="0.3.3" -DEDIT_ON_ACTIVATE -DQT_NO_DEBUG -DQT_OPENGLWIDGETS_LIB -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_XML_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB -DQT_NEEDS_QMAIN'
CXXFLAGS='-fno-keep-inline-dllexport -march=nocona -msahf -mtune=generic -Wa,-mbig-obj -Ilib/qhull/src -isystem lib/gli/gli -isystem lib/gli/external -Ilib/libfo76utils/src -std=c++20 -O3 -march=haswell -std=gnu++2a -Wall -Wextra -fexceptions -mthreads'
INCPATH='-I. -Isrc -Ilib -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtOpenGLWidgets -IC:/msys64/ucrt64/include/qt6/QtOpenGL -IC:/msys64/ucrt64/include/qt6/QtWidgets -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtXml -IC:/msys64/ucrt64/include/qt6/QtNetwork -IC:/msys64/ucrt64/include/qt6/QtCore -IGeneratedFiles/.moc -IGeneratedFiles/.ui -I/include -IC:/msys64/ucrt64/share/qt6/mkspecs/win32-g++'
rc=0
for f in "$@"; do
	echo "--- $f"
	g++ -fsyntax-only $CXXFLAGS $INCPATH $DEFINES "$f" || rc=1
done
echo "syntax rc=$rc"
exit $rc
