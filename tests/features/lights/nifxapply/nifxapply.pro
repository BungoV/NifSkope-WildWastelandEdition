# NIFXLIGHT1 unit gate: .nifx lights applied to a cell's lights (src/gl/cellnifxlights).
# qmake6 tests/features/lights/nifxapply/nifxapply.pro && make && ./nifxapply <frames dir> <nifx dir> [--red words] [--dump file]
TEMPLATE = app
TARGET = nifxapply
QT = core
CONFIG += c++20 console
CONFIG -= app_bundle

ROOT = $$PWD/../../../..
INCLUDEPATH += $$ROOT/src $$ROOT/src/gl $$ROOT/src/features/lights

HEADERS += $$ROOT/src/features/lights/cellnifxlights.h $$ROOT/src/gl/celllights.h $$ROOT/src/features/lights/nifxlights.h
SOURCES += nifxapply.cpp \
	$$ROOT/src/features/lights/cellnifxlights.cpp \
	$$ROOT/src/features/lights/nifxlights.cpp
