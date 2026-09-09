export PATH=/ucrt64/bin:$PATH
set -e
cd /e/Projects/NifskopeWildWastelandEdition
DEFINES='-DUNICODE -D_UNICODE -DWIN32 -DMINGW_HAS_SECURE_API=1 -DQT_DISABLE_DEPRECATED_BEFORE=0x060400 -DQT_NO_DEBUG_OUTPUT -D_USE_MATH_DEFINES -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING -DNIFSKOPE_VERSION="2.0.dev11" -DNIFSKOPE_REVISION="2dd8444" -DWW_EDITION_VERSION="0.3.3" -DEDIT_ON_ACTIVATE -DQT_NO_DEBUG -DQT_OPENGLWIDGETS_LIB -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_XML_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB -DQT_NEEDS_QMAIN'
INCPATH='-I. -Isrc -Ilib -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtOpenGLWidgets -IC:/msys64/ucrt64/include/qt6/QtOpenGL -IC:/msys64/ucrt64/include/qt6/QtWidgets -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtXml -IC:/msys64/ucrt64/include/qt6/QtNetwork -IC:/msys64/ucrt64/include/qt6/QtCore -IGeneratedFiles/.moc -IGeneratedFiles/.ui -I/include -IC:/msys64/ucrt64/share/qt6/mkspecs/win32-g++'
g++ -fsyntax-only -fno-keep-inline-dllexport -Ilib/qhull/src -isystem lib/gli/gli -isystem lib/gli/external -Ilib/libfo76utils/src -std=gnu++2a -Wall -Wextra -fexceptions $DEFINES $INCPATH src/nifskope_ui.cpp src/nifskope.cpp
echo "RC=$?"
