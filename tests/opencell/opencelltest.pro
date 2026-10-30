# OPENCELL1 unit test: qmake6 tests/opencell/opencelltest.pro && make && ./opencelltest
TEMPLATE = app
TARGET = opencelltest
QT += core gui widgets
CONFIG += c++20 console
CONFIG -= app_bundle

ROOT = $$PWD/../..
INCLUDEPATH += $$ROOT/src/ui $$ROOT/src $$ROOT/lib/libfo76utils/src
!*msvc*:QMAKE_CXXFLAGS += -march=haswell
# lane REDRETIRE1: the red build (qmake6 <this> REDTWIN=1) brings back the retired reds as twins (src/wwredtwin.h);
# run it with REDTWIN=OPENCELL=<value>. tests/opencell/run.sh builds and runs both.
!isEmpty(REDTWIN): DEFINES += WW_REDTWIN

HEADERS += $$ROOT/src/ui/opencellcatalog.h $$ROOT/src/ui/opencelldialog.h \
	$$ROOT/src/ui/widgets/wwdropdown.h
SOURCES += opencelltest.cpp \
	stubs.cpp \
	$$ROOT/src/ui/opencellcatalog.cpp \
	$$ROOT/src/ui/opencelldialog.cpp \
	$$ROOT/src/ui/widgets/wwdropdown.cpp \
	$$ROOT/src/wwswitch.cpp \
	$$ROOT/src/lodgenloadorder.cpp \
	$$ROOT/lib/libfo76utils/src/ba2file.cpp \
	$$ROOT/lib/libfo76utils/src/esmfile.cpp \
	$$ROOT/lib/libfo76utils/src/filebuf.cpp \
	$$ROOT/lib/libfo76utils/src/common.cpp \
	$$ROOT/lib/libfo76utils/src/zlib.cpp
