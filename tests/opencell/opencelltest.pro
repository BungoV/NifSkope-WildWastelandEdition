# OPENCELL1 unit test: qmake6 tests/opencell/opencelltest.pro && make && ./opencelltest
TEMPLATE = app
TARGET = opencelltest
QT += core gui widgets
CONFIG += c++20 console
CONFIG -= app_bundle

ROOT = $$PWD/../..
INCLUDEPATH += $$ROOT/src/ui $$ROOT/src $$ROOT/lib/libfo76utils/src
!*msvc*:QMAKE_CXXFLAGS += -march=haswell

HEADERS += $$ROOT/src/ui/opencellcatalog.h $$ROOT/src/ui/opencelldialog.h
SOURCES += opencelltest.cpp \
	$$ROOT/src/ui/opencellcatalog.cpp \
	$$ROOT/src/ui/opencelldialog.cpp \
	$$ROOT/src/wwswitch.cpp \
	$$ROOT/lib/libfo76utils/src/ba2file.cpp \
	$$ROOT/lib/libfo76utils/src/esmfile.cpp \
	$$ROOT/lib/libfo76utils/src/filebuf.cpp \
	$$ROOT/lib/libfo76utils/src/common.cpp \
	$$ROOT/lib/libfo76utils/src/zlib.cpp
