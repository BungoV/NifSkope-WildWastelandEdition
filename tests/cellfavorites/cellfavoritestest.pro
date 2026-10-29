# FAVCAM1 unit test: the camera favorites file (write, read back) and the start pick a cell's open takes.
# Build: copy the tree's .qmake.stash beside the build folder, qmake6 <this> && make && ./cellfavoritestest <scratch dir>
# Reds: WW_CELL_FAVORITES_RED=range (the pick ignores the opened cells) and =angles (the writer drops yaw and pitch).
TEMPLATE = app
TARGET = cellfavoritestest
QT += core
CONFIG += c++20 console
CONFIG -= app_bundle

ROOT = $$PWD/../..
INCLUDEPATH += $$ROOT/src
!*msvc*:QMAKE_CXXFLAGS += -march=haswell

SOURCES += cellfavoritestest.cpp $$ROOT/src/cellfavorites.cpp $$ROOT/src/wwswitch.cpp
