#!/bin/sh
# Lane UI5 -- the ww-qss-geometry-probe rig. Links Qt only: it cannot touch
# release/NifSkope.exe, GeneratedFiles/ or the Makefile, so it is safe while a
# build lane holds the slot. Run from the repo root in an MSYS2 UCRT64 shell.
set -e
g++ -std=gnu++2a -O1 -DUNICODE -D_UNICODE -DWIN32 \
    -DQT_NO_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB \
    -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtWidgets \
    -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtCore \
    scratchpad/ui5_20260910/probe.cpp -o release/ui5_probe.exe \
    -lQt6Widgets -lQt6Gui -lQt6Core
echo "built release/ui5_probe.exe"
