#!/bin/bash
# Lane HKXEDIT2: build the standalone gate binary release/hkxclipedit_gate.exe
# (tests/hkxclipedit_gate.cpp + src/hkxclipedit.cpp + src/hkxanim.cpp + src/hkxwrite.cpp, Qt6Core only)
# and run the document gates, then HKXPACK re-reads the saved file (gate (g)).
# Run inside MSYS2 UCRT64:
#   MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWildWastelandEdition && bash scratchpad/hkxedit2_20260910/build_gate.sh'
# Flags: lane HKXEDIT1's build_gate.sh verbatim (the real Makefile.Release flags).
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
FLAGS="-march=nocona -msahf -mtune=generic -Wa,-mbig-obj -Ilib/qhull/src -isystem lib/gli/gli -isystem lib/gli/external -Ilib/libfo76utils/src -std=gnu++2a -Wall -Wextra -fexceptions -mthreads -DUNICODE -D_UNICODE -DWIN32 -DMINGW_HAS_SECURE_API=1 -DQT_NO_DEBUG -DQT_DISABLE_DEPRECATED_BEFORE=0x060400 -DQT_NO_DEBUG_OUTPUT -D_USE_MATH_DEFINES -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING -DEDIT_ON_ACTIVATE -DNIFSKOPE_VERSION='\"x\"' -DNIFSKOPE_REVISION='\"x\"' -DWW_EDITION_VERSION='\"x\"' -DQT_OPENGLWIDGETS_LIB -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_XML_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB -DQT_NEEDS_QMAIN -I. -Isrc -Ilib -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtOpenGLWidgets -IC:/msys64/ucrt64/include/qt6/QtOpenGL -IC:/msys64/ucrt64/include/qt6/QtWidgets -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtXml -IC:/msys64/ucrt64/include/qt6/QtNetwork -IC:/msys64/ucrt64/include/qt6/QtCore -IGeneratedFiles/.moc -IGeneratedFiles/.ui -IC:/msys64/ucrt64/share/qt6/mkspecs/win32-g++"
OUT=scratchpad/hkxedit2_20260910/out
mkdir -p "$OUT"
echo "== build release/hkxclipedit_gate.exe"
eval g++ -O1 $FLAGS -Wno-unused-parameter -DWW_HKXCLIP_CANON tests/hkxclipedit_gate.cpp src/hkxclipedit.cpp src/hkxanim.cpp src/hkxwrite.cpp src/hkxfile.cpp tests/hkxanim_shim.cpp -o release/hkxclipedit_gate.exe -LC:/msys64/ucrt64/lib -lQt6Core -lQt6Gui 2>&1 | grep -v "sfinae-incomplete\|qchar.h" | head -60
r=${PIPESTATUS[0]}
echo "BUILD-RC=$r"
ls -l --time-style=+%H:%M:%S release/hkxclipedit_gate.exe 2>/dev/null
[ $r -ne 0 ] && exit $r
echo "== run"
WW_HKCLASSDB="E:/Projects/NifskopeWildWastelandEdition/res/hkclasses_fo4.json" ./release/hkxclipedit_gate.exe "E:/Projects/NifskopeWildWastelandEdition/fixtures/Running_To_Slide_And_Back_To_Running.hkx" \
  "E:/Projects/NifskopeWildWastelandEdition/scratchpad/hkx1_20260910/clips/skeleton.hkx" \
  "E:/Projects/NifskopeWildWastelandEdition/scratchpad/hkx1_20260910/clips/jog.hkx" \
  "E:/Projects/NifskopeWildWastelandEdition/$OUT" | tee "$OUT/gate_run.txt"
g=${PIPESTATUS[0]}
echo "GATE-RC=$g"
echo "== (g) HKXPACK re-reads the saved file"
export PATH="$PATH:/c/Program Files/Eclipse Adoptium/jdk-17.0.9.9-hotspot/bin"
java -jar "/e/Tools/Fallout 4/HKXPACK/hkxpack-cli.jar" unpack "$OUT/mixamo_edited.hkx" -o "$OUT/mixamo_edited.xml" > "$OUT/hkxpack.log" 2>&1
echo "HKXPACK-RC=$?"
grep -c "hkaInterleavedUncompressedAnimation" "$OUT/mixamo_edited.xml" 2>/dev/null
grep -o 'name="transforms" numelements="[0-9]*"' "$OUT/mixamo_edited.xml" 2>/dev/null | head -1
java -jar "/e/Tools/Fallout 4/HKXPACK/hkxpack-cli.jar" unpack "$OUT/mixamo_annot.hkx" -o "$OUT/mixamo_annot.xml" >/dev/null 2>&1
n=$(grep -c '<hkparam name="text">FootLeft</hkparam>' "$OUT/mixamo_annot.xml")
echo "(d) HKXPACK sees FootLeft in the saved file: $n (need >= 1)"
[ "$n" -ge 1 ] || g=1
java -jar "/e/Tools/Fallout 4/HKXPACK/hkxpack-cli.jar" unpack "$OUT/jog_resaved.hkx" -o "$OUT/jog_resaved.xml" >/dev/null 2>&1
n2=$(grep -c '<hkparam name="text">FootLeft</hkparam>' "$OUT/jog_resaved.xml")
echo "(d) HKXPACK sees FootLeft in the resaved jog: $n2 (need >= 1)"
[ "$n2" -ge 1 ] || g=1
echo "GATE-RC-WITH-HKXPACK=$g"
exit $g
