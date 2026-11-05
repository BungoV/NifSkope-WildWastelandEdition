# NIFXFIX2 unit gate: Memory Den mesh-light reach and the dimmer of a mesh's only NiLight (src/gl/cellnifxlights).
# qmake6 tests/features/lights/nifxfix/nifxfix.pro && make && ./nifxfix <frames dir> <nifx dir> [--red words]
# SRC= another tree's src folder (the red against origin/main): qmake6 SRC=<tree>/src tests/features/lights/nifxfix/nifxfix.pro
TEMPLATE = app
TARGET = nifxfix
QT = core
CONFIG += c++20 console
CONFIG -= app_bundle

isEmpty(SRC): SRC = $$PWD/../../../../src
INCLUDEPATH += $$SRC $$SRC/gl $$SRC/features/lights

HEADERS += $$SRC/features/lights/cellnifxlights.h $$SRC/gl/celllights.h $$SRC/features/lights/nifxlights.h
SOURCES += nifxfix.cpp \
	$$SRC/features/lights/cellnifxlights.cpp \
	$$SRC/features/lights/nifxlights.cpp
