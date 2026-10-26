# NIFXLIGHT1 unit gate: .nifx lights applied to a cell's lights (src/gl/cellnifxlights).
# qmake6 tests/nifxapply/nifxapply.pro && make && ./nifxapply <frames dir> <nifx dir> [--red words] [--dump file]
TEMPLATE = app
TARGET = nifxapply
QT = core
CONFIG += c++20 console
CONFIG -= app_bundle

ROOT = $$PWD/../..
INCLUDEPATH += $$ROOT/src $$ROOT/src/gl

HEADERS += $$ROOT/src/gl/cellnifxlights.h $$ROOT/src/gl/celllights.h $$ROOT/src/io/nifxlights.h
SOURCES += nifxapply.cpp \
	$$ROOT/src/gl/cellnifxlights.cpp \
	$$ROOT/src/io/nifxlights.cpp
