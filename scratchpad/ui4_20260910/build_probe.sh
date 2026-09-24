#!/bin/bash
# Lane UI4 -- build the standalone QSS geometry probe (skill ww-qss-geometry-probe).
# Links Qt6 only; it cannot touch release/NifSkope.exe, GeneratedFiles/ or the Makefile.
set -e
cd /e/Projects/NifskopeWildWastelandEdition
g++ -std=gnu++2a -O1 -DUNICODE -D_UNICODE -DWIN32 \
    -DQT_NO_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB \
    -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtWidgets \
    -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtCore \
    scratchpad/ui4_20260910/probe.cpp -o release/ui4_probe.exe \
    -lQt6Widgets -lQt6Gui -lQt6Core
echo "probe built"
