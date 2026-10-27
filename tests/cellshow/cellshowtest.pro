# lane OPENCELLFIX1 unit test: qmake6 tests/cellshow/cellshowtest.pro && make && ./cellshowtest
TEMPLATE = app
TARGET = cellshowtest
QT += core gui widgets
CONFIG += c++20 console
CONFIG -= app_bundle

ROOT = $$PWD/../..
INCLUDEPATH += $$ROOT/src/ui $$ROOT/src

HEADERS += $$ROOT/src/ui/cellshowpanel.h $$ROOT/src/ui/widgets/wwdropdown.h
SOURCES += cellshowtest.cpp \
	stubs.cpp \
	$$ROOT/src/ui/cellshowpanel.cpp \
	$$ROOT/src/ui/widgets/wwdropdown.cpp \
	$$ROOT/src/wwswitch.cpp
