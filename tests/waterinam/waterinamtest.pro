# WATERUNDER5 unit test: qmake6 tests/waterinam/waterinamtest.pro && make && ./waterinamtest "<Fallout 4>/Data" <scratch>
# ESMDATA overrides the esmdata.cpp under test (the red builds the base commit's copy).
TEMPLATE = app
TARGET = waterinamtest
QT += core
CONFIG += c++20 console
CONFIG -= app_bundle

ROOT = $$PWD/../..
isEmpty(ESMDATA): ESMDATA = $$ROOT/src/esmdata.cpp
INCLUDEPATH += $$ROOT/src $$ROOT/lib/libfo76utils/src
!*msvc*:QMAKE_CXXFLAGS += -march=haswell

SOURCES += waterinam_test.cpp \
	$$ESMDATA \
	$$ROOT/src/wwswitch.cpp \
	$$ROOT/lib/libfo76utils/src/esmfile.cpp \
	$$ROOT/lib/libfo76utils/src/filebuf.cpp \
	$$ROOT/lib/libfo76utils/src/common.cpp \
	$$ROOT/lib/libfo76utils/src/zlib.cpp
