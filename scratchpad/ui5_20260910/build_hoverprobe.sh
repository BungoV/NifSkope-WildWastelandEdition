#!/bin/sh
# Lane UI5-HOVERPIC -- the ww-qss-geometry-probe rig for the HOVERED menu title.
# Links Qt only: it cannot touch release/NifSkope.exe, GeneratedFiles/ or the
# Makefile, so it is safe while lane UI6 holds the build slot and the one allowed
# NifSkope instance. Run from the repo root in an MSYS2 UCRT64 shell.
set -e
g++ -std=gnu++2a -O1 -DUNICODE -D_UNICODE -DWIN32 \
    -DQT_NO_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB \
    -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtWidgets \
    -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtCore \
    scratchpad/ui5_20260910/hoverprobe.cpp -o release/ui5_hoverprobe.exe \
    -lQt6Widgets -lQt6Gui -lQt6Core
echo "built release/ui5_hoverprobe.exe"
