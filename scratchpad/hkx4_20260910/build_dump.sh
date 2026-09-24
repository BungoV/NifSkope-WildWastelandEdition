#!/bin/bash
# Lane HKX4. Syntax-check src/gltfexport.cpp with the REAL Makefile.Release
# flags, then link the standalone release/gltfexport_dump.exe (Qt6Core +
# Qt6Gui). Never touches release/NifSkope.exe, GeneratedFiles/ or the .pro
# build -- CONSTITUTION 6 (Fallout4.exe was up), skill ww-standalone-writer-gate.
#
# Run inside MSYS2 UCRT64:
#   MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
#     'cd /e/Projects/NifskopeWildWastelandEdition && bash scratchpad/hkx4_20260910/build_dump.sh'
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
FLAGS="-march=nocona -msahf -mtune=generic -Wa,-mbig-obj -Ilib/qhull/src -isystem lib/gli/gli -isystem lib/gli/external -Ilib/libfo76utils/src -std=gnu++2a -Wall -Wextra -fexceptions -mthreads -DUNICODE -D_UNICODE -DWIN32 -DMINGW_HAS_SECURE_API=1 -DQT_NO_DEBUG -DQT_DISABLE_DEPRECATED_BEFORE=0x060400 -DQT_NO_DEBUG_OUTPUT -D_USE_MATH_DEFINES -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING -DEDIT_ON_ACTIVATE -DNIFSKOPE_VERSION='\"x\"' -DNIFSKOPE_REVISION='\"x\"' -DWW_EDITION_VERSION='\"x\"' -DQT_OPENGLWIDGETS_LIB -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_XML_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB -DQT_NEEDS_QMAIN -I. -Isrc -Ilib -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtOpenGLWidgets -IC:/msys64/ucrt64/include/qt6/QtOpenGL -IC:/msys64/ucrt64/include/qt6/QtWidgets -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtXml -IC:/msys64/ucrt64/include/qt6/QtNetwork -IC:/msys64/ucrt64/include/qt6/QtCore -IGeneratedFiles/.moc -IGeneratedFiles/.ui -IC:/msys64/ucrt64/share/qt6/mkspecs/win32-g++"
echo "== syntax src/gltfexport.cpp"
eval g++ -fsyntax-only $FLAGS src/gltfexport.cpp 2>&1 | grep -v "sfinae-incomplete\|qchar.h" | head -40
echo "SYNTAX-RC=${PIPESTATUS[0]}"
echo "== syntax tests/gltfexport_dump.cpp"
eval g++ -fsyntax-only $FLAGS tests/gltfexport_dump.cpp 2>&1 | grep -v "sfinae-incomplete\|qchar.h" | head -40
echo "SYNTAX-DUMP-RC=${PIPESTATUS[0]}"
HKXANIM=${HKXANIM_SRC:-src/hkxanim.cpp}
echo "== build release/gltfexport_dump.exe (hkxanim from $HKXANIM)"
eval g++ -O1 $FLAGS tests/gltfexport_dump.cpp src/gltfexport.cpp "$HKXANIM" tests/hkxanim_shim.cpp \
  -o "${OUT:-release/gltfexport_dump.exe}" -LC:/msys64/ucrt64/lib -lQt6Core -lQt6Gui 2>&1 \
  | grep -v "sfinae-incomplete\|qchar.h" | head -40
echo "BUILD-RC=${PIPESTATUS[0]}"
ls -l --time-style=+%H:%M:%S "${OUT:-release/gltfexport_dump.exe}" 2>/dev/null
