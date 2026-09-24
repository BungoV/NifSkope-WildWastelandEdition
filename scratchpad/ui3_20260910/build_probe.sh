#!/bin/sh
# UI3: build the standalone bar-button probe. Links Qt6Widgets only; it does
# NOT touch release/NifSkope.exe or any object in GeneratedFiles/.
set -e
cd /e/Projects/NifskopeWildWastelandEdition
g++ -std=gnu++2a -O1 -DUNICODE -D_UNICODE -DWIN32 \
    -DQT_NO_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB \
    -IC:/msys64/ucrt64/include/qt6 \
    -IC:/msys64/ucrt64/include/qt6/QtWidgets \
    -IC:/msys64/ucrt64/include/qt6/QtGui \
    -IC:/msys64/ucrt64/include/qt6/QtCore \
    scratchpad/ui3_20260910/probe.cpp \
    -o release/ui3_probe.exe \
    -lQt6Widgets -lQt6Gui -lQt6Core
echo "PROBE-BUILD-RC=$?"
ls -l --time-style=+%H:%M:%S release/ui3_probe.exe
