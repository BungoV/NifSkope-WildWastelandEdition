# lane OPENCELLFIX2 unit test: the Display groups' masters (src/gl/celllooks.cpp WwGroup) gate their rows' readers.
# Build: copy the tree's .qmake.stash beside the build folder, qmake6 <this> && make && ./cellgroupstest
TEMPLATE = app
TARGET = cellgroupstest
QT += core
CONFIG += c++20 console
CONFIG -= app_bundle

ROOT = $$PWD/../..
INCLUDEPATH += $$ROOT/src $$ROOT/src/gl
!*msvc*:QMAKE_CXXFLAGS += -march=haswell

SOURCES += cellgroupstest.cpp $$ROOT/src/gl/celllooks.cpp $$ROOT/src/wwswitch.cpp
